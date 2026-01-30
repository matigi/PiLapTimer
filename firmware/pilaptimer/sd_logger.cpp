#include "sd_logger.h"

#include <SD.h>
#include <SPI.h>

#ifndef SD_CS_PIN
#define SD_CS_PIN 23
#endif

#ifndef SD_LOG_BUFFER_LINES
#define SD_LOG_BUFFER_LINES 48
#endif

#ifndef SD_LOG_LINE_MAX
#define SD_LOG_LINE_MAX 200
#endif

#ifndef SD_LOG_FLUSH_THRESHOLD
#define SD_LOG_FLUSH_THRESHOLD 12
#endif

#ifndef SD_LOG_FLUSH_INTERVAL_MS
#define SD_LOG_FLUSH_INTERVAL_MS 400
#endif

#ifndef SD_LOG_SUMMARY_INTERVAL_MS
#define SD_LOG_SUMMARY_INTERVAL_MS 2000
#endif

#ifndef LOG_USE_RTC_DATETIME
#define LOG_USE_RTC_DATETIME 0
#endif

#ifndef LOG_USE_USER_SET_DATETIME
#define LOG_USE_USER_SET_DATETIME 0
#endif

#ifndef SD_LOG_DEBUG
#define SD_LOG_DEBUG 0
#endif

namespace {

static const char* kBaseDir = "/PILAPTIMER";
static const char* kSessionsDir = "/PILAPTIMER/SESSIONS";
static const uint8_t kMaxDrivers = 10;

enum LogType : uint8_t {
  LOG_TYPE_LAP = 0,
  LOG_TYPE_RT = 1,
  LOG_TYPE_ALL = 2,
};

struct LogEntry {
  LogType type;
  char line[SD_LOG_LINE_MAX];
};

static LogEntry gLogBuffer[SD_LOG_BUFFER_LINES];
static uint8_t gLogHead = 0;
static uint8_t gLogTail = 0;
static uint8_t gLogCount = 0;
static uint32_t gDroppedLines = 0;

static bool gReady = false;
static uint32_t gSessionId = 0;
static uint32_t gSessionStartMs = 0;
static char gSessionStartDatetime[32] = "--";

static char gSessionPath[64] = "";
static char gLapsPath[96] = "";
static char gReactionPath[96] = "";
static char gAllEventsPath[96] = "";
static char gSummaryPath[96] = "";

static uint16_t gDriverLapCounts[kMaxDrivers] = {};
static uint32_t gDriverBestLapMs[kMaxDrivers] = {};
static uint16_t gDriverBestLapIndex[kMaxDrivers] = {};
static uint32_t gDriverBestRtMs[kMaxDrivers] = {};

static uint32_t gLastFlushMs = 0;
static uint32_t gLastSummaryMs = 0;

#if LOG_USE_RTC_DATETIME
extern bool sd_logger_get_rtc_datetime(char* out, size_t len) __attribute__((weak));
#endif
#if LOG_USE_USER_SET_DATETIME
extern bool sd_logger_get_user_datetime(char* out, size_t len) __attribute__((weak));
#endif

static void DebugPrint(const char* message) {
#if SD_LOG_DEBUG
  Serial.println(message);
#else
  (void)message;
#endif
}

static bool EnsureDir(const char* path) {
  if (SD.exists(path)) return true;
  return SD.mkdir(path);
}

static uint32_t FindNextSessionId() {
  uint32_t maxId = 0;
  File dir = SD.open(kSessionsDir);
  if (!dir) return 1;
  File entry = dir.openNextFile();
  while (entry) {
    const char* name = entry.name();
    if (name && name[0] == 'S') {
      uint32_t id = strtoul(name + 1, nullptr, 10);
      if (id > maxId) maxId = id;
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  return maxId + 1;
}

static void GetDatetime(char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  bool got = false;
#if LOG_USE_RTC_DATETIME
  if (sd_logger_get_rtc_datetime) {
    got = sd_logger_get_rtc_datetime(out, outSize);
  }
#endif
#if LOG_USE_USER_SET_DATETIME
  if (!got && sd_logger_get_user_datetime) {
    got = sd_logger_get_user_datetime(out, outSize);
  }
#endif
  if (!got) {
    snprintf(out, outSize, "--");
  }
}

static bool EnqueueLog(LogType type, const char* line) {
  if (gLogCount >= SD_LOG_BUFFER_LINES) {
    gDroppedLines++;
#if SD_LOG_DEBUG
    Serial.printf("SD log buffer full, dropped=%lu\n", (unsigned long)gDroppedLines);
#endif
    return false;
  }
  LogEntry& entry = gLogBuffer[gLogHead];
  entry.type = type;
  strncpy(entry.line, line, sizeof(entry.line) - 1);
  entry.line[sizeof(entry.line) - 1] = '\0';
  gLogHead = (uint8_t)((gLogHead + 1) % SD_LOG_BUFFER_LINES);
  gLogCount++;
  return true;
}

static bool EnsureFileHeader(const char* path, const char* header) {
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  if (file.size() == 0) {
    file.println(header);
  }
  file.close();
  return true;
}

static void ResetSummaryStats() {
  for (uint8_t i = 0; i < kMaxDrivers; ++i) {
    gDriverLapCounts[i] = 0;
    gDriverBestLapMs[i] = 0;
    gDriverBestLapIndex[i] = 0;
    gDriverBestRtMs[i] = 0;
  }
  gDroppedLines = 0;
}

static void WriteSummary(uint32_t now) {
  if (!gReady || gSessionId == 0) return;

  SD.remove(gSummaryPath);
  File file = SD.open(gSummaryPath, FILE_WRITE);
  if (!file) return;

  file.println("PiLapTimer Session Summary");
  file.printf("Session ID: %lu\n", (unsigned long)gSessionId);
  file.printf("Start uptime_ms: %lu\n", (unsigned long)gSessionStartMs);
  file.printf("Start datetime: %s\n", gSessionStartDatetime);
  file.println("----------------------------------------");

  for (uint8_t i = 0; i < kMaxDrivers; ++i) {
    file.printf("Driver %u:\n", (unsigned)(i + 1));
    file.printf("  Laps recorded: %u\n", (unsigned)gDriverLapCounts[i]);
    if (gDriverBestLapMs[i] > 0) {
      file.printf("  Best lap: %lu ms", (unsigned long)gDriverBestLapMs[i]);
      if (gDriverBestLapIndex[i] > 0) {
        file.printf(" (lap %u)", (unsigned)gDriverBestLapIndex[i]);
      }
      file.println();
    } else {
      file.println("  Best lap: --");
    }
    if (gDriverBestRtMs[i] > 0) {
      file.printf("  Best RT: %lu ms\n", (unsigned long)gDriverBestRtMs[i]);
    } else {
      file.println("  Best RT: --");
    }
  }

  file.println("----------------------------------------");
  file.printf("Dropped log lines: %lu\n", (unsigned long)gDroppedLines);

  char nowDatetime[32];
  GetDatetime(nowDatetime, sizeof(nowDatetime));
  file.printf("Last updated uptime_ms: %lu\n", (unsigned long)now);
  file.printf("Last updated datetime: %s\n", nowDatetime);
  file.close();
}

static bool FlushLogs() {
  if (gLogCount == 0) return false;

  File lapsFile;
  File reactionFile;
  File allFile;
  bool openedLaps = false;
  bool openedReaction = false;
  bool openedAll = false;
  bool wroteAny = false;

  while (gLogCount > 0) {
    LogEntry& entry = gLogBuffer[gLogTail];
    File* target = nullptr;
    switch (entry.type) {
      case LOG_TYPE_LAP:
        if (!openedLaps) {
          lapsFile = SD.open(gLapsPath, FILE_WRITE);
          openedLaps = lapsFile;
        }
        target = openedLaps ? &lapsFile : nullptr;
        break;
      case LOG_TYPE_RT:
        if (!openedReaction) {
          reactionFile = SD.open(gReactionPath, FILE_WRITE);
          openedReaction = reactionFile;
        }
        target = openedReaction ? &reactionFile : nullptr;
        break;
      case LOG_TYPE_ALL:
      default:
        if (!openedAll) {
          allFile = SD.open(gAllEventsPath, FILE_WRITE);
          openedAll = allFile;
        }
        target = openedAll ? &allFile : nullptr;
        break;
    }

    if (!target) {
#if SD_LOG_DEBUG
      Serial.println("SD log flush aborted (file open failed)");
#endif
      break;
    }

    target->println(entry.line);
    gLogTail = (uint8_t)((gLogTail + 1) % SD_LOG_BUFFER_LINES);
    gLogCount--;
    wroteAny = true;
  }

  if (openedLaps) lapsFile.close();
  if (openedReaction) reactionFile.close();
  if (openedAll) allFile.close();

  return wroteAny;
}

}  // namespace

bool sd_logger_init() {
  if (gReady) return true;

  Serial.println("SD: init start");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.printf("SD: init failed (CS pin %u)\n", (unsigned)SD_CS_PIN);
    gReady = false;
    return false;
  }

  if (!EnsureDir(kBaseDir)) {
    Serial.println("SD: mkdir failed (base dir)");
    gReady = false;
    return false;
  }
  if (!EnsureDir(kSessionsDir)) {
    Serial.println("SD: mkdir failed (sessions dir)");
    gReady = false;
    return false;
  }

  gReady = true;
  Serial.println("SD: init OK");
  return true;
}

bool sd_logger_is_ready() {
  return gReady;
}

void sd_logger_start_new_session() {
  if (!gReady) return;

  if (!EnsureDir(kBaseDir) || !EnsureDir(kSessionsDir)) {
    DebugPrint("SD session dirs missing");
    return;
  }

  gSessionId = FindNextSessionId();
  gSessionStartMs = millis();
  GetDatetime(gSessionStartDatetime, sizeof(gSessionStartDatetime));

  snprintf(gSessionPath, sizeof(gSessionPath), "%s/S%lu", kSessionsDir, (unsigned long)gSessionId);
  if (!EnsureDir(gSessionPath)) {
    DebugPrint("SD session mkdir failed");
    return;
  }

  snprintf(gLapsPath, sizeof(gLapsPath), "%s/laps.csv", gSessionPath);
  snprintf(gReactionPath, sizeof(gReactionPath), "%s/reaction.csv", gSessionPath);
  snprintf(gAllEventsPath, sizeof(gAllEventsPath), "%s/all_events.csv", gSessionPath);
  snprintf(gSummaryPath, sizeof(gSummaryPath), "%s/summary.txt", gSessionPath);

  EnsureFileHeader(gLapsPath,
                   "session_id,uptime_ms,datetime,driver,lap_index,lap_time_ms,best_lap_time_ms,target_laps,mode");
  EnsureFileHeader(gReactionPath,
                   "session_id,uptime_ms,datetime,driver,reaction_time_ms,best_reaction_time_ms,mode");
  EnsureFileHeader(gAllEventsPath,
                   "session_id,uptime_ms,datetime,event_type,driver,value_ms,extra");

  ResetSummaryStats();
  WriteSummary(gSessionStartMs);

#if SD_LOG_DEBUG
  Serial.printf("SD session started: %s\n", gSessionPath);
#endif
}

uint32_t sd_logger_session_id() {
  return gSessionId;
}

void sd_logger_log_lap(uint8_t driver,
                       uint16_t lap_index,
                       uint32_t lap_time_ms,
                       uint32_t best_lap_ms,
                       uint16_t target_laps) {
  if (!gReady || gSessionId == 0) return;

  uint32_t uptime = millis();
  char datetime[32];
  GetDatetime(datetime, sizeof(datetime));

  char line[SD_LOG_LINE_MAX];
  snprintf(line,
           sizeof(line),
           "%lu,%lu,%s,%u,%u,%lu,%lu,%u,LAP",
           (unsigned long)gSessionId,
           (unsigned long)uptime,
           datetime,
           (unsigned)driver,
           (unsigned)lap_index,
           (unsigned long)lap_time_ms,
           (unsigned long)best_lap_ms,
           (unsigned)target_laps);
  EnqueueLog(LOG_TYPE_LAP, line);

  snprintf(line,
           sizeof(line),
           "%lu,%lu,%s,LAP,%u,%lu,lap_index=%u;best_lap_ms=%lu;target_laps=%u",
           (unsigned long)gSessionId,
           (unsigned long)uptime,
           datetime,
           (unsigned)driver,
           (unsigned long)lap_time_ms,
           (unsigned)lap_index,
           (unsigned long)best_lap_ms,
           (unsigned)target_laps);
  EnqueueLog(LOG_TYPE_ALL, line);

  if (driver >= 1 && driver <= kMaxDrivers) {
    uint8_t idx = (uint8_t)(driver - 1);
    gDriverLapCounts[idx]++;
    if (gDriverBestLapMs[idx] == 0 || lap_time_ms < gDriverBestLapMs[idx]) {
      gDriverBestLapMs[idx] = lap_time_ms;
      gDriverBestLapIndex[idx] = lap_index;
    }
  }
}

void sd_logger_log_rt(uint8_t driver,
                      uint32_t reaction_time_ms,
                      uint32_t best_rt_ms) {
  if (!gReady || gSessionId == 0) return;

  uint32_t uptime = millis();
  char datetime[32];
  GetDatetime(datetime, sizeof(datetime));

  char line[SD_LOG_LINE_MAX];
  snprintf(line,
           sizeof(line),
           "%lu,%lu,%s,%u,%lu,%lu,RT",
           (unsigned long)gSessionId,
           (unsigned long)uptime,
           datetime,
           (unsigned)driver,
           (unsigned long)reaction_time_ms,
           (unsigned long)best_rt_ms);
  EnqueueLog(LOG_TYPE_RT, line);

  snprintf(line,
           sizeof(line),
           "%lu,%lu,%s,RT,%u,%lu,best_rt_ms=%lu",
           (unsigned long)gSessionId,
           (unsigned long)uptime,
           datetime,
           (unsigned)driver,
           (unsigned long)reaction_time_ms,
           (unsigned long)best_rt_ms);
  EnqueueLog(LOG_TYPE_ALL, line);

  if (driver >= 1 && driver <= kMaxDrivers) {
    uint8_t idx = (uint8_t)(driver - 1);
    if (gDriverBestRtMs[idx] == 0 || reaction_time_ms < gDriverBestRtMs[idx]) {
      gDriverBestRtMs[idx] = reaction_time_ms;
    }
  }
}

void sd_logger_loop() {
  if (!gReady || gSessionId == 0) return;

  uint32_t now = millis();
  bool shouldFlush = (gLogCount >= SD_LOG_FLUSH_THRESHOLD) ||
                     (gLogCount > 0 && (uint32_t)(now - gLastFlushMs) >= SD_LOG_FLUSH_INTERVAL_MS);

  bool flushed = false;
  if (shouldFlush) {
    flushed = FlushLogs();
    if (flushed) {
      gLastFlushMs = now;
    }
  }

  bool summaryDue = (uint32_t)(now - gLastSummaryMs) >= SD_LOG_SUMMARY_INTERVAL_MS;
  bool summaryAfterFlush = flushed && (uint32_t)(now - gLastSummaryMs) >= 250;
  if (summaryDue || summaryAfterFlush) {
    WriteSummary(now);
    gLastSummaryMs = now;
  }
}
