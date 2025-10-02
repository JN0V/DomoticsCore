# DomoticsCore-NTP

Network Time Protocol (NTP) component for DomoticsCore providing automatic time synchronization with timezone management for ESP32 devices.

## Features

- ✅ Multiple NTP servers with automatic fallback
- ✅ Timezone management with DST support (POSIX TZ strings)
- ✅ Formatted time strings (strftime compatible)
- ✅ ISO 8601 time format support
- ✅ System uptime tracking
- ✅ Configurable sync interval
- ✅ Sync status callbacks
- ✅ WebUI integration with real-time display
- ✅ Configuration persistence via Preferences

## Installation

### PlatformIO

Add to `platformio.ini`:
```ini
lib_deps =
    DomoticsCore-Core @ >=0.2.0
    DomoticsCore-NTP @ >=0.1.0
```

## Quick Start

### Basic Time Synchronization

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>

using namespace DomoticsCore::Components;

Core core;

void setup() {
    Serial.begin(115200);
    
    // Configure NTP
    NTPConfig cfg;
    cfg.timezone = Timezones::CET;          // Central European Time
    cfg.syncInterval = 3600;                // Sync every hour
    cfg.servers = {"pool.ntp.org", "time.google.com"};
    
    auto ntp = std::make_unique<NTPComponent>(cfg);
    auto* ntpPtr = ntp.get();
    
    // Register sync callback
    ntpPtr->onSync([ntpPtr](bool success) {
        if (success) {
            Serial.printf("Time synced: %s\n", ntpPtr->getFormattedTime().c_str());
        } else {
            Serial.println("Time sync failed!");
        }
    });
    
    core.addComponent(std::move(ntp));
    core.begin();
}

void loop() {
    core.loop();
}
```

### Using Timestamps in Logging

```cpp
void loop() {
    core.loop();
    
    auto* ntp = core.getComponent<NTPComponent>("NTP");
    if (ntp && ntp->isSynced()) {
        String timestamp = ntp->getFormattedTime("%Y-%m-%d %H:%M:%S");
        Serial.printf("[%s] Temperature: %.2f°C\n", timestamp.c_str(), readTemp());
    }
    
    delay(60000);  // Every minute
}
```

### Scheduled Tasks

```cpp
void loop() {
    core.loop();
    
    auto* ntp = core.getComponent<NTPComponent>("NTP");
    if (ntp && ntp->isSynced()) {
        struct tm time = ntp->getLocalTime();
        
        // Turn on lights at 18:00
        if (time.tm_hour == 18 && time.tm_min == 0 && !lightsOn) {
            turnOnLights();
            lightsOn = true;
        }
        
        // Turn off at 23:00
        if (time.tm_hour == 23 && time.tm_min == 0 && lightsOn) {
            turnOffLights();
            lightsOn = false;
        }
    }
}
```

## Configuration

### NTPConfig Structure

```cpp
struct NTPConfig {
    bool enabled = true;                    // Enable/disable NTP sync
    std::vector<String> servers = {         // NTP servers
        "pool.ntp.org",
        "time.google.com",
        "time.cloudflare.com"
    };
    uint32_t syncInterval = 3600;           // Sync interval in seconds
    String timezone = "UTC0";               // POSIX TZ string
    uint32_t timeoutMs = 5000;              // Connection timeout
    uint32_t retryDelayMs = 5000;           // Retry delay on failure
};
```

### Common Timezone Presets

```cpp
Timezones::UTC      // "UTC0"
Timezones::EST      // US Eastern
Timezones::CST      // US Central
Timezones::MST      // US Mountain
Timezones::PST      // US Pacific
Timezones::CET      // Central European
Timezones::GMT      // Greenwich Mean
Timezones::JST      // Japan
Timezones::AEST     // Australia Eastern
Timezones::IST      // India
Timezones::NZST     // New Zealand
```

For custom timezones, use POSIX TZ format:
```cpp
cfg.timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Central European with DST
```

## API Reference

### Time Synchronization

```cpp
bool syncNow();                     // Trigger immediate sync
bool isSynced() const;              // Check if time is synced
time_t getLastSyncTime() const;     // Get last sync timestamp
uint32_t getNextSyncIn() const;     // Seconds until next sync
```

### Time Access

```cpp
time_t getUnixTime() const;         // Unix timestamp
struct tm getLocalTime() const;     // Local time structure
String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S") const;
String getISO8601() const;          // "2025-10-02T19:30:45+02:00"
```

### Uptime

```cpp
uint64_t getUptimeMs() const;       // Milliseconds since boot
String getFormattedUptime() const;  // "2d 5h 32m 15s"
```

### Timezone

```cpp
void setTimezone(const String& tz); // Set timezone
String getTimezone() const;         // Get current timezone
int getGMTOffset() const;           // GMT offset in seconds
bool isDST() const;                 // Check if DST is active
```

### Callbacks

```cpp
void onSync(SyncCallback callback); // Register sync callback
```

### Statistics

```cpp
struct NTPStatistics {
    uint32_t syncCount;             // Total successful syncs
    uint32_t syncErrors;            // Total sync failures
    time_t lastSyncTime;            // Last successful sync
    uint32_t lastSyncDuration;      // Last sync duration (ms)
    time_t lastFailTime;            // Last failure time
    uint32_t consecutiveFailures;   // Consecutive failures
};

const NTPStatistics& getStatistics() const;
```

## Time Format Strings

Common `strftime` format strings:

| Format | Output | Example |
|--------|--------|---------|
| `%Y-%m-%d %H:%M:%S` | Full datetime | `2025-10-02 19:30:45` |
| `%Y/%m/%d` | Date | `2025/10/02` |
| `%H:%M` | Time (24h) | `19:30` |
| `%I:%M %p` | Time (12h) | `07:30 PM` |
| `%A, %B %d, %Y` | Long date | `Thursday, October 02, 2025` |
| `%a %b %d` | Short date | `Thu Oct 02` |
| `%Y-W%W` | Week number | `2025-W40` |

## WebUI Integration

The NTP component provides a web interface when used with `DomoticsCore-WebUI`:

### Features

- **Header Badge**: Shows sync status (synced/not synced)
- **Dashboard Card**: Displays current time, date, timezone, uptime
- **Settings Panel**: Configure NTP servers, sync interval, timezone
- **Component Detail**: Statistics, GMT offset, DST status, next sync time

### Usage

```cpp
#include <DomoticsCore/NTPWebUI.h>

auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* ntp = core.getComponent<NTPComponent>("NTP");

if (webui && ntp) {
    webui->registerProviderWithComponent(new NTPWebUI(ntp), ntp);
}
```

## Examples

### BasicNTP
Minimal NTP setup with time synchronization and formatted output.

**Location:** `examples/BasicNTP/`

### NTPWithWebUI
Full-featured example with web interface for configuration and monitoring.

**Location:** `examples/NTPWithWebUI/`

## Troubleshooting

### Time not syncing

**Check WiFi connection:**
```cpp
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
}
```

**Verify NTP server accessibility:**
- Try different servers: `pool.ntp.org`, `time.google.com`, `time.cloudflare.com`
- Check firewall/network allows UDP port 123

**Check sync status:**
```cpp
auto* ntp = core.getComponent<NTPComponent>("NTP");
const auto& stats = ntp->getStatistics();
Serial.printf("Sync attempts: %d, Errors: %d\n", stats.syncCount, stats.syncErrors);
```

### Wrong timezone

**Verify POSIX TZ string format:**
```cpp
// Correct format: STDoffsetDST,start/time,end/time
"CET-1CEST,M3.5.0,M10.5.0/3"
```

**Check offset sign** (opposite of usual):
- `UTC-5` = 5 hours EAST of UTC (Asia)
- `UTC5` = 5 hours WEST of UTC (USA)

### Time drifts

**Reduce sync interval:**
```cpp
cfg.syncInterval = 1800;  // 30 minutes instead of 1 hour
```

## Architecture

The NTP component follows the DomoticsCore modular architecture:

- **Header-only**: No compilation required
- **Zero dependencies**: Only needs DomoticsCore-Core
- **ESP32 SNTP**: Uses built-in ESP32 SNTP client
- **Thread-safe**: Safe to call from multiple tasks
- **Lifecycle managed**: Automatic cleanup on destruction

## Performance

- **Memory**: ~2KB RAM
- **Flash**: ~8KB
- **Sync time**: 1-5 seconds (typical)
- **CPU**: Minimal (SNTP runs in background)

## License

MIT License - See LICENSE file for details

## Contributing

Contributions welcome! Please open an issue or pull request on GitHub.

## Related Components

- **DomoticsCore-Core**: Base framework
- **DomoticsCore-WebUI**: Web interface
- **DomoticsCore-Storage**: Configuration persistence
- **DomoticsCore-MQTT**: Use NTP timestamps in MQTT messages
