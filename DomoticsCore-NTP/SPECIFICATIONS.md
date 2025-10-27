# DomoticsCore-NTP Component Specifications

**Version:** 0.1.0  
**Component:** DomoticsCore-NTP  
**Date:** 2025-10-02

---

## 1. Overview

Network Time Protocol (NTP) component providing automatic time synchronization with timezone management for ESP32 devices.

### 1.1 Purpose
- Synchronize system time with NTP servers
- Manage timezones and DST automatically
- Provide formatted time/date strings
- Track system uptime accurately

### 1.2 Dependencies
- **Required:** DomoticsCore-Core >= 0.2.0
- **Optional:** DomoticsCore-WebUI >= 0.1.0 (for web interface)

---

## 2. Core Features

### 2.1 Time Synchronization
- Connect to multiple NTP servers (primary + fallbacks)
- Configurable sync interval (default: 1 hour)
- Automatic retry on failure with exponential backoff
- Manual sync trigger via API
- Sync status callbacks

### 2.2 Timezone Management
- POSIX TZ string format support
- Common timezone presets (UTC, EST, PST, CET, etc.)
- Automatic DST transitions
- Timezone change at runtime
- GMT offset calculation

### 2.3 Time Access
- Unix timestamp (seconds since epoch)
- Milliseconds since boot (uptime)
- Formatted strings (strftime compatible)
- Struct tm access
- ISO 8601 format strings

### 2.4 Configuration
- Multiple NTP server URLs
- Sync interval (seconds)
- Timezone (POSIX TZ string)
- Enable/disable sync
- Preferences persistence

---

## 3. API Design

### 3.1 Configuration Structure

```cpp
struct NTPConfig {
    bool enabled = true;
    std::vector<String> servers = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    uint32_t syncInterval = 3600;  // seconds (1 hour)
    String timezone = "UTC0";      // POSIX TZ string
    uint32_t timeoutMs = 5000;     // Connection timeout
};
```

### 3.2 Component Interface

```cpp
class NTPComponent : public IComponent {
public:
    NTPComponent(const NTPConfig& cfg = NTPConfig());
    
    // IComponent interface
    String getName() const override { return "NTP"; }
    String getVersion() const override { return "0.1.0"; }
    bool begin() override;
    void loop() override;
    ComponentStatus getStatus() const override;
    
    // Time sync
    bool syncNow();
    bool isSynced() const;
    time_t getLastSyncTime() const;
    uint32_t getNextSyncIn() const;  // seconds
    
    // Time access
    time_t getUnixTime() const;
    struct tm getLocalTime() const;
    String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S") const;
    String getISO8601() const;  // "2025-10-02T19:30:00+02:00"
    
    // Uptime
    uint64_t getUptimeMs() const;
    String getFormattedUptime() const;  // "2d 5h 32m 15s"
    
    // Timezone
    void setTimezone(const String& tz);
    String getTimezone() const;
    int getGMTOffset() const;  // seconds
    bool isDST() const;
    
    // Configuration
    void setConfig(const NTPConfig& cfg);
    const NTPConfig& getNTPConfig() const { return config; }
    
    // Callbacks
    using SyncCallback = std::function<void(bool success)>;
    void onSync(SyncCallback callback);
    
    // Statistics
    struct NTPStatistics {
        uint32_t syncCount = 0;
        uint32_t syncErrors = 0;
        time_t lastSyncTime = 0;
        uint32_t lastSyncDuration = 0;  // ms
    };
    const NTPStatistics& getStatistics() const { return stats; }
};
```

---

## 4. Common Timezone Presets

```cpp
namespace Timezones {
    constexpr const char* UTC = "UTC0";
    constexpr const char* EST = "EST5EDT,M3.2.0,M11.1.0";  // US Eastern
    constexpr const char* PST = "PST8PDT,M3.2.0,M11.1.0";  // US Pacific
    constexpr const char* CET = "CET-1CEST,M3.5.0,M10.5.0/3";  // Central European
    constexpr const char* GMT = "GMT0";
    constexpr const char* JST = "JST-9";  // Japan
    constexpr const char* AEST = "AEST-10AEDT,M10.1.0,M4.1.0/3";  // Australia Eastern
}
```

---

## 5. WebUI Integration

### 5.1 Contexts

**Status Badge:**
- Location: HeaderStatus
- Shows: Sync status (synced/syncing/error)
- Updates: Real-time (5s)

**Dashboard Card:**
- Location: Dashboard
- Shows: Current date/time, timezone, next sync
- Updates: Real-time (1s)

**Settings Card:**
- Location: Settings
- Fields:
  - Enable/disable sync
  - NTP servers (comma-separated)
  - Sync interval (hours)
  - Timezone (dropdown + custom)
  - Manual sync button

**Component Detail:**
- Location: ComponentDetail  
- Shows: Statistics, uptime, sync history
- Updates: Real-time (2s)

---

## 6. Usage Examples

### 6.1 Basic Time Sync

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>

Core core;

void setup() {
    NTPConfig cfg;
    cfg.timezone = Timezones::CET;
    cfg.syncInterval = 3600;  // 1 hour
    
    auto ntp = std::make_unique<NTPComponent>(cfg);
    auto* ntpPtr = ntp.get();
    
    ntpPtr->onSync([ntpPtr](bool success) {
        if (success) {
            DLOG_I(LOG_CORE, "Time synced: %s", ntpPtr->getFormattedTime().c_str());
        }
    });
    
    core.addComponent(std::move(ntp));
    core.begin();
}

void loop() {
    core.loop();
}
```

### 6.2 Logging with Timestamps

```cpp
auto* ntp = core.getComponent<NTPComponent>("NTP");
if (ntp && ntp->isSynced()) {
    String timestamp = ntp->getFormattedTime("%Y-%m-%d %H:%M:%S");
    DLOG_I(LOG_CORE, "[%s] Sensor reading: %.2f°C", timestamp.c_str(), temp);
}
```

### 6.3 Scheduled Tasks

```cpp
auto* ntp = core.getComponent<NTPComponent>("NTP");
struct tm time = ntp->getLocalTime();

// Turn lights on at 18:00
if (time.tm_hour == 18 && time.tm_min == 0 && !lightsOn) {
    turnOnLights();
    lightsOn = true;
}
```

---

## 7. Implementation Notes

### 7.1 ESP32 NTP Client
Use built-in ESP32 SNTP:
```cpp
#include <time.h>
#include <esp_sntp.h>

// Configure
sntp_setoperatingmode(SNTP_OPMODE_POLL);
sntp_setservername(0, "pool.ntp.org");
sntp_init();
```

### 7.2 Timezone Setting
```cpp
setenv("TZ", timezone.c_str(), 1);
tzset();
```

### 7.3 Sync Detection
```cpp
time_t now = time(nullptr);
bool synced = (now > 1000000000);  // After 2001
```

---

## 8. Testing Requirements

### 8.1 Unit Tests
- Timezone conversion accuracy
- Formatted string output
- Sync callback triggering
- Configuration persistence

### 8.2 Integration Tests
- Actual NTP server connection
- Sync interval timing
- Fallback server switching
- WebUI configuration changes

---

## 9. WebUI Mockup

**Dashboard Card:**
```
┌─────────────────────────────────┐
│ 📅 Current Time                 │
├─────────────────────────────────┤
│ 2025-10-02 19:30:45 CET        │
│ Timezone: Europe/Brussels       │
│ Next sync: in 45m 15s           │
│ Uptime: 2d 5h 32m               │
└─────────────────────────────────┘
```

**Settings Card:**
```
┌─────────────────────────────────┐
│ ⚙️ NTP Configuration            │
├─────────────────────────────────┤
│ [x] Enable NTP Sync             │
│ Servers: pool.ntp.org, ...      │
│ Sync Interval: [1] hours        │
│ Timezone: [CET dropdown ▼]      │
│ [Sync Now] button               │
└─────────────────────────────────┘
```

---

## 10. File Structure

```
DomoticsCore-NTP/
├── library.json
├── README.md
├── SPECIFICATIONS.md (this file)
├── include/
│   └── DomoticsCore/
│       ├── NTP.h           # Component class
│       └── NTPWebUI.h      # WebUI provider
└── examples/
    ├── BasicNTP/
    │   ├── platformio.ini
    │   └── src/main.cpp
    └── NTPWithWebUI/
        ├── platformio.ini
        └── src/main.cpp
```

---

**Status:** Ready for implementation ✅
