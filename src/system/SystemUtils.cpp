#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/Config.h>
#include <DomoticsCore/Logger.h>

// Static member definitions
bool SystemUtils::timeInitialized = false;
const char* SystemUtils::ntpServer = "pool.ntp.org";
const long SystemUtils::gmtOffset_sec = 3600;
const int SystemUtils::daylightOffset_sec = 3600; // Daylight saving time

void SystemUtils::displaySystemInfo() {
  DLOG_I(LOG_SYSTEM, "Chip Model: %s", ESP.getChipModel());
  DLOG_I(LOG_SYSTEM, "Chip Revision: %d", ESP.getChipRevision());
  DLOG_I(LOG_SYSTEM, "CPU Frequency: %d MHz", ESP.getCpuFreqMHz());
  DLOG_I(LOG_SYSTEM, "Flash Size: %d bytes", ESP.getFlashChipSize());
  DLOG_I(LOG_SYSTEM, "Free Heap: %d bytes", ESP.getFreeHeap());
}

void SystemUtils::initializeNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    DLOG_W(LOG_SYSTEM, "Cannot initialize NTP - WiFi not connected");
    return;
  }
  DLOG_I(LOG_SYSTEM, "Initializing NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  int ntpRetries = 0;
  const int maxRetries = 10;

  while (!getLocalTime(&timeinfo) && ntpRetries < maxRetries) {
    DLOG_I(LOG_SYSTEM, "Waiting for NTP time sync... (attempt %d/%d)", ntpRetries + 1, maxRetries);
    delay(1000);
    ntpRetries++;
  }

  if (getLocalTime(&timeinfo)) {
    timeInitialized = true;
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    DLOG_I(LOG_SYSTEM, "Time synchronized: %s", timeStr);
  } else {
    DLOG_E(LOG_SYSTEM, "Failed to synchronize time with NTP server after 10 attempts");
    DLOG_W(LOG_SYSTEM, "System will continue without time synchronization");
  }
}

bool SystemUtils::isTimeInitialized() { return timeInitialized; }

String SystemUtils::getCurrentTimeString() {
  if (!timeInitialized) return "";
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStr);
  }
  return "";
}

// Removed getFormattedLog - Arduino Core logging handles timestamps automatically

String SystemUtils::getFullFirmwareVersion() {
  // This returns the DomoticsCore library version with build info
  // Applications should use DomoticsCore::version() for their own version
  String s;
  s.reserve(32);
  s = String(FIRMWARE_VERSION);  // Library version
  s += "+build.";
  s += String((uint64_t)BUILD_NUMBER_NUM);
  return s;
}
