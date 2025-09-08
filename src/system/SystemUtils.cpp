#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/Config.h>

// Static member definitions
bool SystemUtils::timeInitialized = false;
const char* SystemUtils::ntpServer = "pool.ntp.org";
const long SystemUtils::gmtOffset_sec = 3600;
const int SystemUtils::daylightOffset_sec = 3600; // Daylight saving time

void SystemUtils::displaySystemInfo() {
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size: %d bytes\n", ESP.getFlashChipSize());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
}

void SystemUtils::initializeNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Warning: Cannot initialize NTP - WiFi not connected");
    return;
  }
  Serial.println("Initializing NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  int ntpRetries = 0;
  const int maxRetries = 10;

  while (!getLocalTime(&timeinfo) && ntpRetries < maxRetries) {
    Serial.printf("Waiting for NTP time sync... (attempt %d/%d)\n", ntpRetries + 1, maxRetries);
    delay(1000);
    ntpRetries++;
  }

  if (getLocalTime(&timeinfo)) {
    timeInitialized = true;
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("Time synchronized: %s\n", timeStr);
  } else {
    Serial.println("Error: Failed to synchronize time with NTP server after 10 attempts");
    Serial.println("System will continue without time synchronization");
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

String SystemUtils::getFormattedLog(const String& message) {
  if (timeInitialized) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[32];
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
      return "[" + String(timeStr) + "] " + message;
    }
  }
  return "[" + String(millis()/1000) + "] " + message;
}

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
