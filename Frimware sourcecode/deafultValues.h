#ifndef DEFAULTVALUES_H
#define DEFAULTVALUES_H
// =======================
// firmware version
// =======================
static constexpr const char* FIRMWARE_VERSION = "2.8.0";

// =======================
// CPU Frequency
// =======================
constexpr uint32_t CPU_FREQ_MIN = 80;  // MHz used while the clock is idle.
constexpr uint32_t CPU_FREQ_MAX = 120; // MHz used for portal and network operations.

// =======================
// Timing
// =======================
constexpr uint32_t CONFIG_PORTAL_TIME_MS = 60000UL; // 60 s
constexpr uint32_t CONFIG_PORTAL_CONNECTED_TIME_MS = 600000UL; // 10 min

// =======================
// Display
// =======================

constexpr uint8_t DEFAULT_BRIGHTNESS = 200;
constexpr uint8_t TEMPERATURE_BRIGHTNESS_REDUCTION = 30;
const uint8_t AUTO_BRIGHTNESS_LOW = 150;
const uint8_t AUTO_BRIGHTNESS_HIGH = 240;

// =======================
// Timezone
// =======================

constexpr int DEFAULT_TIMEZONE_OFFSET = -3;
constexpr int DEFAULT_MANUAL_TIMEZONE_OFFSET = 0;

// =======================
// WiFi
// =======================

constexpr uint32_t WIFI_CONNECT_TIMEOUT = 8000;

// =======================
// Year
// =======================
constexpr int DEFAULT_YEAR = 2026;

#endif
