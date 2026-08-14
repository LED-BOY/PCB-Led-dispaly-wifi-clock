#include "CharlieDisplay.h"
#include "WiFiManager.h"
#include "Utils.h"
#include "driver/gpio.h"
#include <time.h>

// Runtime configuration and synchronization state
uint8_t brightlevel = DEFAULT_BRIGHTNESS;
bool syncIntervalEnable = false;
bool weatherEnable = false;
bool autoBrightnessEnable = false;
bool temperatureModeActive = false;
bool syncInProgress = false;
bool ignoreBootUntilReleased = false;

// Cached weather and daylight data
float weatherTemp = -1000;
int sunriseMinute = -1;
int sunsetMinute = -1;
bool sunTimesValid = false;

uint8_t getAutoBrightnessForTime(struct tm* t) {
  int currentMinute = t->tm_hour * 60 + t->tm_min;

  if (syncIntervalEnable && sunTimesValid) {
    return (currentMinute >= sunriseMinute && currentMinute < sunsetMinute)
        ? AUTO_BRIGHTNESS_HIGH
        : AUTO_BRIGHTNESS_LOW;
  }

  return (t->tm_hour >= 20 || t->tm_hour < 8)
      ? AUTO_BRIGHTNESS_LOW
      : AUTO_BRIGHTNESS_HIGH;
}

uint8_t getDisplayBrightness() {
  if (!temperatureModeActive) return brightlevel;

  return (brightlevel > TEMPERATURE_BRIGHTNESS_REDUCTION)
      ? brightlevel - TEMPERATURE_BRIGHTNESS_REDUCTION
      : 0;
}

void applyDisplayBrightness() {
  const uint8_t level = getDisplayBrightness();
  display.setBrightness(level);
  display.setColonBrightness(level);
}

void applyAutoBrightness(struct tm* t) {
  if (!autoBrightnessEnable || t == nullptr) return;

  uint8_t target = getAutoBrightnessForTime(t);
  if (brightlevel == target) return;

  brightlevel = target;
  applyDisplayBrightness();
}

void updateDisplayMode(bool showTemp, struct tm* t) {

  temperatureModeActive = showTemp && weatherTemp > -999;
  applyDisplayBrightness();

  if (showTemp && weatherTemp > -999) {
    char buf[6];
    utils.formatTemperature(weatherTemp, buf);
    display.print(buf);
  } else {
    display.update(t);
  }
}


// =======================================================
// Weather refresh
// =======================================================
bool updateOpenMeteo() {
  float t, h;
  int newSunriseMinute = -1;
  int newSunsetMinute = -1;

  // A failed or invalid refresh immediately restores the 08:00-20:00 fallback.
  sunTimesValid = false;
  if (!wifi.getWeatherAuto(t, h, newSunriseMinute, newSunsetMinute)) return false;

  if (weatherEnable) {
    weatherTemp = t;
  }
  sunriseMinute = newSunriseMinute;
  sunsetMinute = newSunsetMinute;
  sunTimesValid = true;
  return true;
}

// =======================================================
// NTP
// =======================================================
bool syncNtp() {
  time_t ntpNow = wifi.getNtpTime();
  if (ntpNow == 0) return false;

  struct timeval tv = { ntpNow, 0 };
  settimeofday(&tv, nullptr);

  return true;
}

// =======================================================
// Wi-Fi, NTP, and weather synchronization with short retries
// =======================================================
// Show the SYNC message only for the first synchronization after the portal.
// Scheduled synchronizations leave the current display in place and blink the colon.
void handleWifiAndNtp(bool showSyncMessage) {

  syncInProgress = true;
  // Do not queue a BOOT press made while this blocking synchronization runs.
  ignoreBootUntilReleased = true;

  setCpuFrequencyMhz(CPU_FREQ_MAX);

  // The colon signals synchronization only; manual-time mode keeps it off.
  if (showSyncMessage) display.print("SYNC");
  display.setColon(syncIntervalEnable);
  display.setColonBlink(syncIntervalEnable);

  bool ntpOk = false;
  bool openMeteoNeeded = weatherEnable || (syncIntervalEnable && autoBrightnessEnable);
  bool openMeteoOk = !openMeteoNeeded;
  if (openMeteoNeeded) sunTimesValid = false;
  bool wifiOk = wifi.tryReconnect();

  if (wifiOk) {

    delay(800);  // Allow the Wi-Fi stack to settle after connecting.

    // ======================
    // NTP (up to four attempts)
    // ======================
    for (uint8_t i = 0; i < 4 && !ntpOk; i++) {
      ntpOk = syncNtp();

      if (!ntpOk) {
        delay(300);  // Short delay before retrying.
      }
    }

    // ======================
    // Open-Meteo weather and sunrise/sunset data (up to four attempts)
    // ======================
    if (openMeteoNeeded) {
      for (uint8_t i = 0; i < 4 && !openMeteoOk; i++) {
        openMeteoOk = updateOpenMeteo();

        if (!openMeteoOk) {
          delay(300);
        }
      }
    }

  }

  // The colon reports whether the most recent WiFi connection succeeded.
  // NTP and weather-update failures do not change this indicator.
  display.setColonBlink(false);
  display.setColon(syncIntervalEnable && wifiOk);

  wifi.wifiOff();
  delay(1000);

  setCpuFrequencyMhz(CPU_FREQ_MIN);
  syncInProgress = false;
}


// =======================================================
// Setup
// =======================================================
void setup() {
  utils.setupPins();
  setCpuFrequencyMhz(CPU_FREQ_MAX);

  display.begin();
  display.setBrightness(DEFAULT_BRIGHTNESS);
  // Same 0-255 brightness scale as the charlieplexed display LEDs.
  display.setColonBrightness(DEFAULT_BRIGHTNESS);
  display.print("APON");
  display.setColon(false);

  wifi.startPortalAsync();

  unsigned long t0 = millis();
  uint32_t portalTimeMs = CONFIG_PORTAL_TIME_MS;
  while (millis() - t0 < portalTimeMs) {
    wifi.handlePortal();
    if (WiFi.softAPgetStationNum() > 0) {
      portalTimeMs = CONFIG_PORTAL_CONNECTED_TIME_MS;
    }
    if (wifi.isConfigured() || gpio_get_level((gpio_num_t)BOOT_PIN) == 0) break;
    yield();
  }

  WiFi.softAPdisconnect(true);
  display.print("APOF");
  delay(2000);
  WiFi.mode(WIFI_STA);

  brightlevel = wifi.getBrightness();
  syncIntervalEnable = wifi.isTimeSyncEnabled();
  weatherEnable = syncIntervalEnable && wifi.isTemperatureEnabled();
  autoBrightnessEnable = wifi.isAutoBrightnessEnabled();

  applyDisplayBrightness();
  display.setColon(false);  // Manual time / portal mode starts with no colon.

  if (!syncIntervalEnable) {
    wifi.wifiOff();
  }

  if (syncIntervalEnable) {
    handleWifiAndNtp(true);
  }

  struct tm timeinfo;
  if (!utils.getTimeSafe(timeinfo)) {
    //display.print("--:--");
    wifi.applyManualTimeIfNeeded();
    utils.getTimeSafe(timeinfo);
  }

  applyAutoBrightness(&timeinfo);
}

// =======================================================
// LOOP
// =======================================================
void loop() {

  static struct tm t;
  static uint32_t lastRead = 0;
  static int lastSec = -1;

  static int lastHour = -1;
  static bool hourSynced = false;

  static bool showTemp = true;
  static bool lastMode = false;
  static uint32_t lastToggle = 0;
  static bool bootWasPressed = false;
  static uint32_t lastBootPress = 0;

  // BOOT requests an immediate Wi-Fi/NTP synchronization on each press.
  const bool bootPressed = gpio_get_level((gpio_num_t)BOOT_PIN) == 0;
  if (!bootPressed) {
    bootWasPressed = false;
    ignoreBootUntilReleased = false;
  } else if (!syncInProgress && !ignoreBootUntilReleased && !bootWasPressed &&
             millis() - lastBootPress >= 250) {
    bootWasPressed = true;
    lastBootPress = millis();
    handleWifiAndNtp(true);
  } else {
    bootWasPressed = true;
  }

  // =========================
  // Refresh the cached clock time once per second.
  // =========================
  if (millis() - lastRead >= 1000) {
    lastRead = millis();

    struct tm tmp;
    if (utils.getTimeSafe(tmp)) {
      t = tmp;
      applyAutoBrightness(&t);
    }
  }

  // =========================
  // Alternate between time and temperature display modes.
  // =========================
  // Keep the time and temperature visible for 10 seconds each.
  if (millis() - lastToggle >= 10000) {
    lastToggle = millis();
    showTemp = !showTemp;
  }

  bool shouldShowTemp = weatherEnable && showTemp;

  // =========================
  // Show temperature
  // =========================
  if (shouldShowTemp) {

    if (shouldShowTemp != lastMode) {
      lastMode = shouldShowTemp;
      updateDisplayMode(true, &t);
    }

  }
  // =========================
  // Show time
  // =========================
  else {

    if (t.tm_sec != lastSec) {
      lastSec = t.tm_sec;
      updateDisplayMode(false, &t);
    }

    lastMode = false;
  }

  // =========================
  // Hourly synchronization and weather retry
  // =========================
  if (t.tm_hour != lastHour) {
    // Setup performs the initial synchronization; normal operation begins
    // scheduling at the next observed hour.
    const bool firstObservedHour = lastHour == -1;
    lastHour = t.tm_hour;
    hourSynced = firstObservedHour;
  }

  // Sync once after entering a new hour. This catches a missed HH:00 minute
  // when another blocking operation was running.
  if (syncIntervalEnable && !hourSynced) {
    hourSynced = true;
    handleWifiAndNtp(false);
  } else if (weatherEnable && weatherTemp <= -999) {
    // Weather retries do not mark the current hour as synchronized.
    handleWifiAndNtp(false);
  }
}
