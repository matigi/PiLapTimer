#include <Arduino.h>

// Burst-gated 38 kHz IR beacon. Do NOT use continuous illumination.
// Continuous carrier can saturate IR receivers and reduce reliability.

// ================== USER CONFIG ==================
#define IR_PIN        9
#define IR_FREQ       38000
#define IR_RES        8
#define IR_DUTY       85      // ~33% duty on carrier

// Envelope timings (microseconds)
#define BURST_ON_US     2000  // 2.0 ms ON
#define BURST_OFF_US    2000  // 2.0 ms OFF

#define BURSTS_PER_PACKET  10
#define FRAME_GAP_US     20000 // 20 ms gap between packets
// ================================================

static bool irOn = false;
static uint32_t lastToggleUs = 0;
static uint8_t burstCount = 0;
static bool inFrameGap = false;

#if defined(RGB_BUILTIN)
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}
#else
void setRGB(uint8_t, uint8_t, uint8_t) {}
#endif

void pwmStart() {
  ledcAttach(IR_PIN, IR_FREQ, IR_RES);
  ledcWrite(IR_PIN, 0);
}

inline void irEnable(bool enable) {
  irOn = enable;
  ledcWrite(IR_PIN, enable ? IR_DUTY : 0);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  setRGB(0, 0, 40); // boot
  pwmStart();

  burstCount = 0;
  inFrameGap = false;

  irEnable(true);
  lastToggleUs = micros();

  setRGB(0, 40, 0); // transmitting
}

void loop() {
  uint32_t nowUs = micros();

  if (inFrameGap) {
    if ((uint32_t)(nowUs - lastToggleUs) >= FRAME_GAP_US) {
      inFrameGap = false;
      burstCount = 0;
      irEnable(true);
      lastToggleUs = nowUs;
    }
    return;
  }

  if (irOn) {
    if ((uint32_t)(nowUs - lastToggleUs) >= BURST_ON_US) {
      irEnable(false);
      lastToggleUs = nowUs;
    }
  } else {
    if ((uint32_t)(nowUs - lastToggleUs) >= BURST_OFF_US) {
      burstCount++;

      if (burstCount >= BURSTS_PER_PACKET) {
        // enter frame gap
        inFrameGap = true;
        irEnable(false);
        lastToggleUs = nowUs;
      } else {
        irEnable(true);
        lastToggleUs = nowUs;
      }
    }
  }
}
