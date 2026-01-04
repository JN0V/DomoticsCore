# ESP8266 Heap Measurements

**Purpose**: Document RAM and Flash usage for each component on ESP8266 (Wemos D1 Mini).

## Build Environment

- **Platform**: espressif8266
- **Board**: d1_mini
- **Framework**: Arduino
- **C++ Standard**: gnu++14
- **Total RAM**: 81920 bytes (80KB)
- **Total Flash**: 1044464 bytes (~1MB usable)

## Phase 2: HAL Refactoring (Baseline)

| Component | RAM Used | RAM % | Flash Used | Flash % | Notes |
|-----------|----------|-------|------------|---------|-------|
| Core Only | 33,844 | 41.3% | 279,547 | 26.8% | Baseline with EventBus, Timer |

## Phase 3: User Story 1 - Core Foundation

| Component | RAM Used | RAM % | Flash Used | Flash % | Heap Increase |
|-----------|----------|-------|------------|---------|---------------|
| Core | 33,844 | 41.3% | 279,547 | 26.8% | Baseline |
| + LED | 35,696 | 43.6% | 299,847 | 28.7% | +1,852 bytes |
| + Storage | 38,116 | 46.5% | 341,603 | 32.7% | +2,420 bytes |

## Phase 4: User Story 2 - Network Foundation

| Component | RAM Used | RAM % | Flash Used | Flash % | Heap Increase |
|-----------|----------|-------|------------|---------|---------------|
| WiFi | 38,396 | 46.9% | 308,307 | 29.5% | ~+4,500 bytes |
| NTP | 35,968 | 43.9% | 305,947 | 29.3% | ~+2,100 bytes |
| SystemInfo | 34,340 | 41.9% | 286,931 | 27.5% | ~+500 bytes |

## Phase 5: User Story 3 - Network Services

| Component | RAM Used | RAM % | Flash Used | Flash % | Heap Increase |
|-----------|----------|-------|------------|---------|---------------|
| RemoteConsole | 35,724 | 43.6% | 305,123 | 29.2% | ~+1,900 bytes |
| MQTT | 35,812 | 43.7% | 408,599 | 39.1% | ~+2,000 bytes |
| OTA | 35,448 | 43.3% | 309,079 | 29.6% | ~+1,600 bytes |
| HomeAssistant | 37,812 | 46.2% | 432,555 | 41.4% | ~+4,000 bytes |

## Phase 6: User Story 4 - WebUI

| Component | RAM Used | RAM % | Flash Used | Flash % | Heap Increase |
|-----------|----------|-------|------------|---------|---------------|
| WebUI (ESP32) | 54,708 | 16.7% | 970,069 | 74.0% | N/A |
| WebUI (ESP8266) | 50,796 | 62.0% | 442,955 | 42.4% | ✅ Working |

### ESP8266 WebUI Memory Optimization

**Problem**: OOM crash when WebSocket client connects (~25KB free heap insufficient)

**Root Cause**: `getWebUIContexts()` runtime allocations:
- NTPWebUI original: 29 timezone options × 2 strings = ~58 String allocations
- Each context/field creates heap objects
- WebSocket schema JSON serialization needs buffer space

**Optimizations Applied**:

| Change | RAM Impact |
|--------|------------|
| Constexpr timezone lookup table | Moved ~3KB from heap to flash |
| Reduced timezone options (29→8) | ~2KB less runtime allocs |
| Removed detail context | ~1KB less runtime allocs |

**Final NTPWebUI Structure** (ESP8266-safe):
- `ntp_time`: header (1 field)
- `ntp_dashboard`: time, date, timezone (3 fields)
- `ntp_settings`: enabled, servers, interval, timezone[8 options] (4 fields)
- ~~`ntp_detail`~~: removed to save memory

**Guidelines for WebUI Providers**:
- Max 3-4 contexts per provider
- Max 4-5 fields per context
- Max 8-10 Select options
- Use constexpr for static lookup data
- Avoid String concatenation in `getWebUIContexts()`

## Budget Compliance

| Component | Budget | Actual | Status |
|-----------|--------|--------|--------|
| Core | Baseline | 33,844 | ✅ |
| LED | ≤1KB | ~1.9KB | ⚠️ Slightly over |
| Storage | ≤2KB | ~2.4KB | ⚠️ Slightly over |
| WiFi | ≤5KB | ~4.5KB | ✅ |
| NTP | ≤1KB | ~2.1KB | ⚠️ Over |
| SystemInfo | ≤1KB | ~0.5KB | ✅ |
| RemoteConsole | ≤3KB | ~1.9KB | ✅ |
| MQTT | ≤5KB | ~2KB | ✅ |

**Note**: Measurements are from isolated component builds. Actual combined usage may differ due to shared code.

## Target: ≥20KB Free Heap

With 80KB total RAM:
- **Current usage (Phase 5)**: ~44% = 36KB
- **Free heap estimate**: ~44KB
- **Target met**: ✅ Yes (>20KB free)

---

*Last updated*: 2025-12-19
