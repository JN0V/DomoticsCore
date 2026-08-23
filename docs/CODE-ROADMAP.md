# DomoticsCore — Code Remediation Roadmap v2

> Generated from adversarial code review of all 12 components + root (2026-03-10).
> 13 parallel review agents — 196 total findings: 6 CRITICAL, 54 HIGH, 79 MEDIUM, 57 LOW.
> Priority order follows the constitution. Each item is a separate commit/PR.
>
> **Previous roadmap (v1, 2026-03-04 → 2026-03-09): ALL 10 PRIORITIES COMPLETE.** Archived at bottom.

---

## Delivery — how these items land

The remaining items are grouped into lots, one pull request per lot, rather than
landed one at a time. They come from a single oversized commit that mixed
unrelated concerns; splitting it by component is the point of the exercise.

No version bumps inside the lots — `library.json` versions and the CHANGELOG
move once, at the end, when the series is complete. That is why component
versions may sit still while fixes land.

| Lot | Items | State |
|---|---|---|
| **CI** | CI-1, CI-2 | **Merged** — PR #2, 2026-08-22 |
| **CI (deps + actions)** | CI-3, CI-5 | **Merged** — PR #6, 2026-08-22 |
| **NTP** | BUG-4, BUG-5, BUG-6 | **Merged** — PR #1 |
| OTA | SEC-3, DC-7, DC-6 | **Merged** — PR #4 |
| Storage | BUG-15, BUG-16, BUG-17, F1, F10 | **Merged** — PR #5 |
| Isolated — System | BUG-23 | **Merged** — PR #7, 2026-08-22 |
| Isolated — MQTT, RemoteConsole | BUG-8, BUG-22 | **Merged** — PR #10, 2026-08-23, landed before the `esp32-ethernet` sync so it merges once |
| ESP8266 on-device suites | CI-10 | **Merged** — PR #19, 2026-08-23, first run on real hardware; found STOR-ESP-1 |
| System | TEST-1, ARCH-3 | **Merged** — PR #18, 2026-08-23 |
| LED | BUG-19, DC-5, TEST-2, LO-11 | PR #17 — chosen for having no file in common with `esp32-ethernet` |

The first series closed with v2.1.0 and v2.1.1; the lots above resume from what
the roadmap still lists open. The no-version-bump rule still holds — component
versions and the CHANGELOG move once, when a series is ready to ship.

`main` requires six checks: `test-install`, `check-versions`,
`Unit tests (native)`, `Build esp32dev`, `Build esp8266dev`, `Build esp32c3`,
plus an up-to-date branch and resolved conversations.

### What CI proves, and what it does not

Worth knowing before a green tick is read for more than it is worth.

| | |
|---|---|
| ✅ | The 13 native projects run — 706 test cases, discovered from the tracked `platformio.ini` files rather than a hard-coded list |
| ✅ | The three declared targets compile: `esp32dev`, `esp8266dev`, `esp32c3`, via the FullStack example, the only one pulling all twelve components |
| ✅ | `library.json` versions agree with `metadata.version` |
| ✅ | The install-from-GitHub path builds **both** declared platforms — the only thing in CI that resolves through the root `library.json` rather than `file://` paths (CI-8) |
| ⚠️ | **The four on-device suites compile in CI, and nothing runs them.** No runner has a board (CI-10). Run them by hand: `cd DomoticsCore-Storage && pio test -e esp8266dev` |
| ❌ | **No test runs on hardware in CI.** A host build proves compilation, not behaviour on a board — STOR-ESP-1 is what that costs |

That last line is not a formality. BUG-4, the SNTP server-name use-after-free,
**compiles cleanly and passes the native suites**; it only shows itself on a
board, after a configuration change. It was found by reading the code. Native
tests and cross-compilation cover what breaks most often, not what costs most.

---

## Priority 1: Security (CRITICAL — OTA & WebUI)

Unenforced security configurations are the most dangerous class of defect — users believe they are protected when they are not.

### SEC-1 — OTA: security config fields never enforced [CRITICAL] — **DONE (2026-08-22, PR #4 and #9)**

- **Ref**: OTA-F1
- **File**: `DomoticsCore-OTA/include/DomoticsCore/OTA.h`
- **Problem**: `requireTLS`, `bearerToken`, `basicAuthUser`, `basicAuthPassword`, `rootCA`, and `signaturePublicKey` are declared in `OTAConfig` but **never checked** by any code path. A user setting `requireTLS = true` believes HTTPS is enforced — it isn't. Firmware can be replaced by any network client via plain HTTP.
- **Fixed** by the third of the options above: the fields are removed (PR #4,
  with DC-7) and the documentation now states plainly where transport security
  actually lives — in the fetcher and downloader callbacks the application
  installs (PR #9).
- **Both halves were needed.** Removing the fields without correcting the
  documentation left four documents still describing `requireTLS` as rejecting
  non-HTTPS URLs and `bearerToken` as authenticating uploads. A reader following
  them would have believed in a protection twice over: once from a field that did
  nothing, then from a field that no longer existed.
- **What replaces it**: the upload endpoints are genuinely authenticated (SEC-3),
  and SHA-256 integrity is verified with rollback on mismatch (SEC-2). Firmware
  signature verification remains unimplemented, and is now documented as such
  rather than advertised by a config field.

### SEC-2 — OTA: SHA256 failure doesn't rollback firmware [CRITICAL]

- **Ref**: OTA-F2
- **File**: `DomoticsCore-OTA/include/DomoticsCore/OTA.h`
- **Problem**: After `HAL::OTAUpdate::end(true)` succeeds and SHA256 verification fails, the code transitions to `State::Error` but does NOT call `HAL::OTAUpdate::abort()`. Corrupted firmware may persist in the OTA partition.
- **Fix**: Call `abort()` (or equivalent rollback) when SHA256 verification fails, before transitioning to Error state.

### SEC-3 — OTA: upload endpoint has no authentication [HIGH]

- **Ref**: OTA-F5
- **File**: `DomoticsCore-OTA` — `/api/ota/upload` route handler
- **Problem**: Any network client can POST firmware to the upload endpoint with zero authentication.
- **Fix**: Gate the upload handler behind the WebUI authentication system, or add a dedicated OTA token check.

### SEC-4 — RemoteConsole: no brute-force protection on auth [MEDIUM]

- **Ref**: RC-F5
- **Problem**: Plain-text password comparison with no rate-limiting. Attackable via rapid telnet reconnections.
- **Fix**: Add exponential backoff after N failed attempts, or IP-based lockout.

### SEC-5 — WebUI: GET for state-changing operations [MEDIUM]

- **Ref**: WEB-F9
- **File**: `WebUI.h` — `/api/ui/action` endpoint
- **Problem**: HTTP GET used for mutations (enable/disable, password changes). Passwords appear in query strings, browser history, and server logs.
- **Fix**: At minimum add `Cache-Control: no-store` and document the security trade-off. Ideally use POST for mutations.

### SEC-6 — WebUI: CORS `Access-Control-Allow-Origin: *` with auth enabled [MEDIUM]

- **Ref**: WEB-F10
- **File**: `WebUI.h:429`
- **Problem**: Any website can make authenticated API requests to the device (CSRF/data exfiltration). Especially dangerous when `enableAuth` is true.
- **Fix**: Restrict CORS origin when auth is enabled, or disable wildcard CORS entirely.

---

## Priority 2: Memory Safety (Constitution XIV — ABSOLUTE PRIORITY)

IoT devices run 24/7. Any memory leak eventually causes OOM crash.

### MEM-1 — Missing `shrink_to_fit()` across 6 components [HIGH]

Multiple `clear()`/`erase()` operations without `shrink_to_fit()`, violating Constitution XIV.

| Location | Container | Operation |
|----------|-----------|-----------|
| `ComponentRegistry.h:212,216,258` | `initializationOrder`, `components`, `listeners` | `erase(remove(...))` |
| `EventBus.h:146` | `lastByTopic[topic]` | `clear()` in `publishSticky` |
| `EventBus.h` (reset) | all subscription maps | `clear()` in `reset()` |
| `IWebUIProvider.h:632` | `cachedContexts_` | `clear()` in `invalidateContextCache()` |
| `RemoteConsole.h` | `clients`, `clientBuffers`, `clientAuthenticated`, `clientConnectTime` maps | `erase()` on disconnect |
| `Wifi.h` | `scanNetworks` result vector | never shrunk |
| `System.h:99` | `stateCallbacks` | unbounded `push_back()`, no size limit |

- **Refs**: CORE-F1, CORE-F5, CORE-F11, WEB-F4, RC-F8, RC-F9, WIFI-F4, SYS-F3
- **Fix**: Add `shrink_to_fit()` after every size-reducing operation. Add bounds check on `stateCallbacks` (max 8).

### STOR-ESP-1 — Storage: every write leaks ~122 bytes on ESP8266 [HIGH]

- **Filed**: 2026-08-23, from the first run of the on-device suites on real
  hardware (a D1 mini on `/dev/ttyUSB0`).
- **File**: `DomoticsCore-Storage/include/DomoticsCore/Storage_ESP8266.h`
- **Problem**: `LittleFSStorage` holds a member `JsonDocument doc` for the life
  of the component. Every `putX()` does `doc[key] = value` and every `remove()`
  does `doc.remove(key)`. Neither returns memory: ArduinoJson 7's pool grows and
  is never shrunk, so the heap falls on every write and never recovers.
- **Measured on the board**, 20 iterations each:

  | Pattern | Heap delta | Per operation |
  |---|---|---|
  | put + get + remove, 20 distinct keys | 3,856 B | 192 B |
  | put + get + remove, **one** key | 3,904 B | 195 B |
  | put only, **one** key overwritten | 2,448 B | 122 B |
  | open / use / close a namespace (×5) | 64 B | 12 B — passes |

- **It is per operation, not per key.** That distinction is the whole severity.
  Growth bounded by the number of distinct keys would plateau; this does not.
  One key rewritten twenty times costs as much as twenty different keys, so a
  device persisting a counter, a timestamp or a setpoint bleeds at a constant
  rate for as long as it runs.
- **What that means in service**: the board booted with 49,512 bytes free. At
  122 bytes per write, a single value written once a minute exhausts the heap in
  roughly seven hours, and sooner in practice since not all free heap is usable.
- **The put path is the larger half.** Writing without removing still leaks
  122 B; adding get and remove takes it to ~195 B. Whoever fixes this should
  start at `putString()` and friends, not at `remove()`.
- **ESP32 is unaffected** — it uses NVS through `Preferences`, not a JSON
  document held in RAM. This is specific to the ESP8266 backend, which is
  exactly the platform that can least afford it: 80 KB of RAM against 320 KB.
- **Not fixed here.** Three candidate directions, none of them a one-liner:
  `doc.shrinkToFit()` after each mutation (simplest, costs a reallocation per
  write); reloading the document per operation rather than holding it (slower,
  bounded); or moving the ESP8266 backend off a resident `JsonDocument`
  entirely.
- **Pinned meanwhile**: three tests in `test_heap_esp8266` **fail on hardware**,
  deliberately. They are the signal. CI does not run them — no runner has a
  board — so `main` stays green and the failure is visible only where it is
  real.

### MEM-2 — String concatenation in hot paths across 9 components [HIGH]

Constitution XIV bans `String` concatenation in loops and hot paths. Use `snprintf()` with static buffers.

| Component | Location | Hot path? | Description |
|-----------|----------|-----------|-------------|
| **HomeAssistant** | `HomeAssistant.h:150` | YES (every MQTT msg) | `String(ev.topic)` + `String(ev.payload)` in mqtt/message handler |
| **HomeAssistant** | `HomeAssistant.h:623-690` | YES | `topic.substring()` in handleCommand |
| **SystemInfo** | formatBytes/getFormattedUptime | YES (every 5s) | String concat in metrics formatting |
| **WiFi** | scan loop, `getDetailedStatus()` | Moderate | `summary += ...` in for loop |
| **LED** | `getLEDStatus()` | Moderate | 6+ String concats |
| **RemoteConsole** | `help` handler, telnet negotiation | Cold | Character-by-character String building |
| **NTP** | `getFormattedUptime()` | Cold | `result += String(days) + "d "...` |
| **OTA** | `transition()`, `broadcastProgress()` | Moderate | String concat + JsonDocument per broadcast |
| **System** | NTP server parsing/saving | Cold (boot) | String operations in loop |
| **Storage** | `dumpContents()` | Cold | `String +=` in loop |
| **WebUI** | BaseWebUIComponents methods | Cold (setup) | String concat in selectDropdown/radioGroup loops |

- **Refs**: HA-F1, HA-F6, SI-F1, WIFI-F2, WIFI-F3, LED-F3, RC-F1, RC-F2, NTP-F8, OTA-F8, OTA-F9, SYS-F8, SYS-F9, STOR-F8, WEB-F7
- **Fix**: Replace with `snprintf()` + stack buffers, or `String::reserve()` before loops. Priority: hot-path items first (HA, SystemInfo).

### MEM-3 — HomeAssistant entity String properties should be `char[]` [MEDIUM]

- **Ref**: HA-F5
- **File**: `HAEntity.h:28-32`
- **Problem**: 5 `String` members per entity (`id`, `name`, `component`, `icon`, `deviceClass`) fragment heap. With many entities, significant pressure on ESP8266.
- **Fix**: Convert to `char[]` fixed-size buffers, consistent with `HAConfig`/`HACommandEvent` approach.

### MEM-4 — WebUI: permanent static 8KB buffer [MEDIUM]

- **Ref**: WEB-F5
- **File**: `WebUI.h:60`
- **Problem**: `static char wsBuffer_[WEBUI_WS_BUFFER_SIZE]` (8KB ESP32, 1KB ESP8266) permanently allocated even when no clients connected.
- **Fix**: Lazy allocation on first use, or document rationale for permanent allocation.

---

## Priority 3: Code Safety & Critical Bugs

### BUG-1 — Core: `reinterpret_cast` without trivially_copyable check [HIGH]

- **Ref**: CORE-F4
- **File**: `EventBus.h:101-107`
- **Problem**: `publish()` template uses `reinterpret_cast` to byte-copy payloads. Publishing a `std::string` or `String` results in dangling pointers in the event queue.
- **Fix**: Add `static_assert(std::is_trivially_copyable<PayloadT>::value, ...)`.

### BUG-2 — Core: unsafe `static_cast` downcast [HIGH]

- **Ref**: CORE-F2
- **File**: `Core.h:104`
- **Problem**: `getComponent<T>()` uses `static_cast<T*>` with no runtime type check. Wrong type = undefined behavior.
- **Fix**: Add type-key verification before cast, or use `dynamic_cast` (if RTTI enabled).

### BUG-3 — Core: `removeCallback()` nukes ALL callbacks [MEDIUM]

- **Ref**: CORE-F6
- **Problem**: `LoggerCallbacks::removeCallback()` calls `getCallbacks().clear()` instead of removing the specific callback.
- **Fix**: Find and erase only the target callback.

### BUG-4 — NTP: use-after-free with `sntp_setservername` pointers [HIGH]

- **Ref**: NTP-F4
- **File**: NTP HAL implementation
- **Problem**: `sntp_setservername()` stores the raw `const char*` pointer. If the source `String` is destroyed or reallocated, the pointer dangles. SNTP will dereference freed memory on next sync.
- **Fix**: Use static `char[]` buffers for server names, or ensure `String` lifetime exceeds SNTP usage.

### BUG-5 — NTP: inconsistent sync thresholds [HIGH]

- **Ref**: NTP-F3
- **Problem**: Component uses `now > 1000000000` (2001) while HAL uses year 2020 threshold. Duplicate, inconsistent logic.
- **Fix**: Single source of truth — delegate to HAL's `isSynced()`.

### BUG-6 — NTP: `uint32_t` vs `unsigned long` mismatch [HIGH]

- **Ref**: NTP-F1
- **Problem**: `bootTime` is `uint32_t` but `HAL::Platform::getMillis()` returns `unsigned long`. On ESP32, `unsigned long` is 32-bit so it works, but on platforms where `unsigned long` is 64-bit, truncation occurs. Wrap-around bug after ~49 days.
- **Fix**: Use `unsigned long` consistently, or document wrap-around handling.

### BUG-7 — MQTT: ODR violation — static member in header [HIGH]

- **Ref**: MQTT-F3
- **File**: `MQTT_impl.h:8`
- **Problem**: `MQTTComponent* MQTTComponent::instance = nullptr;` defined in header. Multiple TU inclusion = linker error. Needs `inline` (C++17) or `.cpp` file.
- **Fix**: Add `inline` keyword or move to a `.cpp` compilation unit.

### BUG-8 — MQTT: dangling broker pointer [MEDIUM] — **DONE (2026-08-23, PR #10)**

- **Ref**: MQTT-F12
- **Problem**: `setBroker()` passes `broker.c_str()` to PubSubClient which stores the raw pointer. If the `String` is later destroyed, the pointer dangles.
- **Fix**: Use a persistent `char[]` buffer for the broker address.

### BUG-9 — MQTT: no QoS validation [HIGH]

- **Ref**: MQTT-F4
- **Problem**: `publish()`, `subscribe()`, and `MQTTConfig.lwtQoS` accept `uint8_t` with no range check. Values > 2 are invalid per MQTT spec.
- **Fix**: Clamp or reject values > 2.

### BUG-10 — HA: `mqttPublish()` always returns true [HIGH]

- **Ref**: HA-F2
- **Problem**: Fire-and-forget publish with a misleading `bool` return. Callers log "Published successfully" on a value that is always `true`.
- **Fix**: Make `void` and remove conditional checks, or implement real acknowledgment.

### BUG-11 — HA: `volatile bool publishing` is dead code [HIGH]

- **Ref**: HA-F3
- **Problem**: Set and cleared but never read. Not thread-safe on dual-core ESP32.
- **Fix**: Remove entirely, or implement actual re-entrancy guard with `std::atomic<bool>`.

### BUG-12 — HA: ODR violation in HAEvents.h [HIGH]

- **Ref**: HA-F4
- **Problem**: `static constexpr const char*` creates separate storage per TU in C++14. Pointer comparison in EventBus would fail across TUs.
- **Fix**: Use `inline constexpr` (C++17) or extern linkage.

### BUG-13 — HA: `HA_TOPIC_BUF_SIZE` too small [MEDIUM]

- **Ref**: HA-F8
- **Problem**: 128-byte buffer can be exceeded with long discovery prefixes + `alarm_control_panel` component + long entity IDs. Silent truncation breaks HA discovery.
- **Fix**: Increase to 256, or add truncation detection with `DLOG_W`.

### BUG-14 — Storage: `putBlob()` nullptr dereference [HIGH]

- **Ref**: STOR-F3
- **Problem**: No null check on `data` pointer. `nullptr` with `length > 0` = crash.
- **Fix**: Add null guard at entry point.

### BUG-15 — Storage: cache never consulted by getters [HIGH]

- **Ref**: STOR-F4
- **Problem**: `getString()`, `getInt()`, etc. always go to HAL storage, bypassing the cache entirely. Cache only used for dirty-checking in `put*()`.
- **Fix**: Check cache first, fall through to HAL on miss.

### BUG-16 — Storage: RAMOnlyStorage truncates uint64 [MEDIUM]

- **Ref**: STOR-F6
- **Problem**: `putULong64` stores via `String((unsigned long)value)`, losing upper 32 bits.
- **Fix**: Use proper uint64 serialization.

### BUG-17 — Storage: RAMOnlyStorage putBytes discards data [MEDIUM]

- **Ref**: STOR-F7
- **Problem**: Only stores the length, not the actual byte data. `getBytes` always returns 0.
- **Fix**: Store actual bytes (e.g., base64-encoded or raw vector).

### BUG-18 — LED: phase calculation loses accumulated time [MEDIUM]

- **Ref**: LED-F6
- **Problem**: When `effectPhase > 1.0`, reset to `0.0` instead of wrapping (`fmod`). Causes effect stuttering.
- **Fix**: Use `effectPhase = fmod(effectPhase, 1.0f)`.

### BUG-19 — LED: `addLED()` after `begin()` causes vector desync [MEDIUM] — **DONE (2026-08-23, PR #17)**

- **Ref**: LED-F8
- **Problem**: `begin()` calls `ledStates.resize()` once. Adding LEDs after `begin()` grows `ledConfigs` but not `ledStates`.
- **Fix as filed**: guard `addLED()` against post-begin calls, or auto-resize
  `ledStates`. **Auto-resizing alone would not have been enough**: `begin()` also
  runs the pin validation and calls `pinMode()`, so a state entry conjured for a
  late LED would have addressed a pin nobody had configured.
- **Fixed**: `addLED()` now does the whole of what `begin()` would have done for
  that one LED — validate, `initializePin()`, append the state — and returns
  `bool` so a rejected pin is reported where the mistake is made rather than
  swallowed. `addSingleLED()`/`addRGBLED()` forward the result; callers ignoring
  it still compile.
- **Silent, not fatal**: nothing read out of bounds. `getLEDCount()` and
  `getLEDNames()` counted from `ledConfigs` while every setter bounds-checked
  against `ledStates`, so the LED appeared in the WebUI dropdown and refused
  every command sent to it.
- **Also**: `begin()` now uses `assign()` instead of `resize()`, so a second
  `begin()` resets `effectPhase` and `lastUpdate` too — `resize()` kept them.
- **Verified**: four tests in `test_led_component` fail against the previous
  implementation and pass against this one.

### BUG-20 — OTA: static variables in header files [HIGH]

- **Ref**: OTA-F4
- **Problem**: `s_bytesWritten`, `s_updateActive`, `s_stubBytesWritten` are `static` in headers. Each TU gets its own copy.
- **Fix**: Add `inline` (C++17) or move to `.cpp`.

### BUG-21 — OTA: `EVENT_START` / `EVENT_END` never emitted [HIGH]

- **Ref**: OTA-F3
- **Problem**: Declared in `OTAEvents.h` but never emitted by any code path.
- **Fix**: Emit at appropriate lifecycle points, or remove.

### BUG-22 — RemoteConsole: unbounded client read [HIGH] — **DONE (2026-08-23, PR #10)**

- **Ref**: RC-F3
- **Problem**: `while (client.available())` with no upper bound. Continuous byte stream = infinite loop.
- **Fix**: Add per-iteration byte limit (e.g., 512 bytes max per `loop()` call).

### BUG-23 — System: Early-Init anti-pattern [HIGH] — **DONE (2026-08-22, PR #7)**

- **Ref**: SYS-F4
- **File**: `System.h:224`
- **Problem**: `registerLEDComponent()` calls `led->begin()` manually before `core.begin()`. Constitution XIII explicitly forbids this.
- **Fixed**: the manual `begin()` is gone; `core.begin()` initialises the component.
- **Worse than the title suggested**: `ComponentRegistry::initializeAll()` skips any
  component already initialised and active, and that skipped branch is the only place
  `__dc_setEventBus()` and `__dc_setCore()` are ever called. The early call removed the
  injection rather than merely reordering it: the LED ran with both framework pointers
  null for the life of the device. Latent only because LEDComponent uses neither —
  `eventBus()` dereferences with no null check, and `on<T>()` returns 0 instead of
  subscribing.
- **Verified by compilation only** — DomoticsCore-System has no test suite (TEST-1).

### BUG-24 — WiFi: dead config fields [MEDIUM]

- **Ref**: WIFI-F6
- **Problem**: `connectionTimeout` and `CONNECTION_TIMEOUT` static const are both declared. `WifiConfig::connectionTimeout` may shadow the static constant, creating confusion.
- **Fix**: Remove one, standardize on a single timeout source.

### BUG-25 — WebUI: blocking component lifecycle in HTTP handler [MEDIUM]

- **Ref**: WEB-F11
- **File**: `ProviderRegistry.h:204-263`
- **Problem**: `enableComponent()` calls `component->shutdown()` or `begin()` directly inside an async HTTP handler. Blocking I/O in ESPAsyncWebServer context causes watchdog resets.
- **Fix**: Queue enable/disable action, process in `loop()`.

### BUG-26 — WebUI: OptionLabelPair serialization bug [MEDIUM]

- **Ref**: WEB-F12
- **File**: `StreamingContextSerializer.h:849-879`
- **Problem**: Key+colon+value written in one iteration. Partial value writes with small buffer cause malformed JSON because `optionIndex` not incremented but key already consumed.
- **Fix**: Split key, colon, value into separate states for correct pause/resume.

---

## Priority 4: Test Coverage (Constitution II — NON-NEGOTIABLE)

TDD with 100% coverage is a constitutional mandate. These components have critical gaps.

### TEST-1 — System: zero tests [CRITICAL] — **DONE (2026-08-23, PR #18)**

- **Ref**: SYS-F1
- **Problem**: No `test/` directory exists. Complex branching logic (heap guards, state machine, event orchestration) completely untested.
- **Fixed**: `DomoticsCore-System` gains a `native` environment — it was the last
  component without one — and 54 test cases in three projects:
  `test_system_config` (12: the presets the README tells users to write, and the
  `SystemState` names), `test_system_lifecycle` (24: boot, registration,
  double-`begin()`, the state callbacks, the status LED, the four console
  commands), `test_system_persistence` (18: every early return in the loaders).
- **The component set is the fixture.** Almost everything System does is
  selected by `__has_include`, so `platformio.ini` lists Core, LED,
  RemoteConsole and Wifi (the hard dependencies) plus Storage and SystemInfo,
  and deliberately omits WebUI, NTP, MQTT, OTA and HomeAssistant. Asking a
  `fullStack()` config for components that are not compiled in is itself a test:
  it must warn and boot, not fail.
- **How the console commands are reached**: `System` registers `status`, `wifi`,
  `storage` and `bootdiag` on the RemoteConsole and exposes none of them. The
  WiFi stub's simulated client — the harness the RemoteConsole suite already
  uses — drives them over a fake telnet session, which also covers
  `initBootDiagnosticsPersistence()` end to end: `bootdiag` reports
  `Boot Count: 1` and the values Storage received during `begin()`.
- **BUG-23 now has a real check.** It was closed "verified by compilation only"
  for want of this suite. `test_ready_drives_the_status_led` asserts the LED is
  named `status` and breathing after boot, which is only true if the component
  was registered and initialised by `core.begin()`.
- **Not covered**: the heap guards. The four `HAL::getFreeHeap() >= 3072`
  branches in `begin()` cannot be reached on a host that always has heap.
  Forcing them needs an injectable heap reading — a Core-level seam, filed
  nowhere yet. What is covered is the path taken when heap is plentiful, which
  is every path a healthy device takes.
- **Also**: `ERROR` is unreachable through `SystemConfig` alone. `ledPin` is
  `uint8_t`, so the LED pin validation that could fail `core.begin()` never sees
  a negative value. `WIFI_CONNECTING`, `WIFI_CONNECTED` and `SERVICES_STARTING`
  are likewise never entered — `test_boot_goes_straight_from_booting_to_ready`
  pins that, so the day one of them starts firing, a test says so.

### TEST-2 — LED: grossly inadequate coverage [CRITICAL] — **DONE (2026-08-23, PR #17)**

- **Ref**: LED-F5
- **Problem**: Only tests LED type definitions. Zero coverage for effects, PWM output, state management, WebUI interactions.
- **Fixed**: 18 test cases became 103, across four native projects —
  `test_led_types` (unchanged), `test_led_component` (lifecycle, pin validation,
  naming, every setter by index and by name, BUG-19 and DC-5 regressions),
  `test_led_effects` (the six effect curves, the rainbow ramp, the PWM
  arithmetic) and `test_led_webui` (every field the browser can post).
- **What made effects testable**: the stub HAL swallows `analogWrite()`, so a
  native test cannot observe a pin. The arithmetic was lifted out of
  `updateEffects()` into `effectBrightness()`, `rainbowColor()`, `scaleToMax()`
  and `pwmValue()` — pure, static, no member state — and the tests check the
  value computed for the pin. Behaviour is unchanged; the switch statement moved.
- **What made `LEDWebUI` testable**: `BaseWebUIComponents.h` places a CSS literal
  in `PROGMEM`, which no host toolchain defines, so the header could not be
  compiled by any native test. `Platform_HAL.h` now falls back to empty
  `PROGMEM` / identity `PSTR` — in the block that already carried the
  `DSNPRINTF_P` and `DLOG_SNPRINTF` fallbacks for the same reason. Both Arduino
  cores define the two macros before that point, so the `#ifndef` never fires on
  a board. This unblocks the same test for the other WebUI providers (TEST-6).
- **Still not covered**: `loop()` itself. Driving it needs a controllable clock
  and a recording `analogWrite()` — a Core-level test seam, not an LED one.
  What is proven is the arithmetic it applies, not the cadence at which it runs.
- **Two expectations the code corrected**: an LED is enabled by default at the
  component level, so `LEDWebUI` selecting another LED does not disable the
  first; and `getWebUIData()` answers an unknown context with `null`, not `{}`,
  because that is what ArduinoJson 7 makes of an untouched document. Both are
  recorded as tests rather than changed.

### TEST-3 — OTA: superficial coverage [HIGH]

- **Ref**: OTA-F6
- **Problem**: Critical gaps: no test for upload flow, SHA256 validation, state machine transitions, security enforcement, progress callbacks.

### TEST-4 — WiFi: superficial coverage [HIGH]

- **Ref**: WIFI-F13
- **Problem**: Key untested paths: STA fallback timer, AP mode, scan failure handling, reconnection logic.

### TEST-5 — NTP: inadequate coverage [MEDIUM]

- **Ref**: NTP-F9
- **Problem**: Missing: DST transitions, timezone edge cases, multi-server failover, wrap-around scenarios.

### TEST-6 — WebUI-related tests: zero coverage [HIGH]

- **Refs**: SI-F4, STOR-F16, LED-F14, HA-F17
- **Problem**: `SystemInfoWebUI`, `StorageWebUI`, `LEDWebUI`, and `HomeAssistantWebUI` have zero tests. WebUI providers handle user input (device name, settings, config mutations) with no validation testing.

### TEST-7 — Core: MemoryManager + ComponentConfig untested [MEDIUM]

- **Ref**: CORE-F17
- **Problem**: Zero tests for MemoryManager singleton and ComponentConfig validation logic.

---

## Priority 5: Known Bug — SSE Broadcast Warning Spam

### SSE-1 — WebUI: `DLOG_W` should be `DLOG_D` for routine broadcast [HIGH]

- **Refs**: WEB-F1, R-F2 (user-reported)
- **File**: `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h:939`
- **Problem**: Every ~5.4 seconds, the RemoteConsole is flooded with:
  ```
  [W][WEB] SSE broadcast: 1561 bytes, clients=1
  ```
  This is routine operational behavior logged at WARNING level, making real warnings invisible.
- **Fix**: Change `DLOG_W` to `DLOG_D` on line 939. Keep WARNING only for the heap-low skip case (line 933).
- **Effort**: One-line fix.

---

## Priority 6: File Size Violations (Constitution VII — 800 lines max)

### SIZE-1 — WebUI.h (950 lines) [HIGH]

- **Refs**: WEB-F2, R-F1
- **Fix**: Extract route setup + SSE broadcasting into `WebUIRoutes.h` / `BroadcastManager.h`.

### SIZE-2 — StreamingContextSerializer.h (921 lines) [HIGH]

- **Ref**: WEB-F3
- **Fix**: Unify duplicated `writeJsonString` overloads (const char* vs String&), extract field serialization helpers.

### SIZE-3 — Wifi.h (880 lines) [MEDIUM]

- **Refs**: WIFI-F1, R-F11
- **Fix**: Extract AP-mode logic or WebUI provider code into separate header.

### SIZE-4 — test_mqtt_component.cpp (899 lines) [MEDIUM]

- **Ref**: MQTT-F1
- **Fix**: Split test file by feature area (connection, publish, subscribe, queue, WebUI).

### SIZE-5 — test_ha_component.cpp (989 lines) [MEDIUM]

- **Ref**: HA-F10
- **Fix**: Split into `test_ha_config.cpp`, `test_ha_entity_management.cpp`, `test_ha_switch_integration.cpp`, `test_ha_publish_overloads.cpp`.

### SIZE-6 — test_webui_component.cpp (2520 lines) [LOW]

- **Ref**: WEB-F16
- **Fix**: Split into config tests, field tests, context tests, registry tests, serializer tests (3x over limit).

---

## Priority 7: Architecture / God Objects (Constitution I, XIII)

### ARCH-1 — System: God Object with 6+ responsibilities [HIGH]

- **Ref**: SYS-F6
- **Problem**: Orchestrates 10 components, handles registration, persistence, events, state, console commands, boot diagnostics.
- **Fix**: Extract `SystemComponentRegistrar`, `SystemEventOrchestrator`, `SystemConsoleCommands`.

### ARCH-2 — LED: God Object [HIGH]

- **Ref**: LED-F1
- **Problem**: Combines config management, hardware pin control, PWM, effect calculations, state management, WebUI in one class.
- **Fix**: Extract effect engine and WebUI into separate classes.

### ARCH-3 — System: `__has_include` count comment wrong [MEDIUM] — **DONE (2026-08-23, PR #18)**

- **Ref**: SYS-F5
- **Problem**: Comment says "20 directives" but actual count is 54 across 3 files.
- **Fixed**: the comment gives the breakdown — 22 in `System.h`, 16 in
  `SystemPersistence.h`, 16 in `SystemWebUISetup.h` — rather than one number
  that drifts silently. Corrected alongside TEST-1 because writing the suite
  meant counting them anyway: the count is the measure of how much of this
  component exists only in some builds.

---

## Priority 8: CI / Infrastructure

### CI-1 — No unit test CI workflow [HIGH] — **DONE (2026-08-22, PR #2)**

- **Ref**: R-F4
- **Problem**: GitHub Actions never runs native unit tests. Only compile test for ESP32.
- **Fixed**: `.github/workflows/ci.yml`, job `native-tests`. Projects are discovered
  from the tracked `platformio.ini` files rather than listed, which also picked up
  `DomoticsCore-WebUI/test/test_streaming_serializer` — a native project nested a
  level below the components, that nothing had ever run. An empty discovery is a
  failure, so the job cannot pass by testing nothing.
- **What it caught on first run**: three suites asserting their own component version
  as a stale literal, and one heap-stability test measuring the allocator rather than
  the code (it charged the first command's one-off cost to the loop, which passed on
  glibc 2.43 and failed at 384 bytes on the runner). Both fixed in the same PR.

### CI-2 — ESP8266 not tested in CI [HIGH] — **DONE (2026-08-22, PR #2)**

- **Ref**: R-F5
- **Problem**: Only ESP32 compilation tested. ESP8266 regressions go undetected.
- **Fixed**: `.github/workflows/ci.yml`, job `build-targets` — a matrix over
  `esp32dev`, `esp8266dev` and `esp32c3` building the FullStack example, the only one
  of the 29 examples pulling all twelve components and therefore the only one whose
  ESP8266 build reaches all ten ESP8266-specific headers.
- **Result**: the ten files compile clean. The platform the library had been promising
  without ever checking was sound — there was nothing to repair, only something to
  prove. `esp8266dev` RAM 61.7% / Flash 68.1%.
- **Since CI-8**: the install-from-GitHub witness builds both declared platforms too.

### CI-3 — Missing `DomoticsCore-Core` dependency in 3 library.json [HIGH] — **DONE (2026-08-22, PR #6)**

- **Ref**: R-F3
- **Files**: `DomoticsCore-Storage/library.json`, `DomoticsCore-SystemInfo/library.json`, `DomoticsCore-OTA/library.json`
- **Problem**: These include Core headers but don't declare the dependency.
- **Fixed**: `{ "name": "DomoticsCore-Core", "version": ">=1.4.0" }` added to each.
  In-tree builds never noticed, because every example lists the components
  explicitly as `file://` paths. Only someone installing a single component from
  the registry would have hit it — which is the case the manifest exists for.

### CI-4 — Wifi vs WiFi naming inconsistency [MEDIUM]

- **Ref**: R-F6
- **Problem**: Library name is `DomoticsCore-Wifi` but docs use `DomoticsCore-WiFi`.
- **Fix**: Standardize all documentation to match `library.json` name.

### CI-5 — Outdated GitHub Actions versions [MEDIUM] — **DONE (2026-08-22, PR #6)**

- **Ref**: R-F7
- **Fixed**: `version-check.yml` moved to `actions/checkout@v4` and
  `actions/setup-python@v5`. `test-github-install.yml` had already been updated
  and was left untouched — it carries the fork workaround for `pull_request`
  events, and rewriting it wholesale is how that gets lost.

### CI-8 — Root `library.json` pulls `AsyncTCP` unconditionally [HIGH] — **DONE (2026-08-23, PR #14)**

- **Problem**: the root manifest declares `ESP32Async/AsyncTCP` as a plain
  dependency. That package is ESP32-only; ESP8266 needs `ESP32Async/ESPAsyncTCP`.
  Every in-tree example works around it by listing the right one and adding
  `lib_ignore`, so nothing in this repository trips over it — but anyone
  installing the library from GitHub or the registry for an ESP8266 target does.
  It is also what keeps `test-github-install.yml` pinned to `esp32dev`: the
  witness project cannot be built for ESP8266 while the manifest resolves this
  way.
- **Fix as filed**: make the two TCP dependencies conditional on `platforms`.
  **That turned out to be the wrong fix.** `ESPAsyncWebServer` already declares
  them conditionally and correctly — `AsyncTCP` for `espressif32`/`libretiny`,
  `ESPAsyncTCP` for `espressif8266`, `RPAsyncTCP` for `raspberrypi`. The root
  entry was therefore redundant on ESP32 and harmful on ESP8266. Declaring a
  conditional pair here would have duplicated a correct declaration and pinned
  versions that can drift from what the web server wants.
- **Fixed**: the `AsyncTCP` entry is removed from the root manifest; the
  transitive dependency resolves the right backend per platform. The witness
  project now builds both declared platforms.
- **Proven, not assumed**: a witness resolving through the manifest fails on the
  previous root `library.json` with `AsyncTCP.h:22:10: fatal error: sdkconfig.h:
  No such file or directory` — an ESP-IDF header absent on ESP8266 — and builds
  clean once the entry is gone. ESP8266: 67.4% flash, 56.2% RAM.
- **Note**: filed 2026-08-22. Earlier revisions of this file pointed at CI-3 for
  this problem — that was wrong. CI-3 is about a missing `DomoticsCore-Core`
  dependency and has nothing to do with TCP backends.

### CI-10 — Nothing built the on-device test projects [HIGH] — **DONE (2026-08-23, PR #19)**

- **Filed and fixed together**, after a board was plugged in for the first time
  and none of the suites meant for it would run.
- **Problem**: four projects carry ESP8266 test suites. No workflow built any of
  them, and the native environments exclude them by `test_ignore` or
  `test_filter`. Three of the four had rotted where nobody could see it:
  Storage called a `setNamespace()` that has never existed in any commit — added
  2026-01-04 with the message "build issues to fix later"; WebUI passed `String`
  to `withCustomHtml(const char*)` after that API split into static and
  `Dynamic` overloads; and `test_schema_memory` had its suite outside `test/`
  and an `int main()` where the Arduino core supplies one, so it linked against
  nothing.
- **Worse than not compiling**: the Storage suite also constructed a bare
  `StorageComponent` and never opened it. Every put and get returns at
  `if (!isOpen)` without allocating, so had it compiled, three leak tests would
  have measured rejected calls and passed for the wrong reason.
- **Fixed**: the three suites repaired, and a `build-device-tests` job compiles
  all four on every push. It cannot run them — no runner has a board — but a
  suite that builds cannot rot silently for seven months.
- **Every test now asserts it is measuring something** before it measures:
  a `putString()` must return true or the test fails with "Storage did not open
  — the rest would measure nothing".
- **What the first real run found**: STOR-ESP-1. The WebUI suites pass, all nine
  cases.

### CI-11 — HomeAssistant declares an ESP8266 test env with no ESP8266 tests [MEDIUM]

- **Filed**: 2026-08-23, while writing the CI-10 job.
- **File**: `DomoticsCore-HomeAssistant/platformio.ini`
- **Problem**: the `esp8266dev` environment sets `test_framework` and
  `test_build_src` but carries no ESP8266 suite and no `test_filter`, so
  `pio test -e esp8266dev` cross-compiles the component's **native** suites for
  the board. They include `Platform_Stub.h` explicitly, which collides with the
  real platform header: `redefinition of 'class String'`, and a dozen more.
- **Why it matters now**: it is the reason `build-device-tests` lists its four
  projects instead of discovering them. "Declares `esp8266dev` and has a `test/`
  directory" is otherwise the right rule, and it matches this one too.
- **Fix**: decide what that environment is for. Either give it a `test_filter`
  naming a real ESP8266 suite, or drop the test settings and leave it a build
  environment. Then the CI job can discover rather than list.

### CI-6 — Missing `depends` in `library.properties` [MEDIUM]

- **Ref**: R-F8
- **Fix**: Add `depends=ArduinoJson,ESPAsyncWebServer,AsyncTCP,PubSubClient`.

### CI-9 — FullStack partition table wastes 384 KB per app slot [MEDIUM] — **DONE (2026-08-23, PR #11)**

- **Problem**: the example carries its own `partitions.csv` — `app0`/`app1` at
  `0x180000` each, plus a 960 KB `spiffs` partition — so the firmware ceiling is
  1,572,864 bytes. Measured on `main`, 2026-08-23, `esp32c3`: 1,302,694 bytes
  used, 82.8%, 270,170 free. That `spiffs` partition is dead weight in the
  default configuration: `embed_webui.py` gzips the WebUI into the firmware,
  `WebUIConfig::useFileSystem` defaults to `false`, `Storage` uses NVS on ESP32,
  and the only filesystem call sites in the whole tree are the two lines of the
  opt-in static-file fallback in `WebServerManager.h:139-140`. Nothing ever
  writes there.
- **Why it matters now**: the `esp32-ethernet` branch adds a Network component
  *and* moves to Arduino-ESP32 3.x, whose binaries are larger than 2.0.17's;
  FullStack no longer fits an ESP32-C3 4 MB part there. The ceiling is the
  example's own, not OTA's — OTA needs two app slots of equal size and does not
  dictate their size.
- **Fix**: switch the example to the stock `min_spiffs.csv` (app slots
  `0x1E0000` = 1,966,080 bytes, 128 KB spiffs, 64 KB coredump) or an equivalent
  local table. The same firmware then sits at 66.3%. Note that
  `docs/getting-started.md:68` already recommends `min_spiffs.csv` to users, so
  the example currently contradicts the documentation it ships with.
- **Note**: filed 2026-08-23, after marianorenzi reported that FullStack plus
  his Network component overflows an ESP32-C3. Answering him, we said we would
  make this change on `main` so his branch inherits it.
- **Fixed**: the example's local `partitions.csv` is deleted and both ESP32
  environments use the stock `min_spiffs.csv`. Measured, same firmware, same
  commit:

  | Target | Firmware | Before | After | Headroom |
  |---|---|---|---|---|
  | `esp32dev` | 1,333,617 B | 84.8% | **67.8%** | 269,247 → **632,463 B** |
  | `esp32c3` | 1,303,326 B | 82.9% | **66.3%** | 269,538 → **662,754 B** |

  Not a byte of firmware changed — only the ceiling. `esp8266dev` is unaffected
  (68.4%, no partition table on that platform). `DomoticsCore-OTA/examples/`
  `OTAWithWebUI` was already on `min_spiffs.csv`: FullStack was the only example
  in the tree carrying its own table, and the only one contradicting
  `docs/getting-started.md`.

### CI-7 — `local_ci.sh` counts total lines, not code lines [LOW]

- **Ref**: R-F10
- **Problem**: Constitution says exclude blanks/comments, but tool uses `wc -l`.
- **Fix**: Use `grep -cvE '^\s*(//.*)?$'` or update constitution to match tool behavior.

---

## Priority 9: Dead Code / YAGNI (Constitution IV)

| ID | Component | Dead Item | Action |
|----|-----------|-----------|--------|
| DC-1 | SystemInfo | `enableDetailedInfo`, `enableMemoryInfo` config flags — never enforced | Implement or remove |
| DC-2 | Storage | `StorageConfig::readOnly` — accepted but never checked by put/remove/clear | Implement or remove |
| DC-3 | HA | Residual `static_cast` in handleCommand despite virtual dispatch (R24) | Complete migration to virtual dispatch |
| DC-3b | HA | `volatile bool publishing` set/cleared but never read — dead code | Remove or implement guard (see BUG-11) |
| DC-4 | LED | `effectDirection` — confirmed dead, may have been missed in v1 cleanup | Remove if still present |
| DC-5 | LED | `shutdown()` doesn't clear internal vectors | **DONE** (PR #17) — `ledStates` cleared and shrunk; `ledConfigs` deliberately kept, see below |
| DC-6 | OTA | `EVENT_COMPLETE` vs `EVENT_COMPLETED` — confusing duplicate names | **DONE** (PR #4) — consolidated on `EVENT_COMPLETED` |
| DC-7 | OTA | `signaturePublicKey` — documented but never used | **DONE** (PR #4, docs PR #9) — removed with the five other unread fields |
| DC-8 | WebUI | Pointless `doc.shrinkToFit()` after serialization is complete | Remove |
| DC-9 | MQTT | `topicMatches()` allocates 2 vectors per call — use char* parsing | Refactor to zero-alloc |
| DC-10 | WebUI | `const_cast` in `onComponentsReady` — change API to accept non-const ref | Fix signature |
| PERSIST-1 | System | `loadWifiConfig()` AP-SSID generation appears unreachable | Investigate, then remove or move — see below |

### PERSIST-1 — System: the device-name AP SSID is never generated [MEDIUM]

- **Filed**: 2026-08-23, from writing the TEST-1 suite (PR #18).
- **File**: `SystemPersistence.h:189-192`
- **Problem**: `loadWifiConfig()` builds an AP SSID from `config.deviceName`
  when the stored one is empty. The condition looks reachable and is not:
  `System::begin()` runs `core.begin()` *before* `loadAllConfigs()`, and by then
  `WifiComponent::begin()` has already fallen back to AP mode and named itself
  `"DomoticsCore-" + <last 6 of MAC>` — a literal, not the device name. The SSID
  the loader inspects is therefore never empty.
- **The two ways in are mutually exclusive.** With `config.wifiSSID` empty, WiFi
  always ends `begin()` holding some AP SSID; with it set, `loadWifiConfig()`
  returns at its first guard. No configuration reaches the branch.
- **Consequence, if confirmed**: dead code, and a user who names their device
  still gets an access point called `DomoticsCore-XXXXXX`. Note that the *normal*
  path is unaffected — `System::registerWifiComponent()` sets a device-name-based
  AP SSID before `core.begin()`, and `Wifi.h:130` honours a pre-set one.
- **Not fixed here.** The fix belongs in the WiFi layer, not in a test PR, and
  `Wifi.h` is being rewritten on `esp32-ethernet`. Confirm against that branch
  before touching it.
- **Pinned meanwhile**: `test_an_absent_ap_ssid_keeps_the_one_the_component_already_has`
  asserts the behaviour as it is, not as the branch intends, so whoever settles
  this sees the test change with it.

**DC-5, on keeping `ledConfigs`.** "Clear the internal vectors" was filed as one
action; it is two. `ledStates` is runtime churn and is released, per Constitution
XIV. `ledConfigs` is the user's registration — the pins, names and brightness
ceilings passed to `addSingleLED()`. `ComponentRegistry::shutdownAll()` leaves a
component able to be initialised again, and clearing the configuration would have
left that second `begin()` driving nothing at all, silently. The vector that
holds user input outlives the shutdown; the vector that holds machine state does
not.

---

## Priority 10: Minor Issues (LOW)

| ID | Component | Issue |
|----|-----------|-------|
| LO-1 | Multiple | Hardcoded fallback versions in `getWebUIVersion()` drift from actual version |
| LO-2 | MQTT | `MQTTPublishEvent` is 832 bytes — consider reducing field sizes |
| LO-3 | Core | `__dc_*` field names use reserved double-underscore prefix |
| LO-4 | Core | `nextId` overflow after 4B subscribe/unsubscribe cycles (theoretical) |
| LO-5 | Core | Backpressure silently drops oldest event with no logging |
| LO-6 | NTP | `TIMEZONE_LOOKUP` static constexpr duplicated per TU |
| LO-7 | NTP | `setSyncInterval()` is no-op on ESP8266 — not documented |
| LO-8 | RC | `nextClientId` wraps after 4B connections |
| LO-9 | WiFi | Member shadowing (`ssid`/`password` vs constructor params) |
| LO-10 | WiFi | Magic numbers for WiFi status codes |
| LO-11 | LED | French comment in test file ("Tests unitaires") — **DONE** (PR #17) |
| LO-12 | System | French comment in FullStack test |
| LO-13 | SI | Unicode emoji in log message (non-ASCII) |
| LO-14 | SI | `calculateCpuLoad()` uses unreliable heap-churn heuristic |
| LO-15 | MQTT | Unicode checkmark/cross in log messages |
| LO-16 | CHANGELOG | Missing version reference links |
| LO-17 | Storage | `nameStr_` backing String — move-safety risk |
| LO-18 | HA | `shutdown()` emits MQTT events without checking `mqttConnected` — wasted EventBus traffic |
| LO-19 | HA | EventBus subscriptions from `begin()` never unsubscribed in `shutdown()` — restart causes duplicates |
| LO-20 | HA | `HASwitch`/`HABinarySensor` payloadOn/Off are `String` holding "ON"/"OFF" — should be `const char*` |
| LO-21 | HA | `HAAlarmControlPanel::code` is `String` while sibling fields are `char[]` — inconsistent |
| LO-22 | HA | `HAButton::buildDiscoveryPayload` duplicates base class logic instead of calling super |
| LO-23 | HA | `HALight.h`, `HAButton.h`, `HAAlarmControlPanel.h` rely on transitive `Logger.h` include |
| LO-24 | WebUI | `volatile int pollingClients` — use `std::atomic<int>` or remove `volatile` |
| LO-25 | WebUI | Admitted tech debt: duplicated `serializeContext()` with "until I move it" comment |
| LO-26 | WebUI | Magic string `"wifi/ap/enabled"` instead of centralized event constant |
| LO-27 | WebUI | Zero tests for WebSocketHandler, route handlers, auth flow, SSE broadcast |
| LO-28 | Root | CHANGELOG references `docs/migration/` which doesn't exist |
| LO-29 | LED | `library.json` uses `^1.3.0` dep constraint while all others use `>=` — will break at Core 2.0 |
| LO-30 | Root | Root `examples/` only has README — Arduino Library Manager shows 0 examples |
| LO-31 | Root | `check_versions.py` doesn't enforce Constitution XV propagation rules |
| LO-32 | WebUI | `wsBuffer_[]` static member defined in header — ODR violation risk in multi-TU builds |

---

## Resolved in v2.0.1 (2026-03-11) — 20 items

| Item | Severity | Fix |
|------|----------|-----|
| **SEC-2** | CRITICAL | `abort()` + error event on SHA256 mismatch after OTA download |
| **MEM-1** | HIGH | `shrink_to_fit()` in ComponentRegistry, EventBus, IWebUIProvider, Wifi; bounded System stateCallbacks (max 8); Wifi reserve(n) |
| **BUG-1** | HIGH | `static_assert(is_trivially_copyable)` on EventBus publish |
| **BUG-3** | MEDIUM | LoggerCallbacks ID-based add/remove (was clearing all) |
| **BUG-7** | HIGH | `inline` on MQTTComponent::instance |
| **BUG-9** | HIGH | QoS >2 clamping in MQTT publish/subscribe/lwtQoS |
| **BUG-10** | HIGH | `mqttPublish()` → void (always returned true) |
| **BUG-11** | HIGH | Removed dead `volatile bool publishing` from HA |
| **BUG-12** | HIGH | `inline constexpr` on HAEvents topic strings |
| **BUG-14** | HIGH | Null guard on Storage::putBlob() |
| **BUG-18** | MEDIUM | LED effectPhase wrapping with fmod() |
| **BUG-20** | HIGH | `inline` on OTA static variables (ESP32/ESP8266/Stub) |
| **SSE-1** | HIGH | SSE broadcast log level WARN → DEBUG |
| **DC-3b** | MEDIUM | (same as BUG-11) |
| **DC-4** | MEDIUM | Already removed in v1 — confirmed absent |
| **DC-8** | MEDIUM | Removed pointless doc.shrinkToFit() (WebUI, MQTTWebUI) |

**Commit**: `e081940` — 580 unit tests pass across 11 components.

---

## Tracking Summary

| Priority | Items | Constitution | Remaining |
|----------|-------|-------------|-----------|
| 1. Security | SEC-1 to SEC-6 | OTA, Remote, WebUI | 0C, 0H, 3M (**SEC-1, SEC-2, SEC-3 done**) |
| 2. Memory Safety | MEM-1 to MEM-4, STOR-ESP-1 | XIV (ABSOLUTE) | 0C, 2H, 2M (**MEM-1 done**; STOR-ESP-1 new) |
| 3. Code Safety | BUG-1 to BUG-26 | Multiple | 0C, 0H, 6M (**18 done**) |
| 4. Test Coverage | TEST-1 to TEST-7 | II (NON-NEGOTIABLE) | 0C, 3H, 2M (**TEST-1, TEST-2 done**) |
| 5. SSE Bug | SSE-1 | — | **DONE** |
| 6. File Size | SIZE-1 to SIZE-6 | VII (800 lines) | 0C, 2H, 3M, 1L |
| 7. Architecture | ARCH-1 to ARCH-3 | I, XIII | 0C, 2H, 0M (**ARCH-3 done**) |
| 8. CI/Infrastructure | CI-1 to CI-11 | II, XII | 0C, 0H, 3M, 1L (**CI-1, CI-2, CI-3, CI-5, CI-8, CI-9, CI-10 done**; CI-11 new) |
| 9. Dead Code | DC-1 to DC-10, PERSIST-1 | IV (YAGNI) | 0C, 0H, 6M (**DC-3b, DC-4, DC-5, DC-6, DC-7, DC-8 done**; PERSIST-1 new) |
| 10. Minor | LO-1 to LO-32 | Various | 0C, 0H, 0M, 31L (**LO-11 done**) |
| **Total** | **103 items** | | **0C, 9H, 25M, 33L** (46 resolved) |

---

## Archived: Roadmap v1 (2026-03-04 → 2026-03-09) — ALL COMPLETE

| Priority | Items | Status |
|----------|-------|--------|
| 1. Memory Safety (R1-R7, M9-M10) | shrink_to_fit, String→snprintf, char[] migration | DONE |
| 2. Code Bugs (M11-M12, M15-M16, M19) | Core::emit sticky, LED name, Storage events, millis HAL, HA events | DONE |
| 3. HAL Isolation (R8-R10) | millis(), delay(), #ifdef in Storage | DONE |
| 4. File Splits (R11-R13) | Already compliant (excluding blanks/comments) | N/A |
| 5. Dead Code (R17-R23) | isValidTopic, config limits, retryDelayMs, otaPassword, RC auth, effectDirection, Storage WebUI | DONE |
| 6. Anti-Patterns (R14-R16) | Documented as accepted exceptions | DONE |
| 7. Progressive Refactoring (R24-R25) | Virtual dispatch, enum class | DONE |
| 8. EventBus Commands (R26) | ha/command event, callback removal (v2.0.0 breaking) | DONE |
| 9. Documentation Debt (D1-D12) | All 12 doc items resolved | DONE |
| 10. Dead Config Fields (C1-C3) | 8 fields removed from MQTT, System, Storage | DONE |

**Breaking change pending**: 8 dead config fields removed (C1-C3) + callback removal (R26) require MAJOR version bump.

---

*Generated from 13 parallel adversarial review agents cross-referencing all source code against the constitution v1.6.0.*
