#pragma once

#include <Arduino.h>
#include <driver/gpio.h>
#include <deafultValues.h>

// =========================
// CONFIG
// =========================
#define DIGITS 4
#define SEGMENTS 7
#define MAX_ACTIVE 28

#define COLON_LOW_PIN 20
#define COLON_HIGH_PIN 21

struct LedPair {
  uint8_t hi;
  uint8_t lo;
};

struct StepEntry {
  uint8_t digit;
  uint8_t segment;
};

class CharlieDisplay {

public:
  CharlieDisplay();

  void begin();
  void update(struct tm* t);
  void print(const char* str);

  void setColon(bool on);
  void setColonBlink(bool enable);
  void setColonBrightness(uint8_t level);

  // Sets gamma-corrected brightness using pulse-density modulation.
  void setBrightness(uint8_t level);

  friend void IRAM_ATTR onTimer();

private:

  // =========================
  // LED pin-pair mapping
  // =========================
  LedPair map[DIGITS][SEGMENTS];

  // =========================
  // Seven-segment font
  // =========================
  const uint8_t font[39][7] = {
    // 0–9
    { 1, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 0, 1, 1, 0, 1 },
    { 1, 1, 1, 1, 0, 0, 1 },
    { 0, 1, 1, 0, 0, 1, 1 },
    { 1, 0, 1, 1, 0, 1, 1 },
    { 0, 0, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 0, 0, 1, 1 },

    // A–Z
    { 1, 1, 1, 0, 1, 1, 1 },
    { 0, 0, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 0, 1 },
    { 1, 0, 0, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 1, 1, 1 },
    { 1, 0, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 1, 1 },
    { 0, 0, 0, 1, 1, 1, 0 },
    { 1, 1, 1, 0, 1, 1, 0 },
    { 0, 0, 1, 0, 1, 0, 1 },
    { 0, 0, 1, 1, 1, 0, 1 },
    { 1, 1, 0, 0, 1, 1, 1 },
    { 1, 1, 1, 0, 0, 1, 1 },
    { 0, 0, 0, 0, 1, 0, 1 },
    { 1, 0, 1, 1, 0, 1, 1 },
    { 0, 0, 0, 1, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 0, 1, 1 },
    { 1, 1, 0, 1, 1, 0, 1 },

    // 36: custom degree glyph
    { 1, 1, 0, 0, 0, 1, 1 },

    // 37 → '-'
    { 0, 0, 0, 0, 0, 0, 1 },

    // 38: blank
    { 0, 0, 0, 0, 0, 0, 0 }
  };

  // =========================
  // Display state
  // =========================
  volatile uint8_t charBuffer[DIGITS] = { 0 };

  StepEntry activeTable[MAX_ACTIVE];
  volatile uint8_t activeCount = 0;

  // =========================
  // Brightness state (pulse-density modulation)
  // =========================
  volatile uint8_t brightness;
  volatile uint8_t effectiveDuty;
  volatile uint8_t frame = 0;  // PWM accumulator, 0-63

  // =========================
  // COLON
  // =========================
  volatile bool colonEnabled = true;
  volatile bool colonBlink = false;
  volatile bool colonState = true;
  volatile uint16_t colonBlinkTicks = 0;
  // Same 0-255, gamma-corrected scale as setBrightness().
  volatile uint8_t colonBrightnessLevel;
  // Internal 0-64 PWM duty, compensated for charlieplex multiplexing.
  volatile uint8_t colonBrightness;
  volatile uint8_t colonPwmCounter;  // PWM accumulator, 0-63

  // =========================
  // ISR
  // =========================
  volatile uint8_t isrStep = 0;
  volatile int prevHi = -1;
  volatile int prevLo = -1;

  // =========================
  // TIMER
  // =========================
  hw_timer_t* timer = nullptr;
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

  uint8_t charToIndex(char c);
};

extern CharlieDisplay display;
