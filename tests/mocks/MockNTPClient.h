#pragma once

/**
 * @file MockNTPClient.h
 * @brief Mock NTP client for isolated unit testing without real NTP servers
 * 
 * This mock allows testing NTP-dependent components without requiring
 * actual network time synchronization.
 */

#include <Arduino.h>
#include <functional>
#include <time.h>

namespace DomoticsCore {
namespace Mocks {

/**
 * @brief Mock NTP client that simulates time synchronization
 */
class MockNTPClient {
public:
    // Simulated state
    static bool synced;
    static time_t currentTime;
    static String timezone;
    static int syncAttempts;
    static bool shouldFailSync;
    
    // Callbacks for test verification
    static std::function<void()> onSyncAttempt;
    static std::function<void(time_t)> onTimeSet;
    
    // NTP operations
    static bool begin(const char* server1 = nullptr, const char* server2 = nullptr) {
        // Just record that begin was called
        return true;
    }
    
    static bool sync() {
        syncAttempts++;
        if (onSyncAttempt) onSyncAttempt();
        
        if (shouldFailSync) {
            return false;
        }
        
        synced = true;
        return true;
    }
    
    static bool isSynced() { return synced; }
    
    static time_t getTime() { return currentTime; }
    
    static void setTimezone(const String& tz) {
        timezone = tz;
    }
    
    static String getTimezone() { return timezone; }
    
    static String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S") {
        char buffer[64];
        struct tm* timeinfo = localtime(&currentTime);
        strftime(buffer, sizeof(buffer), format, timeinfo);
        return String(buffer);
    }
    
    // Test control methods
    static void simulateSync(time_t time = 1734508800) {  // 2024-12-18 08:00:00 UTC
        currentTime = time;
        synced = true;
        if (onTimeSet) onTimeSet(time);
    }
    
    static void simulateSyncFailure() {
        shouldFailSync = true;
    }
    
    static void simulateTimePassing(unsigned long seconds) {
        currentTime += seconds;
    }
    
    static void reset() {
        synced = false;
        currentTime = 0;
        timezone = "UTC0";
        syncAttempts = 0;
        shouldFailSync = false;
        onSyncAttempt = nullptr;
        onTimeSet = nullptr;
    }
    
    // Verification helpers
    static int getSyncAttemptCount() { return syncAttempts; }
};

// Static member initialization
bool MockNTPClient::synced = false;
time_t MockNTPClient::currentTime = 0;
String MockNTPClient::timezone = "UTC0";
int MockNTPClient::syncAttempts = 0;
bool MockNTPClient::shouldFailSync = false;
std::function<void()> MockNTPClient::onSyncAttempt = nullptr;
std::function<void(time_t)> MockNTPClient::onTimeSet = nullptr;

} // namespace Mocks
} // namespace DomoticsCore
