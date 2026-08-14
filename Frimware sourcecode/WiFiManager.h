#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <deafultValues.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <HTTPClient.h>
#include <Update.h>

class WiFiManager {

public:
  WiFiManager();

  // Wi-Fi connection
  bool tryReconnect(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT);

  // Configuration portal
  void startPortalAsync();
  void handlePortal();

  // Connection state
  bool isConnected();
  bool isConfigured();

  // Time
  void applyManualTimeIfNeeded();
  time_t getNtpTime();

  // Weather
  bool getWeatherAuto(float &tempOut, float &humOut,
                      int &sunriseMinuteOut, int &sunsetMinuteOut);

  // Cached configuration
  bool isTimeSyncEnabled();
  bool isTemperatureEnabled();
  bool isAutoBrightnessEnabled();
  int getBrightness();

  // Power management
  void wifiOff();

private:
  Preferences prefs;
  WebServer server{80};

  bool configured = false;

  // =========================
  // OTA update state
  // =========================
  bool otaUpdateOk = false;

  // =========================
  // Preferences cache
  // =========================
  bool syncTimeCached = true;
  bool showTempCached = false;
  bool autoBrightnessCached = false;
  int brightnessCached = DEFAULT_BRIGHTNESS;

  bool prefsLoaded = false;

  void loadPrefs();

  // =========================
  // 🕒 Manual time
  // =========================
  bool manualTimeSet = false;
  int manualHour = -1;
  int manualMinute = -1;

  // =========================
  // 🌍 Timezone cache
  // =========================
  int cachedOffset = DEFAULT_TIMEZONE_OFFSET;
  bool timezoneLoaded = false;

  // =========================
  // 🔧 Helpers
  // =========================
  bool connect(uint32_t timeoutMs);
  bool httpGET(const String& url, String& response);
  int getPrefInt(const char* key, int def);
  float extractFloat(const String& src, const String& key);
  int extractIsoTimeMinutes(const String& src, const String& key);

  // =========================
  // 🕒 Time
  // =========================
  void syncWithNTP();
  int detectTimezoneOffsetByIP();
};

extern WiFiManager wifi;

#endif
