#include "WiFiManager.h"
#include "CharlieDisplay.h"

WiFiManager wifi;

WiFiManager::WiFiManager() {}

// ===============================
// 📦 LOAD PREFS CACHE
// ===============================
void WiFiManager::loadPrefs() {

  if (prefsLoaded) return;

  prefs.begin("wifi", true);

  syncTimeCached = prefs.getBool("syncTime", true);
  showTempCached = prefs.getBool("showTemp", false);
  brightnessCached = prefs.getInt("brightness", DEFAULT_BRIGHTNESS);
  autoBrightnessCached = prefs.getBool("autoBrightness", false);

  prefs.end();

  prefsLoaded = true;
}

// ===============================
// 🔧 HELPERS
// ===============================
bool WiFiManager::httpGET(const String& url, String& response) {
  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  if (code != 200) {
    http.end();
    return false;
  }

  response = http.getString();
  http.end();
  return true;
}

int WiFiManager::getPrefInt(const char* key, int def) {
  prefs.begin("wifi", true);
  int val = prefs.getInt(key, def);
  prefs.end();
  return val;
}

// ===============================
// 📶 WIFI
// ===============================
bool WiFiManager::connect(uint32_t timeoutMs) {

  // Manual-time mode is intentionally offline, even with saved credentials.
  if (!isTimeSyncEnabled()) return false;

  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  prefs.end();

  if (ssid.isEmpty()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    delay(100);
  }

  if (isTimeSyncEnabled()) {
    syncWithNTP();
  }

  return true;
}

bool WiFiManager::tryReconnect(uint32_t timeoutMs) {
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  if (isConnected()) return true;
  return connect(timeoutMs);
}

// ===============================
// 🌐 PORTAL
// ===============================
void WiFiManager::startPortalAsync() {

  configured = false;

  prefs.begin("wifi", true);
  String ssidSaved = prefs.getString("ssid", "");
  String passwordSaved = prefs.getString("password", "");
  int offsetSaved = prefs.getInt("offset", DEFAULT_TIMEZONE_OFFSET);
  int offsetMode = prefs.getInt("offsetMode", 1);
  prefs.end();

  loadPrefs();  // Load once; subsequent accesses use the in-memory cache.

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ClockSetup", "12345678");
  delay(500);
  server.on("/update", HTTP_GET, [this]() {
    String html = R"====(
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; padding:20px; }
button { padding:10px 20px; font-size:16px; margin-right:8px; }
input { margin:8px 0; }
.file-input {
  position:absolute;
  width:1px;
  height:1px;
  opacity:0;
}
.file-button {
  display:inline-block;
  padding:4px 12px;
  margin:8px 8px 8px 0;
  border:1px solid #888;
  background:#eee;
  cursor:pointer;
}
</style>
</head>
<body>
<h2>Firmware Update</h2>
)====";

    html += "<p>FW:";
    html += FIRMWARE_VERSION;
    html += "</p>";

    html += R"====(
<form method='POST' action='/update' enctype='multipart/form-data'>
  Select firmware file:<br>
  <input class='file-input' id='firmwareFile' type='file' name='update' accept='.bin' required>
  <label class='file-button' for='firmwareFile'>Choose File</label>
  <span id='fileName'>No file chosen</span>
  <br><br>
  <button type='submit'>Upload Firmware</button>
  <button type='button' onclick="window.location.href='/'">Back to Setup</button>
</form>
<script>
document.getElementById('firmwareFile').addEventListener('change', function() {
  document.getElementById('fileName').textContent =
    this.files.length ? this.files[0].name : 'No file chosen';
});
</script>
</body>
</html>
)====";

    server.send(200, "text/html", html);
  });

  server.on("/manual", HTTP_GET, [this]() {
    String html = R"====(
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; padding:20px; line-height:1.45; }
button { padding:10px 20px; font-size:16px; }
h2 { margin-top:0; }
h3 { margin-bottom:4px; }
p { margin-top:4px; }
</style>
</head>
<body>
<h2>Portal Manual</h2>

<h3>Wi-Fi Setup</h3>
<p>Enter your Wi-Fi network name (SSID) and password so the clock can connect to the internet. Only 2.4 GHz Wi-Fi networks are compatible.</p>

<h3>Time zone</h3>
<p>Select <b>Auto (IP)</b> to detect the time zone from your internet connection, or choose a fixed UTC offset.</p>

<h3>Auto sync time</h3>
<p>When enabled, the clock sets its time from the internet. Turn it off to use <b>Manual time</b>; enter the time as HHMM, for example 0930.</p>

<h3>Show temperature</h3>
<p>When automatic time sync is enabled, this option shows the temperature on the clock display.</p>

<h3>Brightness</h3>
<p>Enable <b>Auto brightness</b> to let the clock adjust automatically. With automatic time sync enabled, the clock uses dawn and sunset data from the server. Otherwise, it uses the default night schedule from 20:00 to 08:00. Turn Auto brightness off to choose Low or High brightness yourself.</p>

<h3>Save</h3>
<p>Press Save to store the settings. The clock reboots after saving.</p>

<h3>Firmware Update</h3>
<p>Use this page to select and upload a compatible .bin firmware file, then wait for the update to finish.</p>

<h3>BOOT button</h3>
<p>Press the BOOT button while the clock is running to force an immediate Wi-Fi and time synchronization.</p>

<br><button type='button' onclick="window.location.href='/'">Back to Setup</button>
</body>
</html>
)====";

    server.send(200, "text/html", html);
  });

  server.on("/", [=]() {
    String html = R"====(
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; padding:20px; }
input:not([type='checkbox']), select {
  width:100%;
  max-width:300px;
  padding:8px;
  margin:8px 0;
}

input[type='checkbox'] {
  width:auto;
  max-width:none;
  padding:0;
  margin-left:8px;
  transform:scale(1.1);
}

button { padding:10px 20px; font-size:16px; }
.portal-link {
  display:inline-block;
  min-width:180px;
  padding:12px 16px;
  margin:8px 0;
  border:1px solid #666;
  border-radius:4px;
  background:#eee;
  color:#111;
  font-size:16px;
  text-align:center;
  text-decoration:none;
}
)====";

    if (syncTimeCached) {
      html += "#manualBlock { display:none; }";
    } else {
      html += "#tempBlock { display:none; }";
    }

    if (autoBrightnessCached) {
      html += "#brightnessBlock { display:none; }";
    }

    html += R"====(
</style>
</head>
<body>
)====";


    html += R"====(
<h2>Wi-Fi Setup</h2>

<form id='setupForm' action='/save' method='POST'>
)====";

    html += "SSID:<br><input name='ssid' value='" + ssidSaved + "'><br>";
    html += "Password:<br><input name='password' type='password' value='" + passwordSaved + "'><br><br>";

    html += "Time zone:<br><select id='offset' name='offset'>";

    html += "<option value='auto' ";
    if (offsetMode == 1) html += "selected";
    html += ">Auto (IP)</option>";

    int offs[] = { -8, -5, -3, 0, 1, 2, 3, 5, 8, 9, 10 };
    const char* labels[] = {
      "UTC-8 LA", "UTC-5 NY", "UTC-3 Uruguay",
      "UTC+0 London", "UTC+1 EU", "UTC+2 Cairo",
      "UTC+3 Moscow", "UTC+5 Karachi", "UTC+8 China",
      "UTC+9 Japan", "UTC+10 Sydney"
    };

    for (int i = 0; i < 11; i++) {
      html += "<option value='" + String(offs[i]) + "'";
      if (offsetMode == 0 && offsetSaved == offs[i]) html += " selected";
      html += ">" + String(labels[i]) + "</option>";
    }

    html += "</select><br><br>";

    html += "Auto sync time:<input type='checkbox' id='syncTime' name='syncTime' ";
    if (syncTimeCached) html += "checked";
    html += "><br><br>";

    html += "<div id='manualBlock'>";
    html += "Manual time (HHMM):<br>";
    html += "<input id='manualTime' name='manualTime' type='text' inputmode='numeric' maxlength='4' placeholder='HHMM' autocomplete='off'>";
    html += "<div id='manualTimeError' style='display:none;color:#b00020;margin-top:4px'>Valid values are from 0000 to 2359.</div><br><br>";
    html += "</div>";

    html += "<div id='tempBlock'>";
    html += "Show temperature:<input type='checkbox' name='showTemp' ";
    if (showTempCached) html += "checked";
    html += "><br><br>";
    html += "</div>";

    html += "Auto brightness:<input type='checkbox' id='autoBrightness' name='autoBrightness' ";
    if (autoBrightnessCached) html += "checked";
    html += "><br><br>";

    html += "<div id='brightnessBlock'>";
    html += "Brightness:<br><select name='brightness'>";

    html += "<option value='" + String(AUTO_BRIGHTNESS_LOW) + "' ";
    if (brightnessCached == AUTO_BRIGHTNESS_LOW) html += "selected";
    html += ">Low</option>";

    html += "<option value='" + String(AUTO_BRIGHTNESS_HIGH) + "' ";
    if (brightnessCached == AUTO_BRIGHTNESS_HIGH) html += "selected";
    html += ">High</option>";

    html += "</select><br><br>";
    html += "</div>";

    html += "<button id='saveButton' type='submit'>Save</button></form>";

    html += "<br><br><a class='portal-link' href='/manual'>Portal Manual</a>";
    html += "<br><br><a class='portal-link' href='/update'>Firmware Update</a>";

    html += R"====(
<script>
function toggleManualTime() {
  const sync = document.getElementById('syncTime');
  const manual = document.getElementById('manualBlock');
  const temp = document.getElementById('tempBlock');
  const offset = document.getElementById('offset');

  if (!sync) return;

  if (manual) manual.style.display = sync.checked ? 'none' : 'block';
  if (temp) temp.style.display = sync.checked ? 'block' : 'none';
  if (offset) {
    offset.disabled = !sync.checked;
    offset.style.opacity = sync.checked ? '1' : '0.55';
  }
}

function toggleBrightness() {
  const auto = document.getElementById('autoBrightness');
  const brightness = document.getElementById('brightnessBlock');

  if (!auto || !brightness) return;

  brightness.style.display = auto.checked ? 'none' : 'block';
}

function validateManualTime() {
  const sync = document.getElementById('syncTime');
  const manualTime = document.getElementById('manualTime');
  const error = document.getElementById('manualTimeError');

  if (!manualTime || !error || (sync && sync.checked)) return true;

  const value = manualTime.value.trim();
  // A blank manual time is intentionally saved as 0000 (midnight).
  const valid = value === '' || (/^\d{4}$/.test(value) &&
      Number(value.slice(0, 2)) <= 23 && Number(value.slice(2, 4)) <= 59);

  error.style.display = valid ? 'none' : 'block';
  if (!valid) manualTime.focus();
  return valid;
}

document.addEventListener("DOMContentLoaded", function() {
  const sync = document.getElementById('syncTime');
  const auto = document.getElementById('autoBrightness');
  const form = document.getElementById('setupForm');
  const manualTime = document.getElementById('manualTime');
  const saveButton = document.getElementById('saveButton');

  if (sync) {
    sync.addEventListener('change', toggleManualTime);
    toggleManualTime();
  }

  if (auto) {
    auto.addEventListener('change', toggleBrightness);
    toggleBrightness();
  }

  if (form) form.addEventListener('submit', function(event) {
    if (!validateManualTime()) {
      event.preventDefault();
      return;
    }

    if (saveButton) {
      saveButton.disabled = true;
      saveButton.textContent = 'Saving...';
      saveButton.style.backgroundColor = '#999';
      saveButton.style.color = '#eee';
      saveButton.style.cursor = 'wait';
    }
  });

  if (manualTime) manualTime.addEventListener('input', validateManualTime);
});
</script>
)====";

    html += "</body></html>";

    server.send(200, "text/html", html);
  });

  // =========================
  // SAVE
  // =========================
  server.on("/save", [this]() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String offsetStr = server.arg("offset");
    String manualTime = server.arg("manualTime");

    bool syncTime = server.hasArg("syncTime");
    bool showTemp = server.hasArg("showTemp");
    bool autoBrightness = server.hasArg("autoBrightness");

    // Start with the saved automatic-time preference. Manual mode uses its
    // own working offset and must not overwrite the user's time-zone choice.
    prefs.begin("wifi", true);
    int offset = prefs.getInt("offset", DEFAULT_TIMEZONE_OFFSET);
    int offsetMode = prefs.getInt("offsetMode", 1);
    prefs.end();

    if (syncTime) {
      if (offsetStr == "auto") {
        offset = detectTimezoneOffsetByIP();
        offsetMode = 1;
      } else {
        offset = offsetStr.toInt();
        offsetMode = 0;
      }
    }

    // Manual time uses DEFAULT_MANUAL_TIMEZONE_OFFSET internally. Preserve
    // the saved automatic-time offset so it resumes when sync is re-enabled.
    if (!syncTime) {
      showTemp = false;
    }

    // In manual mode, an empty field means midnight (0000).
    if (!syncTime && manualTime.length() == 0) {
      manualTime = "0000";
    }

    bool validManualTime = manualTime.length() == 4;
    for (uint8_t i = 0; i < manualTime.length() && validManualTime; i++) {
      validManualTime = manualTime[i] >= '0' && manualTime[i] <= '9';
    }

    int enteredHour = manualTime.substring(0, 2).toInt();
    int enteredMinute = manualTime.substring(2, 4).toInt();
    validManualTime = validManualTime && enteredHour <= 23 && enteredMinute <= 59;

    if (!syncTime && validManualTime) {
      manualHour = enteredHour;
      manualMinute = enteredMinute;
      manualTimeSet = true;
    } else {
      manualTimeSet = false;
    }

    int brightness = server.arg("brightness").toInt();

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.putInt("offset", offset);
    prefs.putInt("offsetMode", offsetMode);
    prefs.putInt("brightness", brightness);
    prefs.putBool("syncTime", syncTime);
    prefs.putBool("showTemp", showTemp);
    prefs.putBool("autoBrightness", autoBrightness);
    prefs.end();

    // Keep the active portal state aligned with the saved preferences.
    syncTimeCached = syncTime;
    showTempCached = showTemp;
    brightnessCached = brightness;
    autoBrightnessCached = autoBrightness;
    prefsLoaded = true;

    configured = true;

    server.send(200, "text/html", "<html><body><h2>Saved! Rebooting...</h2></body></html>");

    delay(1200);
    server.stop();
    WiFi.softAPdisconnect(true);
  });

  server.on(
    "/update",
    HTTP_POST,
    [this]() {
      bool ok = otaUpdateOk && !Update.hasError();

      server.sendHeader("Connection", "close");
      if (ok) {
        server.send(
          200,
          "text/html",
          R"====(<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family:Arial; padding:20px; }
</style>
</head>
<body>
<h2>Upgrade Successful!</h2>
<p>Wait for the device to restart. After that, manually reset the device.</p>
</body>
</html>)====");
      } else {
        server.send(200, "text/html", "<html><body><h2>Update failed.</h2></body></html>");
      }

      if (ok) {
        // Give the browser time to receive the success response, then boot the
        // freshly written OTA image even if the setup portal is about to time out.
        delay(1500);
        ESP.restart();
      }
    },
    [this]() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        display.print("FWUP");
        display.setColon(false);
        otaUpdateOk = false;
        Update.begin(UPDATE_SIZE_UNKNOWN);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!Update.hasError()) {
          Update.write(upload.buf, upload.currentSize);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.hasError()) {
          otaUpdateOk = Update.end(true);
        }
      }
    });

  server.begin();
}

// ===============================
// 🌦 WEATHER
// ===============================
bool WiFiManager::getWeatherAuto(float& tempOut, float& humOut,
                                 int& sunriseMinuteOut, int& sunsetMinuteOut) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String city;
  if (!httpGET("http://ip-api.com/line/?fields=city", city))
    return false;

  city.trim();

  String geo;
  if (!httpGET("http://geocoding-api.open-meteo.com/v1/search?name=" + city + "&count=1", geo))
    return false;

  int latIndex = geo.indexOf("\"latitude\":");
  int lonIndex = geo.indexOf("\"longitude\":");
  if (latIndex < 0 || lonIndex < 0) return false;

  float lat = geo.substring(latIndex + 11).toFloat();
  float lon = geo.substring(lonIndex + 12).toFloat();

  String weather;
  String url =
    "http://api.open-meteo.com/v1/forecast?"
    "latitude="
    + String(lat, 4) + "&longitude=" + String(lon, 4)
    + "&current=temperature_2m,relative_humidity_2m"
      "&daily=sunrise,sunset&timezone=auto&forecast_days=1";

  if (!httpGET(url, weather)) return false;

  int curStart = weather.indexOf("\"current\":{");
  int curEnd = weather.indexOf("}", curStart);
  if (curStart < 0 || curEnd < 0) return false;

  String cur = weather.substring(curStart, curEnd);

  tempOut = extractFloat(cur, "\"temperature_2m\":");
  humOut = extractFloat(cur, "\"relative_humidity_2m\":");
  sunriseMinuteOut = extractIsoTimeMinutes(weather, "\"sunrise\":[\"");
  sunsetMinuteOut = extractIsoTimeMinutes(weather, "\"sunset\":[\"");

  bool validWeather = !(isnan(tempOut) || isnan(humOut));
  bool validSunTimes = sunriseMinuteOut >= 0 && sunriseMinuteOut < 1440
      && sunsetMinuteOut >= 0 && sunsetMinuteOut < 1440
      && sunriseMinuteOut < sunsetMinuteOut;

  return validWeather && validSunTimes;
}

// Extract a numeric weather field from a small JSON fragment.
float WiFiManager::extractFloat(const String& src, const String& key) {
  int idx = src.indexOf(key);
  if (idx < 0) return NAN;

  int start = src.indexOf(":", idx) + 1;
  int end = src.indexOf(",", start);

  return src.substring(start, end).toFloat();
}

int WiFiManager::extractIsoTimeMinutes(const String& src, const String& key) {
  int idx = src.indexOf(key);
  if (idx < 0) return -1;

  int timeStart = src.indexOf('T', idx + key.length());
  if (timeStart < 0 || timeStart + 5 >= src.length()) return -1;

  char separator = src[timeStart + 3];
  if (separator != ':') return -1;

  if (!isDigit(src[timeStart + 1]) || !isDigit(src[timeStart + 2])
      || !isDigit(src[timeStart + 4]) || !isDigit(src[timeStart + 5])) return -1;

  int hour = src.substring(timeStart + 1, timeStart + 3).toInt();
  int minute = src.substring(timeStart + 4, timeStart + 6).toInt();
  if (hour > 23 || minute > 59) return -1;

  return hour * 60 + minute;
}

// ===============================
// 🕒 TIME
// ===============================
void WiFiManager::syncWithNTP() {
  if (!timezoneLoaded) {
    prefs.begin("wifi", true);
    cachedOffset = prefs.getInt("offset", DEFAULT_TIMEZONE_OFFSET);
    prefs.end();
    timezoneLoaded = true;
  }

  configTime(cachedOffset * 3600, 0, "pool.ntp.org");
  delay(800);
}

time_t WiFiManager::getNtpTime() {
  time_t now = time(nullptr);
  return (now > 100000) ? now : 0;
}

void WiFiManager::applyManualTimeIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!manualTimeSet) return;

  // Manual HHMM is wall-clock time. Clear any timezone left by a previous
  // NTP session before mktime() converts it to an epoch.
  static_assert(DEFAULT_MANUAL_TIMEZONE_OFFSET == 0,
                "Manual wall-clock conversion currently requires UTC+0");
  setenv("TZ", "UTC0", 1);
  tzset();

  struct tm t = {};
  t.tm_year = DEFAULT_YEAR - 1900;
  t.tm_mon = 0;
  t.tm_mday = 1;
  t.tm_hour = manualHour;
  t.tm_min = manualMinute;

  struct timeval now = { mktime(&t), 0 };
  settimeofday(&now, nullptr);
}

// ===============================
// 🌍 TIMEZONE
// ===============================
int WiFiManager::detectTimezoneOffsetByIP() {
  String json;
  if (!httpGET("https://ipapi.co/json/", json))
    return DEFAULT_TIMEZONE_OFFSET;

  int idx = json.indexOf("\"utc_offset\":\"");
  if (idx < 0) return DEFAULT_TIMEZONE_OFFSET;

  String off = json.substring(idx + 14, idx + 20);

  int sign = (off[0] == '-') ? -1 : 1;
  int hours = off.substring(1, 3).toInt();

  return sign * hours;
}

// ===============================
// Basic operations
// ===============================
void WiFiManager::handlePortal() {
  server.handleClient();
}

bool WiFiManager::isConfigured() {
  return configured;
}

bool WiFiManager::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ===============================
// Cached configuration accessors
// ===============================
bool WiFiManager::isTemperatureEnabled() {
  loadPrefs();
  return showTempCached;
}

bool WiFiManager::isTimeSyncEnabled() {
  loadPrefs();
  return syncTimeCached;
}

int WiFiManager::getBrightness() {
  loadPrefs();
  return brightnessCached;
}

bool WiFiManager::isAutoBrightnessEnabled() {
  loadPrefs();
  return autoBrightnessCached;
}
