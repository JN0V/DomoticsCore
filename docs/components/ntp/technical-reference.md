# DomoticsCore-NTP Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

---

## Table of Contents

1. [NTPConfig](#ntpconfig)
2. [NTPStatistics](#ntpstatistics)
3. [Timezone Presets](#timezone-presets)
4. [NTPComponent](#ntpcomponent)
   - [Constructor and Destructor](#constructor-and-destructor)
   - [IComponent Lifecycle](#icomponent-lifecycle)
   - [Time Synchronization](#time-synchronization)
   - [Time Access](#time-access)
   - [Uptime](#uptime)
   - [Timezone Management](#timezone-management)
   - [Configuration](#configuration)
   - [Callbacks](#callbacks)
   - [Statistics](#statistics)
5. [NTPEvents](#ntpevents)
6. [NTP HAL](#ntp-hal)
7. [NTPWebUI Provider](#ntpwebui-provider)
   - [WebUI Contexts](#webui-contexts)
   - [API Endpoints](#api-endpoints)
   - [Timezone Lookup Table](#timezone-lookup-table)
   - [Configuration Persistence](#configuration-persistence)
8. [Sync Mechanism Internals](#sync-mechanism-internals)
9. [Formatted Time Reference](#formatted-time-reference)
10. [Platform Support Matrix](#platform-support-matrix)

---

## NTPConfig

Defined in `DomoticsCore/NTP.h`. Namespace: `DomoticsCore::Components`.

```cpp
struct NTPConfig {
    bool enabled = true;
    std::vector<String> servers = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    uint32_t syncInterval = 3600;    // seconds (1 hour)
    String timezone = "UTC0";         // POSIX TZ string
    uint32_t timeoutMs = 5000;       // Sync attempt timeout
    uint32_t retryDelayMs = 5000;    // Retry delay on failure (declared but NOT enforced -- see warning below)
};
```

| Field | Type | Default | Description |
|---|---|---|---|
| `enabled` | `bool` | `true` | Enable or disable NTP synchronization entirely. |
| `servers` | `std::vector<String>` | `{"pool.ntp.org", "time.google.com", "time.cloudflare.com"}` | Up to three NTP server hostnames. Only the first three are used by the HAL. |
| `syncInterval` | `uint32_t` | `3600` | Automatic re-sync interval in seconds. Passed to the HAL in milliseconds. |
| `timezone` | `String` | `"UTC0"` | POSIX TZ string controlling local time and DST rules. |
| `timeoutMs` | `uint32_t` | `5000` | Maximum time in milliseconds to wait for a sync response before declaring failure. |
| `retryDelayMs` | `uint32_t` | `5000` | Delay before retrying after a failed sync attempt. **Warning: this field is declared in `NTPConfig` but is not currently read or enforced by `NTPComponent`. After a sync timeout, no automatic retry is scheduled using this value. The field is retained for forward compatibility but has no runtime effect as of v1.3.0.** |

---

## NTPStatistics

Defined in `DomoticsCore/NTP.h`. Namespace: `DomoticsCore::Components`.

```cpp
struct NTPStatistics {
    uint32_t syncCount = 0;
    uint32_t syncErrors = 0;
    time_t lastSyncTime = 0;
    uint32_t lastSyncDuration = 0;    // milliseconds
    time_t lastFailTime = 0;
    uint32_t consecutiveFailures = 0;
};
```

| Field | Type | Description |
|---|---|---|
| `syncCount` | `uint32_t` | Total number of successful time synchronizations since boot. |
| `syncErrors` | `uint32_t` | Total number of sync timeouts or failures since boot. |
| `lastSyncTime` | `time_t` | Unix timestamp of the most recent successful sync. |
| `lastSyncDuration` | `uint32_t` | Duration of the most recent successful sync in milliseconds. |
| `lastFailTime` | `time_t` | Unix timestamp of the most recent failed sync attempt. |
| `consecutiveFailures` | `uint32_t` | Number of consecutive failures. Reset to zero on any successful sync. |

---

## Timezone Presets

Defined in `DomoticsCore/NTP.h`. Namespace: `DomoticsCore::Components::Timezones`.

All values are `constexpr const char*` suitable for passing to `NTPConfig::timezone` or `NTPComponent::setTimezone()`.

| Constant | POSIX TZ String | Region |
|---|---|---|
| `Timezones::UTC` | `"UTC0"` | Coordinated Universal Time |
| `Timezones::EST` | `"EST5EDT,M3.2.0,M11.1.0"` | US Eastern |
| `Timezones::CST` | `"CST6CDT,M3.2.0,M11.1.0"` | US Central |
| `Timezones::MST` | `"MST7MDT,M3.2.0,M11.1.0"` | US Mountain |
| `Timezones::PST` | `"PST8PDT,M3.2.0,M11.1.0"` | US Pacific |
| `Timezones::CET` | `"CET-1CEST,M3.5.0,M10.5.0/3"` | Central European |
| `Timezones::GMT` | `"GMT0"` | Greenwich Mean Time |
| `Timezones::JST` | `"JST-9"` | Japan Standard Time |
| `Timezones::AEST` | `"AEST-10AEDT,M10.1.0,M4.1.0/3"` | Australia Eastern |
| `Timezones::IST` | `"IST-5:30"` | India Standard Time |
| `Timezones::NZST` | `"NZST-12NZDT,M9.5.0,M4.1.0/3"` | New Zealand Standard Time |

### Custom POSIX TZ Format

The general format is: `STDoffset[DST[offset],start[/time],end[/time]]`

- **STD**: Standard time abbreviation (e.g., `CET`).
- **offset**: Hours west of UTC. Negative values mean east of UTC (note: this is the opposite of common convention).
- **DST**: Daylight saving abbreviation (e.g., `CEST`).
- **start/end**: DST transition rules using `M<month>.<week>.<day>` notation.

Example: `CET-1CEST,M3.5.0,M10.5.0/3` means CET is UTC+1, CEST starts on the last Sunday of March, and ends on the last Sunday of October at 03:00.

---

## NTPComponent

Defined in `DomoticsCore/NTP.h`. Namespace: `DomoticsCore::Components`.

Inherits from `IComponent`. Registered name: `"NTP"`. Current version: `"1.3.0"`.

### Constructor and Destructor

```cpp
explicit NTPComponent(const NTPConfig& cfg = NTPConfig());
virtual ~NTPComponent();
```

- **Constructor**: Initializes component metadata (`name = "NTP"`, `version = "1.3.0"`), stores the configuration, records boot time, and prepares the sync timeout timer in a disabled state.
- **Destructor**: Calls `HAL::NTP::stop()` if the component is enabled, ensuring the SNTP client is shut down cleanly.

### IComponent Lifecycle

#### `ComponentStatus begin()`

Starts the NTP client. Steps performed:

1. If `config.enabled` is `false`, returns `ComponentStatus::Success` immediately.
2. Sets the timezone via `HAL::NTP::setTimezone()`.
3. Passes up to three server hostnames to `HAL::NTP::init()`.
4. Sets the sync interval via `HAL::NTP::setSyncInterval()`.

Returns `ComponentStatus::Success` in all cases.

#### `void loop()`

Called every main loop iteration. Performs two checks:

1. **Sync detection**: Reads `time(nullptr)` and considers the clock synced when the value exceeds `1000000000` (approximately 2001-09-09). On the first sync detection, increments `syncCount`, records the timestamp, emits `NTPEvents::EVENT_SYNCED`, and invokes the sync callback with `true`.
2. **Timeout detection**: If a sync is in progress and the `syncTimeoutTimer` fires, increments `syncErrors` and `consecutiveFailures`, emits `NTPEvents::EVENT_SYNC_FAILED`, and invokes the sync callback with `false`.

#### `ComponentStatus shutdown()`

Stops the SNTP client via `HAL::NTP::stop()` if enabled. Returns `ComponentStatus::Success`.

### Time Synchronization

#### `bool syncNow()`

Triggers an immediate NTP synchronization request.

- Returns `false` if the component is disabled or a sync is already in progress.
- Returns `true` after requesting a non-blocking sync via `HAL::NTP::forceSync()`.
- Starts the timeout timer with `config.timeoutMs`.

#### `bool isSynced() const`

Returns `true` if time has been successfully synchronized at least once and the current system time is greater than `1000000000`.

#### `time_t getLastSyncTime() const`

Returns the Unix timestamp of the most recent successful synchronization.

#### `uint32_t getNextSyncIn() const`

Returns the number of seconds remaining until the next automatic synchronization. Returns `0` if not yet synced or if the component is disabled.

### Time Access

#### `time_t getUnixTime() const`

Returns the current Unix timestamp (seconds since 1970-01-01 00:00:00 UTC).

#### `struct tm getLocalTime() const`

Returns the current local time as a `struct tm`, adjusted for the configured timezone and DST rules.

#### `String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S") const`

Returns a formatted time string using `strftime`. Returns `"Not synced"` if time has not been synchronized. See [Formatted Time Reference](#formatted-time-reference) for format specifiers.

#### `String getISO8601() const`

Returns the current time in ISO 8601 format with timezone offset, e.g., `"2025-10-02T19:30:45+02:00"`. Returns `"Not synced"` if time has not been synchronized.

### Uptime

#### `uint64_t getUptimeMs() const`

Returns the number of milliseconds elapsed since the component was constructed (approximately since device boot).

#### `String getFormattedUptime() const`

Returns a human-readable uptime string in the format `"2d 5h 32m 15s"`. Only includes non-zero higher units (e.g., `"45m 12s"` when uptime is under one hour).

### Timezone Management

#### `void setTimezone(const String& tz)`

Sets the timezone using a POSIX TZ string. Takes effect immediately by calling `HAL::NTP::setTimezone()`, which internally runs `setenv("TZ", tz, 1)` and `tzset()`.

#### `String getTimezone() const`

Returns the currently configured POSIX timezone string.

#### `int getGMTOffset() const`

Returns the current offset from GMT in seconds. Positive values indicate east of GMT, negative values indicate west. This value accounts for DST if currently active.

#### `bool isDST() const`

Returns `true` if Daylight Saving Time is currently active according to the configured timezone rules.

### Configuration

#### `const NTPConfig& getConfig() const`

Returns a const reference to the current `NTPConfig`.

#### `void setConfig(const NTPConfig& cfg)`

Applies a new configuration. Behavior:

1. Computes diffs between old and new config **before** mutating state.
2. If `timezone` changed, calls `setTimezone()` immediately.
3. If `enabled`, `servers`, or `syncInterval` changed and the component is enabled, stops and restarts the SNTP client.

### Callbacks

#### `void onSync(SyncCallback callback)`

Registers a callback invoked after each sync attempt.

```cpp
using SyncCallback = std::function<void(bool success)>;
```

The `success` parameter is `true` on successful synchronization and `false` on timeout.

### Statistics

#### `const NTPStatistics& getStatistics() const`

Returns a const reference to the current `NTPStatistics` structure. See [NTPStatistics](#ntpstatistics) for field descriptions.

---

## NTPEvents

Defined in `DomoticsCore/NTPEvents.h`. Namespace: `DomoticsCore::NTPEvents`.

| Constant | Value | Emitted When |
|---|---|---|
| `EVENT_SYNCED` | `"ntp/synced"` | Time is successfully synchronized (initial or subsequent). |
| `EVENT_SYNC_FAILED` | `"ntp/sync_failed"` | A sync attempt times out without receiving a response. |

These events are emitted via the `IComponent::emit()` mechanism, allowing other components to subscribe through the EventBus without direct coupling.

---

## NTP HAL

Defined in `DomoticsCore/NTP_HAL.h` (routing header). Namespace: `DomoticsCore::HAL::NTP`.

The HAL provides a platform-independent interface. The routing header selects the correct implementation at compile time:

- `NTP_ESP32.h` -- uses the `esp_sntp` API.
- `NTP_ESP8266.h` -- uses `configTime()` and the `sntp` library.
- `NTP_Stub.h` -- no-op stubs for native/test environments.

### HAL Functions

| Function | Signature | Description |
|---|---|---|
| `init` | `void init(const char* server1, const char* server2, const char* server3)` | Initialize the SNTP client with up to three servers. |
| `setTimezone` | `void setTimezone(const char* tz)` | Set the POSIX timezone via `setenv("TZ", ...)` and `tzset()`. |
| `setSyncInterval` | `void setSyncInterval(uint32_t intervalMs)` | Set the automatic re-sync interval. No effect on ESP8266. |
| `stop` | `void stop()` | Stop the SNTP client. |
| `forceSync` | `void forceSync()` | Request an immediate resynchronization. |
| `isSynced` | `bool isSynced()` | Returns `true` if `time(nullptr)` exceeds 2020-01-01 UTC. |
| `getTime` | `time_t getTime()` | Returns current `time(nullptr)`. |
| `getFormattedTime` | `bool getFormattedTime(const char* format, char* buffer, size_t bufferSize)` | Format current local time into buffer. Returns `false` if not synced. |

### Platform Notes

**ESP32**: `forceSync()` calls `sntp_restart()`, which reinitializes the SNTP client and triggers an immediate poll. `setSyncInterval()` calls `sntp_set_sync_interval()`.

**ESP8266**: `forceSync()` stops and reinitializes SNTP (`sntp_stop()` + `sntp_init()`). `setSyncInterval()` is a no-op because the ESP8266 SNTP library does not expose interval control.

**Stub**: All functions are no-ops, suitable for native unit testing.

---

## NTPWebUI Provider

Defined in `DomoticsCore/NTPWebUI.h`. Namespace: `DomoticsCore::Components::WebUI`.

`NTPWebUI` extends `CachingWebUIProvider` and provides a web interface for the NTP component. It holds a non-owning pointer to `NTPComponent`.

### Constructor

```cpp
explicit NTPWebUI(NTPComponent* ntp);
```

### Initialization

```cpp
void init(WebUIComponent* webui);
```

Must be called after the WebUI component is ready. Registers the `/api/ntp/timezones` endpoint.

### WebUI Contexts

`NTPWebUI` exposes two contexts:

| Context ID | Type | Description | Update Rate |
|---|---|---|---|
| `ntp_time` | Header Info | Displays current time and date in the header bar. | Real-time, 1000 ms |
| `ntp_settings` | Settings Card | NTP configuration: enabled, servers, sync interval, timezone. | On change |

#### `ntp_time` Fields

| Field | Type | Description |
|---|---|---|
| `time` | Display | Current time in `HH:MM:SS` format, or `"--:--:--"` if not synced. |
| `date` | Display | Current date in `YYYY-MM-DD` format, or empty if not synced. |

#### `ntp_settings` Fields

| Field | Type | Description |
|---|---|---|
| `enabled` | Boolean | Enable or disable NTP synchronization. |
| `servers` | Text | Comma-separated list of NTP server hostnames. |
| `sync_interval` | Number | Synchronization interval in hours. |
| `timezone` | Select | Timezone selector; options loaded from `/api/ntp/timezones`. |

### API Endpoints

#### `GET /api/ntp/timezones`

Returns a JSON array of timezone options streamed from the flash-stored constexpr lookup table:

```json
[
  {"value": "UTC0", "label": "UTC"},
  {"value": "CET-1CEST,M3.5.0,M10.5.0/3", "label": "Paris (CET)"},
  ...
]
```

#### `GET /api/ntp/time`

Returns the current time and date data for the `ntp_time` context.

#### `POST /api/ntp/settings`

Accepts field-by-field configuration updates via `field` and `value` parameters:

| field | Expected value | Effect |
|---|---|---|
| `enabled` | `"true"` or `"false"` | Enables or disables NTP. |
| `servers` | Comma-separated hostnames | Updates the server list. |
| `sync_interval` | Integer (hours) | Sets sync interval (converted to seconds internally). |
| `timezone` | POSIX TZ string | Changes the timezone immediately. |

After any successful settings update, a `syncNow()` is triggered automatically if NTP is enabled. If a config save callback is registered, it is also invoked.

### Timezone Lookup Table

The `TIMEZONE_LOOKUP` constexpr array contains 29 entries mapping POSIX TZ strings to user-friendly city/region names. It is stored in flash (not RAM) to conserve approximately 3-4 KB of heap on ESP8266.

The lookup uses O(n) linear search, which is acceptable for the 29-entry dataset.

### Configuration Persistence

```cpp
void setConfigSaveCallback(std::function<void(const NTPConfig&)> callback);
```

Register a callback that is invoked whenever the configuration is changed through the WebUI. Typically used to persist settings via the Storage component.

### Change Detection

`NTPWebUI` implements `hasDataChanged()` using the `LazyState<T>` helper:

- `ntp_time`: Tracks `getUnixTime()` -- changes every second.
- `ntp_settings`: Tracks a struct of `{enabled, timezone}` -- changes only on configuration updates.

This prevents unnecessary JSON serialization when the WebUI polls for updates.

---

## Sync Mechanism Internals

The synchronization lifecycle within `loop()` follows this flow:

1. **Idle state**: `syncInProgress == false`. The HAL SNTP client runs in the background at the configured interval.
2. **Manual trigger**: `syncNow()` sets `syncInProgress = true`, starts the `syncTimeoutTimer`, and calls `HAL::NTP::forceSync()`.
3. **Sync detected**: On each `loop()` call, the component reads `time(nullptr)`. If the value exceeds `1000000000` and has changed since the last recorded sync, it records success, resets `consecutiveFailures`, emits `EVENT_SYNCED`, and invokes the callback.
4. **Timeout**: If `syncTimeoutTimer.isReady()` fires while `syncInProgress` is still true, the sync is declared failed. `syncErrors` and `consecutiveFailures` increment, `EVENT_SYNC_FAILED` is emitted, and the callback is invoked with `false`.

The `syncTimeoutTimer` uses `Utils::NonBlockingDelay`, consistent with the constitution's non-blocking timer pattern (Principle X).

---

## Formatted Time Reference

The `getFormattedTime()` method accepts standard `strftime` format specifiers:

| Specifier | Output | Example |
|---|---|---|
| `%Y-%m-%d %H:%M:%S` | Full date and time | `2025-10-02 19:30:45` |
| `%Y/%m/%d` | Date with slashes | `2025/10/02` |
| `%H:%M` | 24-hour time | `19:30` |
| `%I:%M %p` | 12-hour time with AM/PM | `07:30 PM` |
| `%A, %B %d, %Y` | Full long date | `Thursday, October 02, 2025` |
| `%a %b %d` | Abbreviated date | `Thu Oct 02` |
| `%Y-W%W` | Year with week number | `2025-W40` |
| `%s` | Unix timestamp | `1727896245` |

The internal buffer is 128 bytes. Formats producing output exceeding this length will be truncated.

---

## Platform Support Matrix

| Platform | HAL File | SNTP Client | Sync Interval Control | Force Sync Mechanism |
|---|---|---|---|---|
| ESP32 | `NTP_ESP32.h` | `esp_sntp` API | `sntp_set_sync_interval()` | `sntp_restart()` |
| ESP8266 | `NTP_ESP8266.h` | `configTime()` / `sntp` | Not available | `sntp_stop()` + `sntp_init()` |
| Native/Test | `NTP_Stub.h` | None (stubs) | No-op | No-op |

---

## Performance Characteristics

| Metric | Value |
|---|---|
| RAM usage | ~2 KB |
| Flash usage | ~8 KB |
| Typical sync time | 1-5 seconds |
| CPU impact | Minimal (SNTP runs in background) |
| `loop()` overhead | Negligible (two `time()` calls + timer check) |
