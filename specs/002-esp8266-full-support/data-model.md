# Data Model: ESP8266 Full Support

**Feature**: 002-esp8266-full-support  
**Date**: 2025-12-18  
**Purpose**: Document component structure and HAL abstractions for ESP8266

## Component Architecture

### Validation Phases

```
Phase 1: Core ──┬── Phase 2: LED
                └── Phase 3: Storage ──┬── Phase 4: WiFi ──┬── Phase 5: NTP
                                       │                   ├── Phase 7: RemoteConsole
                                       │                   ├── Phase 9: OTA
                                       │                   └── Phase 11: WebUI
                                       │
                                       └── Phase 8: MQTT ──── Phase 10: HomeAssistant
                                       
Phase 6: SystemInfo (Core only)
Phase 12: System (All)
```

## HAL Architecture (Separate Files per Platform)

### File Structure Pattern

Each HAL component follows this pattern:

```
ComponentName_HAL.h       → Routing header (platform detection + include)
ComponentName_ESP32.h     → ESP32 implementation
ComponentName_ESP8266.h   → ESP8266 implementation
ComponentName_ESP32C3.h   → Future: ESP32-C3
ComponentName_ESP32H2.h   → Future: ESP32-H2
ComponentName_Stub.h      → Fallback for unsupported platforms
```

### Routing Header Example

```cpp
// SystemInfo_HAL.h
#pragma once
#include "DomoticsCore/Platform_HAL.h"

#if DOMOTICS_PLATFORM_ESP32
    #include "SystemInfo_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "SystemInfo_ESP8266.h"
#elif DOMOTICS_PLATFORM_ESP32C3
    #include "SystemInfo_ESP32C3.h"
#elif DOMOTICS_PLATFORM_ESP32H2
    #include "SystemInfo_ESP32H2.h"
#else
    #include "SystemInfo_Stub.h"
#endif
```

## HAL Entities

### Platform_HAL

**Location**: `DomoticsCore-Core/include/DomoticsCore/Platform_HAL.h` (routing)  
**ESP8266 Impl**: `DomoticsCore-Core/include/DomoticsCore/Platform_ESP8266.h` (NEW)

| Macro | ESP8266 Value | Description |
|-------|---------------|-------------|
| `DOMOTICS_PLATFORM_ESP8266` | 1 | Platform detection |
| `DOMOTICS_PLATFORM_NAME` | "ESP8266" | Platform string |
| `DOMOTICS_HAS_WIFI` | 1 | WiFi available |
| `DOMOTICS_HAS_PREFERENCES` | 0 | No Preferences.h |
| `DOMOTICS_HAS_FREERTOS` | 0 | No FreeRTOS |
| `DOMOTICS_HAS_ASYNC_TCP` | 0 | Different library |
| `DOMOTICS_HAS_SNTP` | 0 | Use configTime() |
| `DOMOTICS_HAS_OTA` | 1 | OTA available |
| `DOMOTICS_HAS_SPIFFS` | 1 | SPIFFS available |
| `DOMOTICS_HAS_LITTLEFS` | 1 | LittleFS available |
| `DOMOTICS_RAM_SIZE_KB` | 80 | RAM size |
| `DOMOTICS_FLASH_SIZE_KB` | 4096 | Flash size |

---

### Storage_HAL

**Location**: `DomoticsCore-Storage/include/DomoticsCore/Storage_HAL.h`

**ESP8266 Implementation**: `LittleFSStorage`

| Method | Description | ESP8266 Notes |
|--------|-------------|---------------|
| `begin(namespace)` | Open storage | Creates `/namespace.json` |
| `end()` | Close and save | Flushes to LittleFS |
| `putString/getString` | String storage | JSON serialization |
| `putInt/getInt` | Integer storage | JSON serialization |
| `putBool/getBool` | Boolean storage | JSON serialization |
| `putFloat/getFloat` | Float storage | JSON serialization |
| `putBytes/getBytes` | Binary storage | Hex-encoded in JSON |
| `remove(key)` | Delete key | Removes from JSON |
| `clear()` | Clear all | Empties JSON document |

**Constraints**:
- JSON document max size: 2KB
- Immediate save on each write for reliability
- LittleFS initialized once on first `begin()`

---

### Wifi_HAL

**Location**: `DomoticsCore-Wifi/include/DomoticsCore/Wifi_HAL.h`

**ESP8266 Header**: `#include <ESP8266WiFi.h>`

| Function | ESP8266 API | Notes |
|----------|-------------|-------|
| `setMode(mode)` | `WiFi.mode()` | WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA |
| `connect(ssid, pass)` | `WiFi.begin()` | Non-blocking |
| `disconnect()` | `WiFi.disconnect()` | - |
| `isConnected()` | `WiFi.isConnected()` | - |
| `getIP()` | `WiFi.localIP()` | - |
| `getMACAddress()` | `WiFi.macAddress()` | - |
| `getSSID()` | `WiFi.SSID()` | - |
| `getRSSI()` | `WiFi.RSSI()` | - |

---

### NTP_HAL

**Location**: `DomoticsCore-NTP/include/DomoticsCore/NTP_HAL.h`

**ESP8266 API**: `configTime(gmtOffset, daylightOffset, server1, server2, server3)`

| Function | ESP8266 Implementation |
|----------|------------------------|
| `init(servers...)` | `configTime(0, 0, server1, server2, server3)` |
| `isSynced()` | `time(nullptr) > 1000000000` |
| `getTime()` | `time(nullptr)` |
| `getLocalTime(tm*)` | `localtime_r()` |

---

### SystemInfo_HAL

**Location**: `DomoticsCore-SystemInfo/include/DomoticsCore/SystemInfo_HAL.h`

| Function | ESP8266 API | Returns |
|----------|-------------|---------|
| `getFreeHeap()` | `ESP.getFreeHeap()` | Bytes |
| `getTotalHeap()` | N/A | 0 (not available) |
| `getMinFreeHeap()` | N/A | 0 (not available) |
| `getHeapFragmentation()` | `ESP.getHeapFragmentation()` | Percentage |
| `getChipId()` | `ESP.getChipId()` | uint32_t |
| `getChipModel()` | N/A | "ESP8266" |
| `getChipRevision()` | N/A | 0 |
| `getCpuFreqMHz()` | `ESP.getCpuFreqMHz()` | 80 or 160 |
| `getFlashChipSize()` | `ESP.getFlashChipSize()` | Bytes |
| `getResetReason()` | `ESP.getResetReason()` | String |

---

## Component RAM Budgets

| Component | Budget | Cumulative | Remaining (80KB) |
|-----------|--------|------------|------------------|
| Core | Baseline (~15KB) | 15KB | 65KB |
| LED | +1KB | 16KB | 64KB |
| Storage | +2KB | 18KB | 62KB |
| WiFi | +5KB | 23KB | 57KB |
| NTP | +1KB | 24KB | 56KB |
| SystemInfo | +1KB | 25KB | 55KB |
| RemoteConsole | +3KB | 28KB | 52KB |
| MQTT | +5KB | 33KB | 47KB |
| OTA | +2KB | 35KB | 45KB |
| HomeAssistant | +3KB | 38KB | 42KB |
| WebUI | +15KB | 53KB | 27KB |
| System overhead | +5KB | 58KB | 22KB |

**Target**: ≥20KB free after full stack initialization ✅

---

## Low-Heap Response State Machine

```
                    ┌─────────────┐
                    │   NORMAL    │ heap ≥ 15KB
                    │  (all on)   │
                    └──────┬──────┘
                           │ heap < 15KB
                           ▼
                    ┌─────────────┐
                    │   WARNING   │ Log warning
                    │ (reject new)│ Reject new connections
                    └──────┬──────┘
                           │ heap < 10KB
                           ▼
                    ┌─────────────┐
                    │  DEGRADED   │ Suspend WebUI
                    │ (WebUI off) │ Keep RemoteConsole + HA
                    └──────┬──────┘
                           │ heap < 5KB
                           ▼
                    ┌─────────────┐
                    │  CRITICAL   │ Trigger reboot
                    │  (reboot)   │
                    └─────────────┘
```

---

## Validation Checklist Entity

| Field | Type | Description |
|-------|------|-------------|
| phase | int | 1-12 |
| component | string | Component name |
| compiles | bool | Compilation passes |
| functions | bool | Functionality verified |
| heap_budget | string | Expected heap increase |
| heap_actual | int | Measured heap increase |
| validated | bool | All checks pass |
| validated_date | date | When validated |
| notes | string | Issues or observations |
