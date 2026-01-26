#include <Arduino.h>

// ================== USER CONFIG ==================
#define IR_PIN        9
#define IR_FREQ       38000
#define IR_RES        8
#define IR_DUTY       85      // ~33%

#define BURST_ON_MS   2
#define BURST_OFF_MS  2

#define BURSTS_PER_PACKET 10
#define FRAME_GAP_MS       20
// ================================================

bool irOn = false;
uint32_t lastToggle = 0;
uint8_t burstCount = 0;
bool inFrameGap = false;

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

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

  setRGB(0, 0, 40); // booting
  pwmStart();

  irEnable(true);
  lastToggle = millis();
  burstCount = 0;
  inFrameGap = false;

  setRGB(0, 40, 0); // transmitting
}

void loop() {
  uint32_t now = millis();

  if (inFrameGap) {
    if (now - lastToggle >= FRAME_GAP_MS) {
      inFrameGap = false;
      burstCount = 0;
      irEnable(true);
      lastToggle = now;
    }
    return;
  }

  if (irOn) {
    if (now - lastToggle >= BURST_ON_MS) {
      irEnable(false);
      lastToggle = now;
    }
  } else {
    if (now - lastToggle >= BURST_OFF_MS) {
      burstCount++;

      if (burstCount >= BURSTS_PER_PACKET) {
        // enter frame gap
        inFrameGap = true;
        irEnable(false);
        lastToggle = now;
      } else {
        irEnable(true);
        lastToggle = now;
      }
    }
  }
}
