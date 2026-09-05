# Changelog

All notable changes to DomoticsCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
— with one departure, recorded here rather than left to be discovered.

> **2.1.0 removed eight public symbols and shipped as a minor release.** Under
> strict SemVer that is a major change, and `2.x` consumers who expected a minor
> update to be safe did not get one.
>
> The decision was taken knowingly. Seven of the eight — the six `OTAConfig`
> security fields and `SystemConfig::otaPassword` — were read by no code path,
> so removing them takes away nothing a caller could have depended on; the break
> is at compile time and it is loud.
>
> The eighth is different and is the reason this note exists.
> `OTAEvents::EVENT_COMPLETE` **did** fire. Code referencing the constant fails
> to compile, but code subscribing by string literal — `on("ota/complete", ...)`
> — still compiles, still runs, and silently never fires again. That is the one
> removal a reader could miss entirely, and no version number would have warned
> them either way.
>
> Releases that remove public API will say so at the top of their entry, as
> 2.1.0 does. If you need the guarantee that a minor release never breaks you,
> pin an exact version.

## [2.3.0] - 2026-09-05

> **This release changes two public contracts and ships as a minor release.**
> The same departure 2.1.0 and 2.2.0 took, recorded here for the same reason.
>
> **`/api/ui/action` is `POST`, and every state-changing HTTP route now
> requires a per-boot CSRF token** (`X-DC-Token` header or `token` query,
> fetched from `GET /api/ui/token`). The device's own page does this for you.
> Anything else that drove the API — a script, a dashboard, `curl` — gets a
> `404` on the old `GET` shape and a `403` without the token. That is SEC-10,
> below, and it is the point: those requests were reachable cross-origin from
> any page you visited.
>
> **`EventBus::publish(topic, value)` refuses at compile time any payload
> type that is not trivially copyable.** `emit<String>(…)` no longer builds.
> It never worked: the queue byte-copied the `String` and dispatched it after
> the publisher's local had gone, so a subscriber doing
> `static_cast<const String*>(payload)` read freed heap. Publish bytes instead
> — `publish(topic, ptr, size)` — and read them as bytes. The one shipped
> payload this touches is Storage's `EVENT_READY`, which now carries the
> namespace as a C string rather than a `String` object.
>
> **On ESP32 and ESP32-C3, `System` now arms a loop watchdog by default** —
> 30 s, `SystemConfig::loopWatchdogSeconds`, `0` to disable. A sketch that
> calls `System::loop()` at least every 30 s never notices. One that stops
> calling it for that long is now a panic and a core dump instead of a silent
> hang — which is the point: measured on a WROOM-32D and a C3, a stuck
> `loop()` never rebooted, because the Arduino core keeps `loopTask` off the
> task watchdog (OBS-7). And `BootDiagnostics` renamed its heap fields —
> `lastBootHeap`/`lastBootMinHeap` are `bootHeap`/`bootMinHeap`, a loud
> compile-time break for the rare code that read them — because they
> described the new boot under the previous run's name. The persisted keys
> follow (`boot_heap`, `boot_minheap`) and the old ones are removed on the
> first boot (OBS-6).

### Security

**SEC-10 — WebUI: an unauthenticated cross-origin request could install
firmware.** Every state-changing WebUI route accepted a request that did not
come from the device's own page: the action dispatcher was registered as
`GET` and synthesised `"POST"` for the provider guards, and `/api/ota/upload`
accepted a bare `multipart/form-data` body — CORS-safelisted, no preflight.
Measured on a `nodemcuv2`: a cross-origin `fetch(…, {mode:'no-cors'})`
installed arbitrary firmware and rebooted the device. `enableAuth` would not
have closed it, because browsers attach cached Basic credentials to
cross-origin requests.

The fix is the per-boot token described above, minted from the hardware RNG
(`esp_random` / `os_get_random`, never Arduino `random()`), served by a route
that carries no CORS headers so cross-origin script cannot read it, and
checked independently of `enableAuth` on `/api/ui/action`, `/api/ota/upload`
(before flash is erased), `/api/ota/check`, `/api/ota/update`'s action branch
and `/api/components/enable`. Verified on the board in both directions.

**SEC-11 — OTA: `/api/ota/update` and `/api/ota/check` had no authentication
at all**, even with `enableAuth` on. Gated by the same token.

**SEC-8 — OTA: `maxDownloadSize` bounded downloads and not uploads.** It now
bounds both; an upload that announces more than the limit is refused before
the first byte reaches flash.

**SEC-9 — OTA: the upload path sized itself from the multipart envelope.**
The numbers the OTA subsystem reports now say what they measured. The fix
this finding first recommended — passing `0` for the size — was measured to
be a regression on both cores and was not applied; the roadmap entry opens
with a warning against its own former advice.

### Observability

The first lot of a new series: devices in the field reboot on their own and
nothing records why. This release reads what the platform already kept.

- **ESP8266 (OBS-2)**: the exception cause and program counter the SDK
  preserves across an exception or a watchdog reset now reach the boot log
  (`Reset detail: Fatal exception:28 … epc1:0x40217580`), `BootDiagnostics`
  and the `bootdiag` console command, with the `addr2line` hint. Measured:
  a hung loop reports `Software Watchdog` with `epc1` inside the loop. What
  it cannot see is said at boot on that platform: an `abort()`, an `assert`
  and an out-of-memory `new` all arrive as "Software/System restart", with
  nothing in the registers — indistinguishable from `ESP.restart()` by
  reason alone.
- **ESP32 / ESP32-C3 (OBS-1)**: the core writes an ELF core dump to the
  `coredump` partition on every panic, and nothing read it. The boot log now
  says whether the partition exists and whether a dump is waiting, with its
  size (`⚠ A core dump from a previous panic is waiting: 15268 bytes`);
  `bootdiag` names it. Reading it off the device comes in a later lot.
- **All platforms (OBS-7, OBS-6)**: the loop watchdog and the renamed boot
  heap fields, described at the top. On ESP8266 the watchdog call is a no-op
  — the SDK's soft WDT already resets a hang in about 3 s.

### Fixed

- **OTA (BUG-35): a client disconnect mid-upload locked OTA out until a
  power cycle.** The upload stayed "in progress" forever and every later
  attempt was refused. Reproduced with a scripted disconnect on both boards,
  red then green.
- **OTA (BUG-21): `ota/start` and `ota/end` were never emitted.** Both fire
  now, on every path.
- **OTA: the download path reported the size the server announced**, not
  the size it received, in both directions.
- **Core (BUG-30): `String` payloads on the EventBus were a use-after-free.**
  See the note above. The compile-time guard found ten publishing sites, seven
  of them in `Wifi.h` publishing a temporary; a grep had found seven.
- **HomeAssistant (BUG-31): the settings handler read six parameters nothing
  sends, and wrote flash on every request.** It now reads the field/value
  convention the dispatcher actually uses, and persists only on change.
- **WebUI (BUG-32): the device name was interpolated raw into every update.**
  A name containing a quote broke the JSON for every client, persistently,
  until renamed. Escaped at the sink; `SystemInfoWebUI` validates the input.
- **WebUI (BUG-34): `/api/ui/schema` truncated when the serializer could not
  make progress** — the fix its twin route received in v1.5.0 had never
  reached it. Both routes share one helper now, and the tests found the stall
  fires at ordinary chunk sizes.
- **WebUI (BUG-33): the streaming escaper's control-character test depended
  on `char` signedness.** Host-only in practice; measured against all three
  toolchains.

### Changed

- **Core**: `HAL::Platform::getRandomBytes()` — the hardware RNG, exposed for
  the token above and for anyone else who needs random bytes that are not
  `random()`.
- **WebUI**: `WebUI.h` (1008 → 769 lines) and `StreamingContextSerializer.h`
  (933 → 756) were split under the 800-line ceiling. New headers under
  `include/DomoticsCore/WebUI/` — `JsonStreamWriter.h`, `SchemaMemProbe.h`,
  `UpdateBuilder.h`, `SystemHeader.h` — and the schema chunk loop, which
  existed three times, lives once in `ProviderRegistry.h`. `#include
  <DomoticsCore/WebUI.h>` is unchanged.
- **Performance (MEM-2)**: `String` concatenation removed from the hot paths
  — HomeAssistant parses inbound MQTT without the allocator, WiFi builds its
  scan summary without growing a `String`, the console's help text is static.
  The 14-character small-string threshold the finding was reasoned against is
  10 on the ESP8266; the roadmap says so.

### Testing

- **Native**: 836 test cases across 13 projects, up from 715 at
  v2.2.0 — 819 before the observability lot, which added 17 across
  SystemInfo and System, driven through new `Platform_Stub.h` seams for the
  reset detail, the core dump status and the loop watchdog. TEST-4 gave WiFi a 16-case behavioural suite over the fallback
  ladder, AP mode and reconnection, on scriptable `millis`/heap/restart and a
  stateful WiFi stub; TEST-6 covered the WebUI-related providers; SIZE-1 and
  SIZE-2 made the chunk loop and the update builder testable for the first
  time.
- **On hardware**: a real `multipart/form-data` POST now runs against both
  boards, refused and accepted, each with a removal check that discriminates
  (TEST-8). Eight on-device suites compile in CI; seven ESP8266 and one
  ESP32-CAM have been executed on silicon. A second real-conditions campaign
  ran both boards against a real network, broker and browser and found BUG-35.
  **The ESP32-C3, which CI had compiled for since v2.1.1 and nothing had ever
  run on, ran the observability lot's probe on 2026-09-05**: same findings as
  the ESP32, with one difference — its Arduino core watches no idle task at
  all, so a hang is silent there for a different reason.
- **Tooling**: `tools/on-device/` — the harness the campaigns ran on, which
  reads the ESP32-CAM that `pio test` cannot; `clean_examples.py` learned the
  `test/` projects after a 19 GB recursion (CI-13).

## [2.2.0] - 2026-08-26

> **This release removes one public symbol and changes the behaviour of another,
> and ships as a minor release.** The same departure 2.1.0 took, recorded here
> for the same reason.
>
> `WebUIComponent::configure()` is gone. Like seven of the eight symbols 2.1.0
> removed, no code path read it — the break is at compile time and it is loud.
>
> The one worth reading twice does not break compilation at all.
> `MQTTComponent::publish()` **no longer returns `false` when the rate limit is
> reached**; it queues the message and returns `true`. Code that inspected that
> return to detect a dropped publish still compiles, still runs, and now never
> sees a drop — because there are no longer any. That is the correct behaviour,
> and it is why BUG-29 existed, but a caller counting rejections will silently
> count zero.
>
> `publishRateLimit` also no longer applies while disconnected. Offline,
> `maxQueueSize` is the bound; the limit governs what goes on the wire.

### Security

**SEC-2 — OTA: a firmware whose SHA-256 did not match was left bootable.**

The fix recorded as resolved in v2.0.1 did nothing, on either platform, for two
releases. It called `abort()` *after* `HAL::OTAUpdate::end(true)`, and `end()` is
the commit: on ESP32 it has already called `esp_ota_set_boot_partition()`, on
ESP8266 it has already staged an eboot copy over the running sketch. Neither
Arduino core lets an application undo that. The component reported `State::Error`
and skipped its own reboot, so nothing looked wrong — until any watchdog reset or
power cycle.

Verification now happens **before** the commit, so a rejected image is never made
bootable. Reproduced on hardware both ways: on a `nodemcuv2` and on an ESP32-CAM,
the pre-fix code really does arm the bootloader with a mismatched image.

**SEC-7 — OTA: the upload path had no integrity check at all.**

`finalizeUpload()` committed whatever arrived. SEC-3 authenticated the endpoint,
which stops a stranger pushing firmware, but did nothing about a corrupt transfer
from a legitimate one.

`beginUpload()` now takes an optional expected SHA-256 — supplied as an
`X-Firmware-SHA256` header or a `?sha256=` query parameter — and verifies it
before the commit. Set `OTAConfig::requireUploadHash` to refuse uploads that
carry none; note that this also rejects the built-in browser upload form, which
cannot send one.

### Fixed

- **MQTT (BUG-29): the publish rate limit silently discarded discovery.** A
  device declaring 9 sensors and 3 buttons left 9 sensor configs retained on the
  broker and none of the buttons. Rate-limited messages are now deferred into the
  queue that already existed and drained over the following seconds. The failure
  was order-dependent and silent: adding one sensor could remove an unrelated
  button.
- **LED (BUG-19): LEDs added after `begin()` were never driven.**
- **OTA**: the ESP8266 HAL's `abort()` no longer commits the update it was asked
  to discard once every announced byte has been written.

### Changed

- `WebUIComponent::configure()` removed — see the note above.
- `HomeAssistantComponent` no longer logs `Discovery published`. It emits on the
  EventBus and never learns the outcome, so it now says what it actually did.

### Testing

- The four ESP8266 on-device suites, unrunnable for months, were repaired and run
  on hardware for the first time. OTA adds a fifth, and an ESP32 suite a sixth.
- **715 native test cases** across 13 projects, up from 567 at v2.1.0.
- STOR-ESP-1 was withdrawn: the suite measured an undrained EventBus and charged
  it to Storage.

## [2.1.1] - 2026-08-23

### Fixed

**The library could not be installed for ESP8266, and had not been able to for
some time.**

`library.json` declared `ESP32Async/AsyncTCP` as a plain dependency. That package
is ESP32-only, so installing DomoticsCore from the registry or from GitHub for an
`espressif8266` target pulled it in and stopped at:

```
AsyncTCP.h:22:10: fatal error: sdkconfig.h: No such file or directory
```

`sdkconfig.h` is an ESP-IDF header; it does not exist on ESP8266. The manifest
listed `espressif8266` among the library's platforms throughout.

Nothing in this repository was affected. Every example lists components as
`file://` paths and names its own TCP backend, so the manifest was never
consulted here — the one path nobody in the project takes is the one every user
takes. If you build from the examples, or pin components individually, nothing
changes for you.

- fix(manifest): remove the unconditional `AsyncTCP` dependency (CI-8) [HIGH].
  `ESPAsyncWebServer` already declares the TCP backends conditionally and
  correctly — `AsyncTCP` for `espressif32`/`libretiny`, `ESPAsyncTCP` for
  `espressif8266`, `RPAsyncTCP` for `raspberrypi` — so the root entry was
  redundant on ESP32 and harmful on ESP8266. The transitive dependency now
  resolves the right backend per platform.
- ci: `test-github-install.yml` builds both declared platforms. It is the only
  workflow that resolves through the manifest rather than through `file://`
  paths, which is why this went unseen; it is now the place it cannot.

No API change. If you are on ESP32, 2.1.1 resolves `AsyncTCP` through
`ESPAsyncWebServer` instead of directly, which may pick a newer version within
the same major.

---

## [2.1.0] - 2026-08-23

### ⚠️ Breaking Changes — one of them needs your attention

**`SystemConfig::otaPassword` is removed, and setting it never protected anything.**

If you followed the FullStack example's *Security Best Practices* section and wrote
`config.otaPassword = "MyStr0ng!P@ssw0rd"`, **your OTA endpoint was never
protected**. The field was assigned to `OTAConfig::bearerToken`, which no code
path ever read. That wiring was itself a past "fix" (R20 in 2.0.0), connecting a
dead field to another dead field.

To actually protect firmware upload, enable WebUI authentication:

```cpp
config.enableWebUI = true;
webuiConfig.enableAuth = true;
webuiConfig.username = "admin";
webuiConfig.password = "...";
```

Both `/ota/upload` and `/api/ota/upload` now require those credentials, checked on
the first upload chunk, before any byte reaches flash.

**Other removals**, none of which ever did anything:

- `OTAConfig`: `requireTLS`, `bearerToken`, `basicAuthUser`, `basicAuthPassword`,
  `rootCA`, `signaturePublicKey` (SEC-1, DC-7). No code path read them. Transport
  security belongs in the `ManifestFetcher` / `Downloader` callbacks you install —
  that is where the HTTP client is.
- `OTAEvents::EVENT_COMPLETE` (DC-6) — use `EVENT_COMPLETED`, whose payload now
  carries the fields both events used to split between them.
- `HAL::IStorage` gains a pure virtual `maxEntries()`. Only affects out-of-tree
  implementations of that interface; the three in-tree backends are updated.

### Security

- fix(ota): `/ota/upload` and `/api/ota/upload` require WebUI authentication
  (SEC-3) [HIGH]. Previously any network client could POST firmware and have it
  written to flash. Checked on the first chunk; a failed check aborts the update
  rather than letting the remaining chunks through.
- fix(system): the LED component was initialised outside `ComponentRegistry`,
  which skips already-initialised components — and that skipped branch is the
  only place EventBus and Core are injected. The component ran with both pointers
  null (BUG-23) [HIGH].

### Bug Fixes

**Storage**
- fix(storage): getters consult the cache instead of always hitting the backend
  (BUG-15) [HIGH]. Note: when a key is absent from both cache and backend, the
  caller's default is cached — use `put()` to set canonical values.
- fix(storage): `RAMOnlyStorage::putBytes()` stored the *length* in place of the
  content and returned success for data it discarded (BUG-17) [MEDIUM].
- fix(storage): `RAMOnlyStorage` uint64 round-trip no longer truncates (BUG-16)
  [MEDIUM].
- fix(storage): entry counts come from the backend rather than from cache size,
  and blob capacity is released rather than merely cleared.

**NTP**
- fix(ntp): SNTP server names live in buffers that outlive the client
  (BUG-4) [HIGH]. `esp_sntp_setservername()` keeps the pointer it is given.
- fix(ntp): a single sync threshold, delegated to `HAL::NTP::isSynced()`
  (BUG-5) [HIGH].
- fix(ntp): `bootTime` width matches `getMillis()` (BUG-6) [HIGH]. Private
  member; no API change.

**MQTT and RemoteConsole**
- fix(mqtt): the broker address lives in a buffer the component owns
  (BUG-8) [MEDIUM]. PubSubClient keeps the pointer given to `setServer()` without
  copying it.
- fix(remoteconsole): telnet input is bounded in both directions (BUG-22) [HIGH]
  — at most 512 bytes read per `loop()` call, and the command buffer is capped.
  Bounding the read alone left the heap open to a client that never sends a
  newline.

### CI

The repository had 29 test suites that no workflow ran, and compiled only
`esp32dev` while declaring three targets.

- ci: run every native suite — 12 projects, 567 test cases (CI-1) [HIGH]
- ci: build `esp32dev`, `esp8266dev` and `esp32c3` (CI-2) [HIGH]. The ten
  ESP8266-specific files had never been compiled by anything; they were sound.
- ci: declare the `DomoticsCore-Core` dependency in Storage, SystemInfo and OTA
  (CI-3) [HIGH]
- ci: `actions/checkout@v4`, `actions/setup-python@v5` (CI-5)
- fix(example): FullStack uses the stock `min_spiffs.csv` (CI-9). Same firmware,
  ceiling raised from 1,572,864 to 1,966,080 bytes — 66.3% instead of 82.9% on
  ESP32-C3.

`main` now requires six checks. Known gap: the install-from-GitHub witness is
still `esp32dev`-only, because the root manifest pulls the ESP32-only `AsyncTCP`
unconditionally (CI-8, open).

### Documentation

- docs: six documents still described the removed OTA fields as working security
  — including a "TLS and Certificate Configuration" section stating that
  `requireTLS` rejected non-HTTPS URLs. Removing the fields without correcting
  the documentation would have had readers trust the protection twice.
- docs: `docs/CODE-ROADMAP.md` gains a Delivery section recording how these lots
  land and, next to what CI proves, what it does not.

### Component Versions

| Component | Version | |
|-----------|---------|---|
| **DomoticsCore** (root) | **2.1.0** | breaking removals |
| Core | 1.5.3 | unchanged |
| WebUI | 1.5.1 | unchanged |
| LED | 1.4.1 | unchanged |
| Storage | **1.5.0** | `IStorage` gains a pure virtual |
| MQTT | **1.4.3** | |
| HomeAssistant | 2.0.1 | unchanged |
| OTA | **1.5.0** | six config fields removed |
| NTP | **1.3.1** | |
| RemoteConsole | **1.4.3** | |
| Wifi | 1.4.2 | unchanged |
| System | **1.5.0** | `otaPassword` removed |
| SystemInfo | **1.4.1** | |

---

## [2.0.1] - 2026-03-11

### Bug Fixes (Roadmap v2 Batch Remediation — 20 items)

**Security**
- fix(ota): SHA256 mismatch now calls `abort()` to rollback corrupted firmware (SEC-2) [CRITICAL]

**Memory Safety (Constitution XIV)**
- fix(core,webui,wifi): add `shrink_to_fit()` after size-reducing operations (MEM-1)
- fix(system): bound `stateCallbacks` to max 8 entries
- fix(wifi): `scanNetworks` uses `reserve(n)` for exact allocation

**ODR Violations**
- fix(mqtt): `inline` on `MQTTComponent::instance` static member (BUG-7)
- fix(ha): `inline constexpr` on HAEvents topic strings (BUG-12)
- fix(ota): `inline` on static variables in Update headers (BUG-20)

**Code Safety**
- fix(core): `static_assert(is_trivially_copyable)` on EventBus publish (BUG-1)
- fix(core): `LoggerCallbacks` now uses ID-based add/remove — was clearing ALL (BUG-3)
- fix(mqtt): clamp QoS values >2 in publish, subscribe, and lwtQoS (BUG-9)
- fix(storage): null guard on `putBlob()` (BUG-14)
- fix(led): `fmod()` wrapping for effectPhase instead of reset to 0 (BUG-18)
- fix(webui): SSE broadcast log level WARNING → DEBUG (SSE-1)

**Dead Code Removal**
- fix(ha): remove dead `volatile bool publishing` (BUG-11/DC-3b)
- fix(ha): `mqttPublish()` → `void` — always returned true (BUG-10)
- fix(webui,mqtt): remove pointless `doc.shrinkToFit()` after serialization (DC-8)

### Component Versions

| Component | Version |
|-----------|---------|
| **DomoticsCore** (root) | 2.0.1 |
| Core | 1.5.3 |
| WebUI | 1.5.1 |
| LED | 1.4.1 |
| Storage | 1.4.3 |
| MQTT | 1.4.2 |
| HomeAssistant | 2.0.1 |
| OTA | 1.4.2 |
| RemoteConsole | 1.4.2 |
| Wifi | 1.4.2 |
| System | 1.4.2 |

---

## [2.0.0] - 2026-03-09

### Breaking Changes

- **HomeAssistant: Virtual dispatch replaces callbacks** (R24/R26)
  - `addSwitch()`, `addSensor()`, etc. no longer accept callback parameters
  - Command handling via `handleCommand()` virtual method override or `ha/command` EventBus topic
  - All HA entity callbacks removed — migrate to `HACommandEvent` event subscription

### Migration from 1.x

**Before (callbacks):**
```cpp
ha.addSwitch("relay", "Relay", [](bool state) { digitalWrite(PIN, state); });
```

**After (EventBus):**
```cpp
ha.addSwitch("relay", "Relay");
core.on<HACommandEvent>("ha/command", [](const HACommandEvent* evt) {
    if (strcmp(evt->entityId, "relay") == 0) {
        digitalWrite(PIN, evt->payload[0] == '1');
    }
});
```

### Bug Fixes

- fix(examples): replace `char[]` direct assignment with setter calls for HAConfig
- fix(ci): rewrite `clean_examples.py` to eliminate recursive `.pio` nesting
- fix(ci): prevent recursive `.pio` nesting and sconsign corruption
- fix(core,wifi,storage): resolve pre-existing test/build failures

### Component Versions

| Component | Version |
|-----------|---------|
| **DomoticsCore** (root) | 2.0.0 |
| **DomoticsCore-HomeAssistant** | 2.0.0 |
| All other components | unchanged from 1.9.0 |

---

## [1.9.0] - 2026-03-07

### New Features

- **HomeAssistant: Alarm Control Panel** — full MQTT discovery support with ARM/DISARM/PENDING/TRIGGERED states
- **Storage: Change notifications** — `EVENT_CHANGED` + `StorageChangedEvent` emitted on value changes (M15)
- **Core: `Core::emit()` sticky parameter** — parity with `IComponent::emit()` (M11)

### Code Quality (Roadmap Remediation)

#### Memory Safety (Constitution XIV)
- fix(core): complete `EventBus::reset()` — now clears `wildcardTopicSubscriptions`, `lastByTopic`, `pendingByTopic` (M9)
- fix(core): `unsubscribe()`/`unsubscribeOwner()` now cover wildcard subscriptions (M10)
- fix(memory): `shrink_to_fit()` after all container `erase()`/`clear()` operations — EventBus, MQTT queue, RemoteConsole clients (R1, R2, R4)
- fix(ha): replace String concatenation in topic generation with `snprintf()` zero-heap API (R5)
- feat(ha): HAConfig migrated from `String` fields to fixed-size `char[]` arrays — zero heap fragmentation on ESP8266 (R6)
- fix(system,rc,storage): replace String concatenation in log hot paths with `snprintf()` + static buffers (R7)

#### HAL Isolation (Constitution IX)
- fix(system): replace direct `millis()` with `HAL::Platform::getMillis()` (R8/M16)
- fix(rc): replace blocking `HAL::delay(100)` in reboot handler with non-blocking reboot flag (R9)

#### Dead Code Removal (Constitution IV)
- fix(mqtt): remove unimplemented `isValidTopic()` declaration (R17)
- fix(mqtt): enforce `maxQueueSize`, `publishRateLimit`, `maxSubscriptions` config fields (R18)
- fix(ntp): remove unused `retryDelayMs` config field (R19)
- fix(system): wire `otaPassword` config to OTAConfig (R20)
- fix(rc): implement `requireAuth`/`password`/`allowCommands` enforcement (R21)
- fix(led): remove dead `effectDirection` field (R22)
- fix(storage): remove dead WebUI `#if` block (R23)

#### Anti-Patterns (Constitution XIII)
- docs: document MemoryManager singleton as accepted exception (R14)
- docs: document MQTT static instance as accepted exception (R15)
- docs: document System `__has_include()` as intentional deviation (R16)

#### Bug Fixes
- fix(ha): `HAEntityAddedEvent` struct used in all `addXxx()` methods — consistent event emission (M19, C21)
- fix(led): `metadata.name` changed from `"LEDComponent"` to `"LED"` (M12)
- fix(core): `ResetReason` enum class with type-safe operators (R25)

### Component Versions

| Component | Version |
|-----------|---------|
| **DomoticsCore** (root) | 1.9.0 |
| **DomoticsCore-Core** | 1.5.2 |
| **DomoticsCore-HomeAssistant** | 1.6.1 |
| **DomoticsCore-LED** | 1.4.0 |
| **DomoticsCore-Storage** | 1.4.2 |
| **DomoticsCore-MQTT** | 1.4.1 |
| **DomoticsCore-RemoteConsole** | 1.4.1 |
| **DomoticsCore-System** | 1.4.1 |
| **DomoticsCore-OTA** | 1.4.1 |
| **DomoticsCore-NTP** | 1.3.0 |
| **DomoticsCore-WiFi** | 1.4.1 |

---

## [1.6.0] - 2026-02-11

### New Features

- **ESP32-C3 full support**: USB CDC serial, platform detection, all components validated
- **MemoryManager**: Device-agnostic memory adaptation for ESP32/ESP8266/native platforms
- **HeapTracker**: Memory leak detection infrastructure for native and hardware testing
- **WebUI SSE dual-mode**: Server-Sent Events transport alongside WebSocket
- **RemoteConsole WebUI**: Web-based console configuration (port, log level controls)
- **Filesystem HAL**: Unified filesystem abstraction (SPIFFS/LittleFS) across platforms
- **Chart field type**: Native canvas-based real-time charts in WebUI frontend

### ESP8266 Support

- Full HAL abstractions for WiFi, Storage (LittleFS), NTP, SystemInfo, LED, MQTT, OTA, RemoteConsole
- WebUI optimized for ~80KB RAM constraint
- Memory leak tests for ESP8266 heap validation
- WiFi mode icons and STA activation fixes

### Bug Fixes

- **RemoteConsole**: Replace `std::deque` with circular buffer to prevent memory leak
- **WebUI**: Fix timezone dropdown, rate limiting, password masking
- **WebUI**: Fix memory leaks with zero-copy `getContextAtRef` schema generation
- **WebUI**: Optimize memory with `const char*` fields and schema caching
- **Platform_HAL**: Fix `DOMOTICS_LOG_BUFFER_SIZE` default after platform includes
- **MQTT**: Refactor event structures to use fixed-size buffers for safe EventBus memcpy

### Refactoring

- **SystemInfo**: Merge static info into `system_info` context, optimize `getWebUIData()`
- **NTP**: Merge dashboard context into header info, reduce to 2 contexts
- **Events**: Split component events into dedicated headers per module
- **HAL**: Extract common Arduino utilities to `Platform_Arduino.h`
- **Examples**: Remove `ARDUINOJSON_ENABLE_PROGMEM=0` flag, replace `Arduino.h` with HAL abstractions

### Component Versions

| Component | Version |
|-----------|---------|
| **DomoticsCore** (root) | 1.6.0 |
| **DomoticsCore-Core** | 1.5.0 |
| **DomoticsCore-WebUI** | 1.5.0 |
| **DomoticsCore-Wifi** | 1.4.1 |
| **DomoticsCore-System** | 1.4.1 |
| **DomoticsCore-Storage** | 1.4.1 |
| **DomoticsCore-RemoteConsole** | 1.4.1 |
| **DomoticsCore-OTA** | 1.4.1 |
| **DomoticsCore-MQTT** | 1.4.0 |
| **DomoticsCore-HomeAssistant** | 1.4.0 |
| **DomoticsCore-SystemInfo** | 1.4.0 |
| **DomoticsCore-NTP** | 1.3.0 |
| **DomoticsCore-LED** | 1.3.0 |

---

## [1.5.0] - 2025-12-18

### 🔧 Bug Fixes

- **EventBus cleanup**: `ComponentRegistry::shutdownAll()` and `removeComponent()` now properly call `unsubscribeOwner()` to prevent memory leaks
- **Storage HAL**: Fixed `sprintf` → `snprintf` for bounds safety in hex encoding
- **bump_version.py**: Fixed regex group reference bug with version numbers containing digits

### 🧪 Testing Infrastructure

- **Isolated unit tests**: 37 tests across 5 components (NTP:7, MQTT:7, HA:7, WebUI:8, OTA:8)
- **Mock infrastructure**: Created mocks for WiFiHAL, MQTTClient, EventBus, Storage, NTPClient, AsyncWebServer
- **CI integration**: `local_ci.sh` now runs isolated tests automatically
- All tests run on native platform without hardware/network dependencies

### 🛠️ Development Tools

- **spec-kit integration**: Added `.windsurf/workflows/` with speckit workflows for specification-driven development
  - `/speckit.specify` - Create/update feature specifications
  - `/speckit.plan` - Generate implementation plans
  - `/speckit.tasks` - Generate actionable task lists
  - `/speckit.implement` - Execute implementation with progress tracking
  - `/speckit.analyze` - Cross-artifact consistency analysis
  - `/speckit.checklist` - Custom checklist generation
  - `/speckit.clarify` - Specification clarification
  - `/speckit.constitution` - Project constitution management

### 📋 Known Issues (Documented)

- **EventBus not thread-safe**: No mutex protection on ESP32 dual-core (low risk - callbacks run on same core)
- **const_cast in MQTT**: PubSubClient::connected() not const (library limitation)
- **Lambda [this] captures**: Safe when component lifecycle managed by Core

### 📦 Version Bumps

- **DomoticsCore-Core**: 1.3.0 → 1.4.0 (minor - EventBus cleanup feature)
- **DomoticsCore-Storage**: 1.3.2 → 1.3.3 (patch - sprintf fix)
- **Root library**: 1.4.0 → 1.5.0 (minor)

---

## [1.4.0] - 2025-12-17

### ✨ New Features

#### BootDiagnostics (SystemInfo + System + Storage)

Persistent boot diagnostics for debugging unexpected reboots without serial access.

**Architecture:**
- **SystemInfo**: Captures volatile data at boot (reset_reason, heap)
- **System**: Orchestrates persistence via Storage component
- **Storage**: Persists boot_count and reset info (uses HAL abstraction)

**Data Captured/Persisted:**
- `boot_count`: Incrementing boot counter (persisted via Storage)
- `last_reset_reason`: ESP32 reset reason code (captured + persisted)
- `last_boot_heap`: Free heap at boot time (captured + persisted)
- `last_boot_min_heap`: Minimum free heap at boot (captured + persisted)

**Reset Reason Mapping:**
Human-readable strings for all ESP32 reset reasons:
- Power-on, Software reset, Panic/Exception
- Interrupt watchdog, Task watchdog, Other watchdog
- Deep sleep wake, Brownout, External reset, SDIO reset

**API:**
```cpp
const BootDiagnostics& diag = systemInfo->getBootDiagnostics();
Serial.printf("Boot #%u, Reset: %s\n", diag.bootCount, diag.getResetReasonString().c_str());
if (diag.wasUnexpectedReset()) {
    Serial.println("Warning: Previous boot ended unexpectedly!");
}
```

**RemoteConsole Command:**
```
> bootdiag
Boot Diagnostics:
  Boot Count: 42
  Reset Reason: Power-on
  Boot Heap: 245760 bytes
  Boot Min Heap: 245760 bytes

Persisted Data:
  boot_count: 42
  last_reset: 1
  last_heap: 245760
  last_minheap: 245760
```

**Configuration:**
```cpp
SystemInfoConfig config;
config.enableBootDiagnostics = true;   // Enable capture (default: true)
// Persistence handled automatically by System via Storage component
```

### 🐛 Bug Fixes

#### HomeAssistant: `isReady()` Always Returning False (Issue #2)

**Problem:** `isReady()` always returned `false` because `availabilityPublished` was never set to `true` after successfully publishing availability.

**Fix:** Added `availabilityPublished = available;` in `setAvailable()` after successful MQTT publish.

**Impact:** Code relying on `isReady()` now correctly reflects component readiness.

### ⚡ Performance

#### Storage: Reduced Periodic Log Verbosity (Issue #1)

**Problem:** Storage status logs were emitted every 30 seconds, creating noise and potentially masking important warnings.

**Fix:** Increased `statusTimer` interval from 30 seconds to 5 minutes (300s), matching the maintenance timer.

### 🔄 Component Versions

| Component | Previous | New |
|-----------|----------|-----|
| **DomoticsCore** | 1.3.3 | 1.4.0 |
| **DomoticsCore-System** | 1.3.1 | 1.4.0 |
| **DomoticsCore-SystemInfo** | 1.3.0 | 1.4.0 |
| **DomoticsCore-HomeAssistant** | 1.2.2 | 1.2.3 |
| **DomoticsCore-Storage** | 1.3.1 | 1.3.2 |

### 📚 References

- Closes #1 (BootDiagnostics + Storage verbosity)
- Closes #2 (HomeAssistant isReady() bug)

---

## [1.3.3] - 2025-12-12

### 🐛 Bug Fixes

- PlatformIO Registry: include WebUI build script (`DomoticsCore-WebUI/embed_webui.py`) and WebUI sources (`DomoticsCore-WebUI/webui_src`) in the exported package

## [1.3.2] - 2025-12-03

### ⚡ Performance

- WebUI: use a static 8KB buffer for WebSocket updates

### ✨ Improvements

- WebUI: add JS/CSS/HTML minification before gzip embedding

### 🐛 Bug Fixes

- MQTT: fix enabled persistence and storage key registration

### 📝 Notes

- Release history around 1.3.x was rebuilt during stabilization; `v1.3.3` is the authoritative tag for the current `main` lineage.

## [1.3.0] - 2025-11-30

### 🏗️ Architecture: Hardware Abstraction Layer (HAL)

Complete HAL refactoring for ESP8266 portability preparation.

#### HAL Files Structure
- **Platform_HAL.h**: Platform detection macros, `getChipId()`, `restart()`, `getFreeHeap()`, `getChipModel()`, `getChipRevision()`, `getCpuFreqMHz()`, `SHA256` class
- **Wifi_HAL.h**: WiFi functions + `NetworkClient`, `SecureNetworkClient` type aliases
- **Storage_HAL.h**: `PlatformStorage` abstraction (Preferences/LittleFS)
- **SystemInfo_HAL.h**: System metrics abstraction
- **NTP_HAL.h**: Time synchronization abstraction

#### Architectural Improvements
- **All platform conditionals** (`#if DOMOTICS_PLATFORM_*`) now only in `*_HAL.h` files
- **MQTT storage removed**: Config persistence now handled by SystemPersistence (consistent with other components)
- **No direct ESP/WiFi calls** outside HAL files
- **Component dependencies updated**: MQTT and RemoteConsole now depend on Wifi component

### 🔧 Components Updated (all to v1.3.0)
- Core, MQTT, OTA, WiFi, Storage, System, WebUI, RemoteConsole, SystemInfo, NTP

### 📦 Dependency Changes
- `DomoticsCore-MQTT` now requires `DomoticsCore-Wifi`
- `DomoticsCore-RemoteConsole` now requires `DomoticsCore-Wifi`

### 🐛 Bug Fixes
- Fixed cross-component HAL dependencies in examples
- Fixed `getChipRevision()` missing from Platform_HAL

---

## [1.2.1] - 2025-11-14

### 🚨 Breaking Changes

**EventBus-Only Communication** - MQTT and HomeAssistant components now use EventBus exclusively

#### MQTT Component
- **REMOVED:** Direct callback methods (`onConnect`, `onDisconnect`, `onMessage`)
- **NEW:** EventBus-based communication via topics
  - Emits: `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`
  - Listens: `mqtt/publish`, `mqtt/subscribe`
- **Migration:** Use `core.on<>()` and `core.emit()` for event handling
- **See:** `MIGRATING_TO_V1.2.md` for detailed migration guide

#### HomeAssistant Component
- **REMOVED:** `MQTTComponent*` parameter from constructor
- **NEW:** EventBus-based MQTT communication (no direct dependency)
- **Before:** `HomeAssistantComponent(mqttPtr, haConfig)`
- **After:** `HomeAssistantComponent(haConfig)`
- **Impact:** Fully decoupled from MQTT component

### ✨ New Features

#### Core EventBus Helpers
- **NEW:** `Core::on<>()` and `Core::emit()` methods for cleaner API
- **Before:** `core.getEventBus().subscribe()` / `core.getEventBus().publish()`
- **After:** `core.on<bool>("topic", callback)` / `core.emit("topic", data)`
- **Impact:** Consistent API between Core and IComponent

### 🐛 Critical Bug Fixes

#### Bug #1: EventBus Listener Registration
- **Problem:** EventBus listeners registered AFTER configuration checks
- **Impact:** Discovery/subscriptions failed after WebUI configuration (reboot required)
- **Fix:** Listeners now registered BEFORE configuration checks
- **Result:** Discovery works immediately after WebUI config (no reboot needed)

#### Bug #2: PubSubClient Callback Registration  
- **Problem:** `mqttClient.setCallback()` called AFTER configuration checks
- **Impact:** Incoming MQTT messages silently dropped (switch commands not received)
- **Fix:** Callback registered BEFORE configuration checks
- **Result:** All MQTT messages properly received

### ✨ Improvements

- **Architecture:** Complete EventBus decoupling for inter-component communication
- **API Consistency:** Same `on<>()` / `emit()` API in Core and IComponent
- **Modularity:** Components can be tested independently
- **Maintainability:** Reduced tight coupling and component dependencies
- **Examples:** Updated all MQTT/HomeAssistant examples to use EventBus
- **Documentation:** Added comprehensive EventBus architecture guide

### 📚 Documentation

- **NEW:** `docs/reference/eventbus-architecture.md` - Complete EventBus reference
- **NEW:** `docs/migration/v1.2.0.md` - Migration guide from v1.1.x
- **REORGANIZED:** Documentation structure with guides/, migration/, reference/ folders
- **UPDATED:** All component examples with EventBus usage
- **UPDATED:** Component README files with new APIs

### 🔄 Component Versions

- **DomoticsCore-MQTT:** 1.0.1 → 1.2.0
- **DomoticsCore-HomeAssistant:** 1.0.2 → 1.2.0  
- **DomoticsCore-System:** 1.0.2 → 1.2.0
- **DomoticsCore-Core:** 1.1.4 → 1.2.0

### 📦 Migration Required

This is a breaking release. See [Migration Guide](docs/migration/v1.2.1.md) for step-by-step migration instructions.

---

## [1.1.4] - 2025-11-07

### 🔧 Architecture Improvements

**Unified Component Metadata System** - Simplified component identification

#### Changes
Refactored component metadata initialization for consistency and proper dependency resolution.

**Before (inconsistent):**
- Some components: `getName()` override returning hardcoded string
- Other components: `metadata.name` in `begin()` (too late for dependency resolution)
- Mixed approach created confusion and potential bugs

**After (unified):**
```cpp
// All components now initialize metadata in constructor
ComponentName() {
    metadata.name = "ComponentName";
    metadata.version = "1.0.0";
    metadata.author = "DomoticsCore";
    metadata.description = "Component description";
}
```

#### Benefits
- ✅ **Consistency:** All components use same pattern
- ✅ **Early availability:** Metadata ready before dependency resolution
- ✅ **Cleaner API:** Removed redundant `getName()` and `getVersion()` overrides
- ✅ **Direct access:** `component->metadata.name` instead of `component->getName()`
- ✅ **Simpler:** One source of truth (metadata struct)

#### Components Updated
All components now follow unified pattern:
- LED, Storage, Wifi, NTP, WebUI, OTA, MQTT, HomeAssistant
- SystemInfo, RemoteConsole, System

#### Technical Details
- Made `metadata` and `config` public in `IComponent` for external access
- Removed `getName()` and `getVersion()` virtual methods
- All metadata initialization moved to constructors
- Updated all WebUI providers to use `metadata.name` and `metadata.version`
- Added `eventBus()` helper method to `IComponent` for direct EventBus API access

**Result:** Cleaner, more maintainable codebase with proper early metadata initialization.

#### Examples Tested
All examples compile and work correctly:
- ✅ FullStack (1.38MB flash, 88.2%)
- ✅ WebUIOnly (1.04MB flash, 79.6%)
- ✅ CoreWithDummyComponent (320KB flash, 24.4%)
- ✅ 03-EventBusBasics (313KB flash, 23.9%)
- ✅ 04-EventBusCoordinators (315KB flash, 24.0%)
- ✅ 05-EventBusTests (315KB flash, 24.1%)

---

## [1.1.3] - 2025-11-01

### ✨ New Features

**Storage Namespace Configuration** - Project isolation for NVS storage

#### Feature
Configurable storage namespace for better project isolation on shared devices.

**System-level:**
```cpp
SystemConfig sysConfig = SystemConfig::standard();
sysConfig.storageNamespace = "watermeter";  // Custom namespace instead of "domotics"
System system(sysConfig);
```

**Component-level:**
```cpp
StorageConfig config;
config.namespace_name = "myproject";
auto storage = new StorageComponent(config);
```

#### Benefits
- ✅ **Isolation:** Multiple projects on same device without key conflicts
- ✅ **Clarity:** Each project has its own namespace
- ✅ **Development:** Easier to test multiple firmwares
- ✅ **Backward compatible:** Defaults to "domotics"

#### Testing
New unit test: `DomoticsCore-Storage/tests/namespace-isolation/`
- Tests namespace isolation with two Storage instances
- Validates same keys store different values in different namespaces
- Tests all data types (String, Int, Bool, Float, ULong64)
- Validates clear() per namespace
- Automated pass/fail with assertion macros

**Test Results:**
- ✅ All namespace isolation tests pass
- ✅ FullStack example compiles with namespace config
- ✅ Backward compatible (defaults to "domotics")

---

## [1.1.2] - 2025-11-01

### 🐛 Bug Fixes

**Fixed:** WifiComponent AP mode broken after v1.1.1 refactoring

#### Problem
- AP configured and logs showed "AP started: FullStackDevice-ac89 (IP: 192.168.4.1)"
- But AP not visible/accessible, system showed IP: 0.0.0.0
- FullStack example affected (any config with AP + empty STA credentials)

#### Root Cause
In `Wifi.h::begin()`, the code always set `WiFi.mode(WIFI_STA)` at the beginning, overriding the AP mode that was configured earlier via `enableAP()`. When SSID was empty, `begin()` returned early without restoring the mode, leaving WiFi in inconsistent state.

#### Solution
Added AP mode detection in `begin()`:
```cpp
// If AP already enabled, don't override mode
if (ssid.isEmpty() && apEnabled) {
    DLOG_I(LOG_WIFI, "No STA credentials - AP-only mode");
    return ComponentStatus::Success;  // Keep AP mode
}
```

**Result:** AP mode now works correctly with empty STA credentials.

#### Testing
- ✅ FullStack example: AP visible at 192.168.4.1
- ✅ WebUI accessible at http://192.168.4.1:80
- ✅ Telnet accessible at 192.168.4.1:23

---

## [1.1.1] - 2025-10-31

### 🎯 Eliminated Early-Init Requirement

**Major Improvement:** Refactored WifiComponent and System to eliminate Storage early-init pattern.

#### WifiComponent Enhancements

**New Features:**
```cpp
// Constructor now accepts empty credentials
WifiComponent wifi;  // Create without credentials
wifi.setCredentials("MySSID", "password");  // Configure later

// Automatic connection in afterAllComponentsReady()
void afterAllComponentsReady() override {
    // Connects to WiFi if credentials were set via setCredentials()
}
```

**Changes:**
- Constructor parameter `ssid` is now optional (defaults to empty)
- Added `setCredentials(ssid, password)` method for late configuration
- `begin()` skips connection if SSID is empty (waits for setCredentials)
- `afterAllComponentsReady()` connects if credentials set after begin()

#### System Improvements

**Before (v1.1.0):** Storage early-init required
```cpp
// Storage initialized BEFORE core.begin() to load WiFi credentials
storage->begin();  // Early-init
storage->setActive(true);
config.wifiSSID = storage->getString("wifi_ssid", "");
auto wifi = new WifiComponent(config.wifiSSID, config.wifiPassword);
core.begin();
```

**After (v1.1.1):** Clean component initialization
```cpp
// All components registered normally
core.addComponent(std::make_unique<StorageComponent>());
core.addComponent(std::make_unique<WifiComponent>());  // Empty credentials OK
core.begin();  // All components initialized in dependency order

// Load credentials AFTER core.begin() (Storage is ready)
auto* storage = core.getComponent<StorageComponent>("Storage");
auto* wifi = core.getComponent<WifiComponent>("Wifi");
wifi->setCredentials(storage->getString("wifi_ssid", ""), 
                     storage->getString("wifi_password", ""));
```

**Result:**
- ✅ Storage early-init eliminated
- ✅ Only LED early-init remains (justified for boot error visualization)
- ✅ Cleaner code, easier to understand
- ✅ Better separation of concerns

#### Documentation Updates

- Roadmap: Marked early-init elimination as implemented
- Technical notes: Updated to reflect new pattern
- Examples: All compile successfully with new pattern

#### Testing

- ✅ Minimal example: 860KB flash, 45KB RAM
- ✅ Backward compatible: Existing code with credentials in constructor still works
- ✅ New pattern validated: Empty credentials + setCredentials() works

---

## [1.1.0] - 2025-10-31

### ✨ New Features

#### Lifecycle Callback: afterAllComponentsReady()

**Problem Solved:** Need clean separation between internal init and dependency setup.

**New Lifecycle Hook:**
```cpp
class MyComponent : public IComponent {
    std::vector<Dependency> getDependencies() const override {
        return {{"Storage", false}, {"MQTT", false}};  // Optional dependencies
    }
    
    ComponentStatus begin() override {
        // Internal initialization only (GPIO, state)
        pinMode(PIN, INPUT);
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        // ALL components guaranteed available here!
        storage_ = getCore()->getComponent<StorageComponent>("Storage");
        mqtt_ = getCore()->getComponent<MQTTComponent>("MQTT");
        // Framework logs if optional dep missing
    }
};
```

**Benefits:**
- Clear separation: `begin()` = internal init, `afterAllComponentsReady()` = dependency setup
- Framework guarantees all components (including built-ins) are ready
- More intuitive than defensive null checks everywhere
- Non-breaking: virtual with default empty implementation

**Lifecycle Order:**
1. `begin()` - Internal initialization (all components)
2. `afterAllComponentsReady()` - Dependency setup (all components)
3. `loop()` - Normal operation

**Files Changed:**
- `IComponent.h`: Added `afterAllComponentsReady()` virtual method
- `ComponentRegistry.h`: Call hook after all components initialized

---

## [1.0.3] - 2025-10-31

### ✨ New Features

#### Optional Dependencies Support

**Problem Solved:** Custom components couldn't declare built-in dependencies without workarounds.

**Enhanced `getDependencies()` API:**
```cpp
// Simple case - all required (implicit)
std::vector<Dependency> getDependencies() const override {
    return {"ComponentA", "ComponentB"};  // Implicit required=true
}

// Advanced case - mix of required and optional
std::vector<Dependency> getDependencies() const override {
    return {
        {"Storage", false},      // Optional - won't fail if missing
        {"MQTT", false},         // Optional  
        {"MyCustomComp", true}   // Required - init fails if missing
    };
}
```

**Benefits:**
- Explicit intent (required vs optional)
- Framework logs informative messages for missing optional deps
- Implicit conversion from `String` for backward compatibility
- Better than defensive null checks everywhere

**Implementation:**
- Added `Dependency` struct with `name` and `required` flag
- Implicit conversion from `String` (defaults to required=true)
- ComponentRegistry checks optional deps but doesn't fail
- Logs INFO when optional dep missing, ERROR for required

**Files Changed:**
- `IComponent.h`: Enhanced `getDependencies()` to return `Dependency` objects
- `ComponentRegistry.h`: Updated dependency resolution to support optional deps

---

## [1.0.2] - 2025-10-30

### ✨ Lazy Core Injection - Enhanced Flexibility

**Problem Solved:** Components can now be registered at any time, even after `begin()`!

#### 🎯 Technical Solution

Implemented **lazy Core injection** to eliminate registration order constraints:

**Before (rigid order required):**
```cpp
// Had to register before begin()
core.addComponent(...);  // MUST be here
core.begin();            // Or crash!
```

**After (flexible - works anytime):**
```cpp
// Can register anytime!
core.begin();
// ... later ...
core.addComponent(...);  // Works perfectly! Core injected on first getCore() call
```

**How it works:**
1. `ComponentRegistry` is injected immediately when component is registered
2. `getCore()` uses lazy injection - fetches Core from registry on first access
3. Zero overhead: one `if` check per `getCore()` call
4. Memory cost: +4 bytes per component for registry pointer

**Files Changed:**
- `DomoticsCore-Core/include/DomoticsCore/IComponent.h` - Added `__dc_registry` member
- `DomoticsCore-Core/src/IComponent.cpp` - Lazy `getCore()` implementation
- `DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h` - Inject registry on registration

#### 📚 Documentation

Updated `docs/CUSTOM_COMPONENTS.md`:
- Enhanced `getDependencies()` limitations section
- Component initialization timeline diagram
- Defensive programming patterns for built-in dependencies
- Removed registration order warnings (no longer needed!)

**Key Points:**
- `getDependencies()` works for custom → custom dependencies
- Use null checks for built-in components (Storage, MQTT, WiFi, etc.)
- Built-ins are registered during `begin()`, customs before

### 🎓 Real-World Validation

**From Production Use (WaterMeter Project):**
- ✅ ESP32 WaterMeter v0.5.1 running in production
- ✅ Lazy injection tested and verified
- ✅ Zero crashes, perfect stability
- ✅ Flexibility without complexity

### 🚀 Benefits

- **Zero crashes**: No more `nullptr` access errors
- **Developer friendly**: Register components in any order
- **Minimal overhead**: Single `if` check per `getCore()` call
- **Backward compatible**: Existing code works without changes
- **Production proven**: Real-world tested

---

## [1.0.1] - 2025-10-30

### ✨ New Features

#### Core Access in Components (HIGH Priority)
- **Automatic Core Injection**: Components now receive automatic Core reference via `getCore()`
- **No More Boilerplate**: Eliminates manual `setCore()` calls in user projects
- **Consistent Pattern**: Same injection mechanism as EventBus (`__dc_core`)
- **Backward Compatible**: Fully additive, no breaking changes

**Before (Manual):**
```cpp
class MyComponent : public IComponent {
private:
    Core* core_ = nullptr;
public:
    void setCore(Core* c) { core_ = c; }
};
// In main.cpp - manual injection required
myComponent->setCore(&domotics->getCore());
```

**After (Automatic):**
```cpp
class MyComponent : public IComponent {
public:
    ComponentStatus begin() override {
        auto* storage = getCore()->getComponent<StorageComponent>("Storage");
        // Core automatically injected by framework!
    }
};
```

#### Storage uint64_t Support (MEDIUM Priority)
- **Native uint64_t Methods**: Added `putULong64()` and `getULong64()`
- **Cleaner API**: No more blob workarounds for counters
- **Use Cases**: Pulse counters, timestamps, large values

**Before (Blob Workaround):**
```cpp
uint8_t buffer[8];
memcpy(buffer, &value, 8);
storage->putBlob("count", buffer, 8);
```

**After (Native Support):**
```cpp
storage->putULong64("pulse_count", g_pulseCount);
uint64_t count = storage->getULong64("pulse_count", 0);
```

### 📚 Documentation

- **New Guide**: `docs/CUSTOM_COMPONENTS.md` - Comprehensive guide for creating custom components
  - Basic component structure
  - Accessing other components with `getCore()`
  - ESP32 ISR best practices (IRAM requirements)
  - Storage patterns (simple types, uint64_t, binary data)
  - Event Bus communication
  - Non-blocking timers
  - Complete real-world example (Water Meter Component)

### 🔧 Improvements

- **ComponentRegistry**: Enhanced to inject Core reference during initialization
- **IComponent**: Added protected `__dc_core` member and public `getCore()` helper
- **Build System**: Improved error messages in `embed_webui.py`

### 📦 Installation

GitHub installation remains simple:
```ini
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.1

build_unflags = -std=gnu++11
build_flags = -std=gnu++14
board_build.partitions = min_spiffs.csv
```

### 🙏 Acknowledgments

These enhancements come from real-world production use in the WaterMeter project, demonstrating DomoticsCore's readiness for IoT applications.

---

## [1.0.0] - 2025-10-27

### 🎉 First Stable Release

This is the first stable release of DomoticsCore with a complete modular architecture.

### ✨ Features

#### Core Architecture
- **Modular Component System**: Header-only components with zero overhead for unused features
- **Dependency Resolution**: Automatic topological sorting for component initialization
- **Component Registry**: Clean lifecycle management (begin/loop/shutdown)
- **Event Bus**: Inter-component communication with sticky events
- **Status Monitoring**: Component health tracking and diagnostics

#### Components Available

- **Core**: Essential framework with component registry and event bus
- **System**: High-level system orchestration with state management
- **WiFi**: Network connectivity with AP fallback mode
- **LED**: Visual status indication with multiple effects (blink, fade, pulse, breathing, rainbow)
- **Storage**: Persistent data management using NVS Preferences
- **RemoteConsole**: Telnet-based debugging console with log streaming
- **WebUI**: Modern web interface with real-time updates via WebSocket
- **NTP**: Network time synchronization
- **MQTT**: Message broker integration with auto-reconnection
- **OTA**: Secure over-the-air firmware updates
- **HomeAssistant**: Auto-discovery integration
- **SystemInfo**: Real-time system metrics and monitoring

#### System Features

- **LED Error Indicators**: Visual feedback for system states
  - BOOTING: Fast blink (200ms)
  - WIFI_CONNECTING: Slow blink (1000ms)
  - WIFI_CONNECTED: Pulse/heartbeat (2000ms)
  - SERVICES_STARTING: Fade (1500ms)
  - READY: Breathing (3000ms)
  - **ERROR: Fast blink (300ms)** ⚠️
  - OTA_UPDATE: Solid on
  - SHUTDOWN: Off

- **Error State Handling**: Components continue running even when initialization fails, allowing LED indicators and console access for debugging

- **Chunked Transfer Encoding**: WebUI schema endpoint supports large responses (>40KB) via chunked HTTP transfer

### 🏗️ Architecture

- **Examples Provided**:
  - FullStack: Complete system with all components
  - CoreOnly: Minimal setup
  - Component-specific examples for each module

### 🔧 Technical

- **ESP32 Platform**: Optimized for ESP32 microcontrollers
- **Build System**: PlatformIO with proper library dependency management
- **C++14**: Modern C++ features with template-based component system
- **Memory Efficient**: Components only consume resources when used

### 📦 Package Structure

```
DomoticsCore/
├── DomoticsCore-Core/           # Essential framework
├── DomoticsCore-System/         # System orchestration
├── DomoticsCore-WiFi/           # Network connectivity
├── DomoticsCore-LED/            # LED indicators
├── DomoticsCore-Storage/        # Persistent storage
├── DomoticsCore-RemoteConsole/  # Telnet console
├── DomoticsCore-WebUI/          # Web interface
├── DomoticsCore-NTP/            # Time sync
├── DomoticsCore-MQTT/           # MQTT client
├── DomoticsCore-OTA/            # OTA updates
├── DomoticsCore-HomeAssistant/  # HA integration
└── DomoticsCore-SystemInfo/     # System monitoring
```

### 🐛 Bug Fixes

- Fixed LED error indicator not animating when system initialization fails
- Fixed WebUI schema endpoint failing with large payloads (>40KB)
- Fixed component loop not running in ERROR state
- Fixed chunked transfer encoding for AsyncWebServer responses

### 📝 Documentation

- Comprehensive README for each component
- Example applications demonstrating usage patterns
- API documentation in header files
- Architecture diagrams and design patterns

---

## Release Notes

This 1.0.0 release represents a complete, production-ready framework for ESP32 IoT applications. The modular architecture allows you to include only the components you need, keeping your binary size small and your code maintainable.

**Upgrade Path**: This is the first stable release. Future versions will maintain backward compatibility within the 1.x series.

**Breaking Changes**: None (initial release)

[1.0.0]: https://github.com/JN0V/DomoticsCore/releases/tag/v1.0.0
