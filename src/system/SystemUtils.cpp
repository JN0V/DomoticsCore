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
  DLOG_I(LOG_SYSTEM, "Initializing NTP (non-blocking)...");
  
  // Just configure NTP, don't wait for sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Check if time is already synchronized (quick check)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    timeInitialized = true;
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    DLOG_I(LOG_SYSTEM, "Time already synchronized: %s", timeStr);
  } else {
    DLOG_I(LOG_SYSTEM, "NTP sync started in background - time will be available shortly");
    // Time sync will happen in background, we'll check later in loop
  }
}

bool SystemUtils::isTimeInitialized() { return timeInitialized; }

void SystemUtils::setTimeInitialized(bool initialized) { timeInitialized = initialized; }

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

void SystemUtils::watchdogSafeDelay(unsigned long milliseconds) {
  // Break down the delay into 10ms chunks with yield() calls
  // to prevent watchdog timer resets
  unsigned long chunks = milliseconds / 10;
  unsigned long remainder = milliseconds % 10;
  
  for (unsigned long i = 0; i < chunks; i++) {
    yield();
    delay(10);
  }
  
  if (remainder > 0) {
    yield();
    delay(remainder);
  }
}
