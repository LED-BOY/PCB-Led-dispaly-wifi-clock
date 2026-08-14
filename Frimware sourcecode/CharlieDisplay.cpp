#include "CharlieDisplay.h"

CharlieDisplay display;

// =========================
// Gamma lookup table
// 2.2 gamma, output 0..64 PWM slots
// =========================
static const uint8_t gammaTable[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
  5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,
  7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10,
  10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14,
  14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18,
  18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 23,
  23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28,
  28, 29, 29, 29, 30, 30, 30, 31, 31, 32, 32, 32, 33, 33, 34, 34,
  34, 35, 35, 35, 36, 36, 37, 37, 38, 38, 38, 39, 39, 40, 40, 40,
  41, 41, 42, 42, 43, 43, 44, 44, 44, 45, 45, 46, 46, 47, 47, 48,
  48, 49, 49, 50, 50, 51, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55,
  56, 57, 57, 58, 58, 59, 59, 60, 60, 61, 61, 62, 62, 63, 63, 64
};

// A digit LED is selected once per activeTable cycle. The colon is available
// on every ISR pass, so reduce its duty by activeCount to match one digit LED.
static inline uint8_t colonDutyForLevel(uint8_t level, uint8_t activeCount) {
  if (activeCount == 0) return 0;

  const uint8_t duty = gammaTable[level];
  return (duty + activeCount / 2) / activeCount;  // nearest available PWM step
}

// =========================
// GPIO
// =========================
static inline void fastHigh(int pin) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)pin, 1);
}

static inline void fastLow(int pin) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)pin, 0);
}

static inline void fastHiZ(int pin) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
}

// =========================
// Release the previously driven LED pair.
// =========================
static inline void IRAM_ATTR turnOff(volatile int &hi, volatile int &lo) {
  if (hi >= 0) fastHiZ(hi);
  if (lo >= 0) fastHiZ(lo);
  hi = -1;
  lo = -1;
}

// =========================
// ISR
// =========================
void IRAM_ATTR onTimer() {

  portENTER_CRITICAL_ISR(&display.timerMux);

  // =====================================
  // Colon PWM (independent)
  // =====================================
  uint8_t colonPwm = display.colonPwmCounter + display.colonBrightness;
  bool colonPulse = colonPwm >= 64;
  display.colonPwmCounter = colonPulse ? colonPwm - 64 : colonPwm;

  // Spread the ON pulses across the PWM cycle instead of grouping them
  // together. This removes the visible 156 Hz flash at low brightness.
  if (display.colonBlink && ++display.colonBlinkTicks >= 25000) {
    display.colonBlinkTicks = 0;
  }
  bool colonVisible = !display.colonBlink || display.colonBlinkTicks < 12500;

  if (display.colonEnabled && display.colonState && colonVisible && colonPulse) {

    fastLow(COLON_LOW_PIN);
    fastHigh(COLON_HIGH_PIN);

  } else {

    fastHiZ(COLON_LOW_PIN);
    fastHiZ(COLON_HIGH_PIN);
  }

  // =====================================
  // Display PWM
  // =====================================
  turnOff(display.prevHi, display.prevLo);

  uint8_t displayPwm = display.frame + display.effectiveDuty;
  bool displayPulse = displayPwm >= 64;
  display.frame = displayPulse ? displayPwm - 64 : displayPwm;

  // Pulse-density modulation keeps the same average brightness while
  // avoiding a long OFF interval that makes all digits visibly blink.
  if (!displayPulse) {
    portEXIT_CRITICAL_ISR(&display.timerMux);
    return;
  }

  asm volatile(
    "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");

  // Nothing to display
  if (display.activeCount == 0) {
    portEXIT_CRITICAL_ISR(&display.timerMux);
    return;
  }

  StepEntry step = display.activeTable[display.isrStep];

  if (display.font[display.charBuffer[step.digit]][step.segment]) {

    auto l = display.map[step.digit][step.segment];

    fastHigh(l.hi);
    fastLow(l.lo);

    display.prevHi = l.hi;
    display.prevLo = l.lo;
  }

  display.isrStep++;

  if (display.isrStep >= display.activeCount) {
    display.isrStep = 0;
  }

  portEXIT_CRITICAL_ISR(&display.timerMux);
}

// -------------------------------------------------
CharlieDisplay::CharlieDisplay() {

  map[0][0] = { 1, 3 };
  map[0][1] = { 1, 4 };
  map[0][2] = { 1, 5 };
  map[0][3] = { 1, 6 };
  map[0][4] = { 1, 7 };
  map[0][5] = { 3, 1 };
  map[0][6] = { 5, 1 };

  map[1][0] = { 3, 4 };
  map[1][1] = { 3, 5 };
  map[1][2] = { 3, 6 };
  map[1][3] = { 3, 7 };
  map[1][4] = { 4, 3 };
  map[1][5] = { 5, 3 };
  map[1][6] = { 6, 3 };

  map[2][0] = { 4, 5 };
  map[2][1] = { 4, 6 };
  map[2][2] = { 4, 7 };
  map[2][3] = { 5, 4 };
  map[2][4] = { 6, 4 };
  map[2][5] = { 7, 4 };
  map[2][6] = { 5, 6 };

  map[3][0] = { 5, 7 };
  map[3][1] = { 6, 5 };
  map[3][2] = { 7, 5 };
  map[3][3] = { 6, 7 };
  map[3][4] = { 7, 6 };
  map[3][5] = { 6, 1 };
  map[3][6] = { 4, 1 };

  brightness = 120;
  effectiveDuty = 120;
  colonBrightnessLevel = 255;
  colonBrightness = 63;  // full brightness until the first display update
  colonPwmCounter = 0;

  activeCount = 0;
  isrStep = 0;

  prevHi = -1;
  prevLo = -1;

  colonEnabled = true;
  colonState = true;
  colonBlink = false;
  colonBlinkTicks = 0;

  for (int i = 0; i < DIGITS; i++) {
    charBuffer[i] = 0;
  }
}

void CharlieDisplay::setColonBrightness(uint8_t level) {
  portENTER_CRITICAL(&timerMux);
  colonBrightnessLevel = level;
  colonBrightness = colonDutyForLevel(level, activeCount);
  portEXIT_CRITICAL(&timerMux);
}

// -------------------------------------------------
void CharlieDisplay::begin() {

  for (int p = 2; p <= 7; p++) {
    fastHiZ(p);
  }

  fastHiZ(COLON_LOW_PIN);
  fastHiZ(COLON_HIGH_PIN);

  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);

  timerAlarm(timer, 40, true, 0);  // trigger every 40 us
}

// -------------------------------------------------
void CharlieDisplay::setBrightness(uint8_t level) {

  portENTER_CRITICAL(&timerMux);

  brightness = level;
  effectiveDuty = gammaTable[level];
  colonBrightness = colonDutyForLevel(colonBrightnessLevel, activeCount);

  portEXIT_CRITICAL(&timerMux);
}

// -------------------------------------------------
void CharlieDisplay::update(struct tm *t) {

  portENTER_CRITICAL(&timerMux);

  int h = t->tm_hour;
  int m = t->tm_min;

  if (h == 0) {
    charBuffer[0] = -1;
    charBuffer[1] = charToIndex('0');
  } else if (h < 10) {
    charBuffer[0] = -1;
    charBuffer[1] = charToIndex('0' + h);
  } else {
    charBuffer[0] = charToIndex('0' + (h / 10));
    charBuffer[1] = charToIndex('0' + (h % 10));
  }

  charBuffer[2] = charToIndex('0' + (m / 10));
  charBuffer[3] = charToIndex('0' + (m % 10));

  uint8_t idx = 0;

  for (int d = 0; d < DIGITS; d++) {

    if ((int8_t)charBuffer[d] < 0) continue;

    for (int s = 0; s < SEGMENTS; s++) {

      if (font[charBuffer[d]][s]) {

        if (idx < MAX_ACTIVE) {
          activeTable[idx++] = { (uint8_t)d, (uint8_t)s };
        }
      }
    }
  }

  activeCount = idx;

  effectiveDuty = gammaTable[brightness];
  colonBrightness = colonDutyForLevel(colonBrightnessLevel, activeCount);

  isrStep = 0;

  portEXIT_CRITICAL(&timerMux);
}

// -------------------------------------------------
void CharlieDisplay::print(const char *str) {

  portENTER_CRITICAL(&timerMux);

  for (int i = 0; i < DIGITS; i++) {
    charBuffer[i] = 255;
  }

  for (int i = 0; i < DIGITS; i++) {
    if (str[i] == '\0') break;
    charBuffer[i] = charToIndex(str[i]);
  }

  uint8_t idx = 0;

  for (int d = 0; d < DIGITS; d++) {

    if (charBuffer[d] == 255) continue;

    for (int s = 0; s < SEGMENTS; s++) {

      if (font[charBuffer[d]][s]) {

        if (idx < MAX_ACTIVE) {
          activeTable[idx++] = { (uint8_t)d, (uint8_t)s };
        }
      }
    }
  }

  activeCount = idx;

  effectiveDuty = gammaTable[brightness];
  colonBrightness = colonDutyForLevel(colonBrightnessLevel, activeCount);

  isrStep = 0;

  portEXIT_CRITICAL(&timerMux);
}

// -------------------------------------------------
void CharlieDisplay::setColon(bool on) {

  portENTER_CRITICAL(&timerMux);

  colonEnabled = on;
  colonState = on;

  if (!on) {
    fastHiZ(COLON_LOW_PIN);
    fastHiZ(COLON_HIGH_PIN);
  }

  portEXIT_CRITICAL(&timerMux);
}

void CharlieDisplay::setColonBlink(bool enable) {

  portENTER_CRITICAL(&timerMux);
  colonBlink = enable;
  colonBlinkTicks = 0;
  portEXIT_CRITICAL(&timerMux);
}

// -------------------------------------------------
uint8_t CharlieDisplay::charToIndex(char c) {

  uint8_t uc = (uint8_t)c;

  if (uc == 36) return 36;

  if (c >= '0' && c <= '9') return c - '0';

  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;

  if (c == '-') return 37;
  if (c == ' ') return 38;

  return 38;
}
