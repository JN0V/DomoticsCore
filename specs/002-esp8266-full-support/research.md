# Research: ESP8266 Full Support

**Feature**: 002-esp8266-full-support  
**Date**: 2025-12-18  
**Purpose**: Resolve technical unknowns and document best practices for ESP8266 support

## Research Topics

### 1. C++ Standard Version

**Decision**: 
- **ESP32**: Force `-std=gnu++14` (framework defaults to C++11)
- **ESP8266**: Do NOT force any standard (framework uses C++17 internally)

**Rationale**:
- ESP32 Arduino framework defaults to C++11, but we need C++14 for `std::make_unique`
- ESP8266 Arduino framework 3.x uses C++17 features internally (`is_same_v` in `WString.h`)
- Forcing C++14 on ESP8266 breaks framework compilation
- **Lesson learned**: `embed_webui.py` was forcing C++14 globally which broke ESP8266

**Configuration**:
```ini
; ESP32 - force C++14 (needed for std::make_unique)
build_flags = -std=gnu++14
build_unflags = -std=gnu++11

; ESP8266 - do NOT force any C++ standard
build_flags = -DCORE_DEBUG_LEVEL=3
; NO -std=gnu++14 here!
```

---

### 2. ESPAsyncWebServer for ESP8266

**Decision**: Use `ESP32Async/ESPAsyncWebServer` without version locks (same lib for both platforms)

**Rationale**: 
- ESP32Async/ESPAsyncWebServer supports both ESP32 and ESP8266
- Version locks are unnecessary - latest versions work fine
- Using same library for both platforms simplifies maintenance

**PlatformIO Configuration** (unified, no version locks):
```ini
; ESP32
lib_deps = 
    ESP32Async/AsyncTCP
    ESP32Async/ESPAsyncWebServer

; ESP8266
lib_deps = 
    ESP32Async/ESPAsyncTCP
    ESP32Async/ESPAsyncWebServer
```

**Note**: Version locks like `@^3.8.0` are not required. PlatformIO resolves latest compatible versions automatically.

---

### 3. LittleFS vs SPIFFS for Storage

**Decision**: Use LittleFS exclusively on ESP8266

**Rationale**:
- SPIFFS is deprecated in ESP8266 Arduino Core >= 3.0.0
- LittleFS has better wear leveling and power-loss resilience
- Already implemented in `Storage_HAL.h` via `LittleFSStorage` class
- 2KB JSON document limit fits well within LittleFS capabilities

**Alternatives Considered**:
- SPIFFS: Deprecated, worse performance
- EEPROM emulation: Too limited for structured data
- External SD card: Adds hardware complexity, violates KISS

---

### 4. ESP8266 Arduino Core Version

**Decision**: Require ESP8266 Arduino Core >= 3.0.0

**Rationale**:
- LittleFS is default filesystem from 3.0.0
- Better WiFi stability and reconnection handling
- Improved SSL/TLS stack (though still limited)
- `configTime()` improvements for NTP

**Alternatives Considered**:
- Core 2.x: Older, SPIFFS-based, less stable WiFi
- Core 3.1.x (latest): Could use, but 3.0.0 is minimum for compatibility

---

### 5. RAM Optimization Strategies

**Decision**: Per-component heap budgets with tiered low-heap response

**Rationale**:
- ESP8266 has only 80KB RAM, ~50KB usable after system overhead
- Fixed budgets per component allow predictable memory usage
- Tiered response (<15KB warn, <10KB suspend WebUI, <5KB reboot) provides graceful degradation

**Strategies Documented**:

| Strategy | When to Use |
|----------|-------------|
| `PROGMEM` for strings | Static strings in flash, not RAM |
| `F()` macro | String literals in print statements |
| Small JSON documents | Max 2KB, use `StaticJsonDocument` when possible |
| Avoid String concatenation | Use char buffers or streaming |
| Release resources early | Close connections, free buffers promptly |

---

### 6. WiFi Reconnection on ESP8266

**Decision**: Use existing `Wifi_HAL.h` with ESP8266WiFi.h

**Rationale**:
- ESP8266WiFi.h API is similar to ESP32 WiFi
- `WiFi.setAutoReconnect(true)` works on both platforms
- Event-based reconnection via EventBus already implemented

**ESP8266-Specific Considerations**:
- `WiFi.persistent(false)` to avoid flash wear
- `WiFi.mode(WIFI_STA)` before connecting
- Handle `WIFI_DISCONNECT_REASON_*` codes for debugging

---

### 7. OTA Updates on ESP8266

**Decision**: Use existing OTAComponent - NO external library needed

**Rationale**:
- OTAComponent already uses Arduino's `Update` API with pluggable providers
- `Update.h` works identically on ESP32 and ESP8266
- `ESP.getFreeSketchSpace()` available on both
- No additional dependencies required

**Existing Code**:
```cpp
// OTA.h already has platform-agnostic providers
using Downloader = std::function<bool(const String& url, size_t& totalSize, DownloadCallback onChunk)>;
```

**Limitations**:
- No dual-OTA partition on most ESP8266 (single 4MB flash)
- Signature validation may be memory-constrained

---

### 8. PubSubClient MQTT

**Decision**: Use existing PubSubClient (already in project)

**Rationale**:
- Project already uses `PubSubClient ^2.8` (knolleary/PubSubClient)
- Already configured with 1024-byte buffer in MQTT.h
- Works identically on ESP32 and ESP8266
- No changes needed

**Existing Configuration**:
```cpp
#define MQTT_MAX_PACKET_SIZE 1024  // Already set in MQTT.h
```

---

### 9. Telnet/RemoteConsole on ESP8266

**Decision**: Use `WiFiServer` (already implemented)

**Rationale**:
- `WiFiServer` and `WiFiClient` work identically on ESP8266
- No library changes needed
- Buffer size already configurable in `RemoteConsoleConfig`

---

### 10. SystemInfo Metrics on ESP8266

**Decision**: Use ESP8266-specific APIs via separate implementation file

**Available Metrics**:
| Metric | ESP8266 API | Notes |
|--------|-------------|-------|
| Free Heap | `ESP.getFreeHeap()` | ✅ Works |
| Heap Fragmentation | `ESP.getHeapFragmentation()` | ✅ ESP8266 only |
| Chip ID | `ESP.getChipId()` | ✅ Different format than ESP32 |
| Flash Size | `ESP.getFlashChipSize()` | ✅ Works |
| CPU Freq | `ESP.getCpuFreqMHz()` | ✅ Works |
| Reset Reason | `ESP.getResetReason()` | ✅ String, not enum |

**Not Available**:
- `ESP.getChipRevision()` - Returns 0 on ESP8266
- PSRAM metrics - ESP8266 has no PSRAM

---

### 11. HAL Architecture: Separate Implementation Files

**Decision**: Split HAL into routing header + platform-specific implementation files

**Rationale**:
- Future platforms planned: ESP32-C3, ESP32-H2, and others
- Multiple platform files will be easier to maintain than growing `#if` blocks
- Better testability with injectable implementations
- Clearer separation of concerns

**New Architecture**:
```
ComponentName_HAL.h       → Routing header (includes platform impl)
ComponentName_ESP32.h     → ESP32 implementation
ComponentName_ESP8266.h   → ESP8266 implementation
ComponentName_ESP32C3.h   → Future: ESP32-C3 implementation
ComponentName_ESP32H2.h   → Future: ESP32-H2 implementation
```

**Example Pattern**:
```cpp
// SystemInfo_HAL.h (routing header)
#pragma once

#if DOMOTICS_PLATFORM_ESP32
    #include "SystemInfo_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "SystemInfo_ESP8266.h"
#else
    #include "SystemInfo_Stub.h"
#endif
```

**Migration Strategy**:
- Create new `*_ESP8266.h` files for each HAL
- Extract ESP8266-specific code from existing HAL files
- Update routing headers to include platform-specific files
- Preserve ESP32 code in `*_ESP32.h` files

---

### 12. HomeAssistant Discovery

**Decision**: No changes needed, depends only on MQTT

**Rationale**:
- HA discovery uses MQTT pub/sub only
- JSON payloads fit within 2KB limit
- Entity types (sensor, switch, etc.) work identically

---

---

### 13. Native Unit Tests with Unity Framework

**Decision**: Use Unity framework for cross-platform native tests

**Rationale**:
- PlatformIO natively supports Unity for native testing
- Tests run on host machine without hardware
- Enables TDD workflow and CI integration
- Catches logic errors before flashing

**Configuration** (`platformio.ini`):
```ini
[env:native]
platform = native
test_framework = unity
test_build_src = true
lib_compat_mode = off
build_flags =
    -std=gnu++17
    -DCORE_DEBUG_LEVEL=3
    -I../DomoticsCore-Core/include
    -I../DomoticsCore-{Component}/include
lib_deps =
    file://../DomoticsCore-Core
    file://../DomoticsCore-{Component}
    bblanchon/ArduinoJson@^7.0.0
```

**Test File Structure**:
```
DomoticsCore-{Component}/
├── platformio.ini              # Contains [env:native]
├── test/
│   └── test_{component}/
│       └── test_{component}.cpp
```

**Unity Test Pattern**:
```cpp
#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/{Component}.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

void setUp() {}
void tearDown() {}

void test_component_creation_default() {
    ComponentClass comp;
    TEST_ASSERT_EQUAL_STRING("ComponentName", comp.metadata.name.c_str());
}

void test_component_config_defaults() {
    ComponentConfig cfg;
    TEST_ASSERT_TRUE(cfg.enabled);
    TEST_ASSERT_EQUAL_UINT32(3600, cfg.interval);
}

void test_component_lifecycle() {
    Core core;
    auto comp = std::make_unique<ComponentClass>();
    core.addComponent(std::move(comp));
    TEST_ASSERT_TRUE(core.begin());
    core.shutdown();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_component_creation_default);
    RUN_TEST(test_component_config_defaults);
    RUN_TEST(test_component_lifecycle);
    return UNITY_END();
}
```

**Running Tests**:
```bash
cd DomoticsCore-{Component}
pio test -e native
```

**Key Points**:
- Use `HAL::Platform::getMillis()` instead of `millis()` for native compatibility
- Use `HAL::Platform::delayMs()` instead of `delay()`
- Event constants: use `{Component}Events::EVENT_NAME` (defined in `{Component}Events.h`)
- Use `static constexpr` (not `inline constexpr`) for C++14 compatibility on ESP32

---

### 14. ESP8266 WebUI Memory Optimization

**Problem**: OOM crashes on ESP8266 when WebUI schema is built (only ~25KB free heap)

**Root Cause**: `getWebUIContexts()` creates many temporary String allocations:
- Each `WebUIContext` contains multiple `WebUIField` objects
- Each `WebUIField::addOption()` allocates 2 Strings
- Original NTPWebUI had 29 timezone options = 58 String allocations
- WebSocket schema serialization requires additional JSON buffer

**Solutions Applied**:

| Optimization | Savings | Impact |
|--------------|---------|--------|
| Constexpr timezone lookup table | ~3-4KB heap | Data in flash, not heap |
| Reduce timezone options 29→8 | ~2KB runtime | Fewer addOption() calls |
| Remove detail context | ~1KB runtime | Less JSON to serialize |

**Constexpr Pattern** (moves data to flash):
```cpp
struct TimezoneLookupEntry {
    const char* posix;
    const char* friendly;
};

static constexpr TimezoneLookupEntry TIMEZONE_LOOKUP[] = {
    {"UTC0", "UTC"},
    {"CET-1CEST,M3.5.0,M10.5.0/3", "Paris/Berlin"},
    // ... more entries
};

String getTimezoneFriendlyName(const String& posixTz) const {
    for (size_t i = 0; i < TIMEZONE_LOOKUP_COUNT; ++i) {
        if (posixTz == TIMEZONE_LOOKUP[i].posix) {
            return String(TIMEZONE_LOOKUP[i].friendly);
        }
    }
    return posixTz;  // Fallback
}
```

**Memory Budget for WebUI Providers**:
- Max contexts per provider: 3-4
- Max fields per context: 4-5
- Max Select options: 8-10
- Avoid runtime String concatenation in `getWebUIContexts()`

**Constitution Compliance**:
- Do NOT use `#ifdef ESP8266` in non-HAL code
- Use same code path for all platforms
- Reduce complexity uniformly, not conditionally

---

### 15. C++14 vs C++17 Compatibility

**Problem**: Event header `inline constexpr` warnings on ESP32 (requires C++17)

**Decision**: Use `static constexpr` for event constants (C++14 compatible)

**Rationale**:
- ESP32 uses `-std=gnu++14` (framework limitation)
- ESP8266 framework uses C++17 internally
- `static constexpr` works on both

**Pattern**:
```cpp
// ✅ Correct (C++14 compatible)
namespace {Component}Events {
    static constexpr const char* EVENT_READY = "component/ready";
    static constexpr const char* EVENT_ERROR = "component/error";
}

// ❌ Avoid (requires C++17)
namespace {Component}Events {
    inline constexpr const char* EVENT_READY = "component/ready";
}
```

---

### 16. time_t Portability

**Problem**: `%ld` format warning for `time_t` on ESP8266

**Root Cause**:
- ESP32: `time_t` is `long` (32-bit)
- ESP8266: `time_t` is `long long` (64-bit)

**Solution**: Cast to `long long` and use `%lld`:
```cpp
DLOG_I(LOG_APP, "Unix: %lld", (long long)ntp->getUnixTime());
```

---

## Summary

All technical unknowns resolved. No NEEDS CLARIFICATION items remain.

| Topic | Decision | Risk Level |
|-------|----------|------------|
| C++ Version | ESP32=C++14, ESP8266=C++17 (framework default) | Low |
| WebUI Library | ESP32Async/ESPAsyncWebServer (unified) | Low |
| Storage | LittleFS only | Low |
| Core Version | >= 3.0.0 | Low |
| RAM Strategy | Per-component budgets + tiered response | Medium |
| WiFi | Existing HAL | Low |
| OTA | Existing OTAComponent (no new lib) | Low |
| MQTT | Existing PubSubClient ^2.8 | None |
| Telnet | WiFiServer | Low |
| SystemInfo | ESP8266-specific APIs | Low |
| HAL Architecture | Separate files per platform | Low |
| HomeAssistant | No changes | Low |
| Native Tests | Unity framework, HAL stubs | Low |
| WebUI Memory | Constexpr tables, reduced options | Medium |
| Event Constants | `static constexpr` (C++14) | Low |
| time_t Format | Cast to `long long`, use `%lld` | Low |

---

*Last updated*: 2025-12-23
