# Adversarial review — OBS Lot A code (branch `obs-lot-a`, PR #59)

**Reviewed**: `git diff docs/obs-observability-roadmap...obs-lot-a` over
`DomoticsCore-Core`, `DomoticsCore-System`, `DomoticsCore-SystemInfo`,
2026-09-05, **after the PR was opened** — it should have run before, and
the maintainer's own review had already found the one Constitution IX
violation (the `#if DOMOTICS_PLATFORM_ESP8266` in `SystemInfo.h`, fixed in
`9b33e36`). **Lens**: adversarial, alone. 13 findings, three of which
would bite a user of the published library on the first release that
ships this lot.

**Verdict: the measurements are right and the tests are non-vacuous where
they exist, but the ESP32 watchdog arms one task and feeds another.**
Whoever drives `System::loop()` from anything but the Arduino `loopTask` —
their own FreeRTOS task, or a custom `main` — gets an error log per loop
and a panic thirty seconds after boot, **by default**. That is the
default-on regression the spec said had to be argued, and the argument
was made on the wrong task.

---

## Findings

### 1. The watchdog is armed on `loopTask` and fed from whatever task calls `System::loop()` — `Platform_ESP32.h` `enableLoopWatchdog` / `feedLoopWatchdog`
- **Trigger**: `enableLoopWDT()` subscribes the Arduino `loopTaskHandle`;
  `feedLoopWDT()` calls `esp_task_wdt_reset()`, which resets the **calling**
  task's entry (`esp32-hal-misc.c:110-115`). A sketch that runs
  `System::loop()` in its own task never feeds `loopTask`: `log_e("Failed to
  feed WDT")` on every call, then a panic 30 s after boot. Default on.
- **Guard**: subscribe the current task — `esp_task_wdt_add(NULL)` — in
  `enableLoopWatchdog()`, which is called from the same task that will call
  `loop()`, and reset it with `esp_task_wdt_reset()`; drop the Arduino
  wrappers.
- **Consequence**: the release announces a safety feature that panics the
  architectures it did not think of.

### 2. Arming is not verified — `Platform_ESP32.h` `enableLoopWatchdog`
- **Trigger**: `enableLoopWDT()` returns `void` and silently does nothing when
  `loopTaskHandle == NULL` (`CONFIG_AUTOSTART_ARDUINO` off); the HAL returns
  `true` regardless and System logs "armed".
- **Guard**: check `esp_task_wdt_add()`'s return and
  `esp_task_wdt_status(NULL) == ESP_OK` before returning true; at System
  level, WARN when the config asked and the platform supports it but it
  did not arm — which needs `supportsLoopWatchdog()` so ESP8266 stays quiet.
- **Consequence**: a log line that says "armed" over a watchdog that is not.

### 3. `inline bool loopWatchdogEnabled_` is C++17 in a header the examples compile as gnu++14 and Arduino defaults to gnu++11 — `Platform_ESP32.h`
- **Trigger**: verified with the xtensa g++: `warning: inline variables are
  only available with -std=c++17`. Compiles today by extension; a user
  with `-Werror` or a stricter toolchain does not.
- **Guard**: a function-local static accessor
  (`static bool& armed() { static bool v = false; return v; }`).
- **Consequence**: a build break for some users of a published library.

### 4. The IDF 4.4 signature of `esp_task_wdt_init(uint32_t, bool)` is gone in IDF 5 / Arduino 3.x — `Platform_ESP32.h`
- **Trigger**: IDF 5 takes an `esp_task_wdt_config_t*`; this header will not
  compile there. The library targets 2.0.x today and pins nothing.
- **Guard**: an `#if ESP_IDF_VERSION_MAJOR >= 5` branch inside the HAL
  (allowed there) using `esp_task_wdt_reconfigure`, or at least a recorded
  port point in the roadmap entry.
- **Consequence**: the first user on the 3.x core is stopped by this lot.

### 5. The TWDT timeout is changed globally and the CHANGELOG does not say so — `Platform_ESP32.h`, `SystemConfig.h`, CHANGELOG (#58)
- **Trigger**: `esp_task_wdt_init(seconds, true)` reconfigures the timeout for
  every subscribed task: on ESP32, `IDLE0` goes from 5 s to 30 s. The
  `SystemConfig` comment says "globally"; the release note does not.
- **Guard**: one sentence in the CHANGELOG's top note.
- **Consequence**: a starved idle task now takes 30 s to be noticed, and
  nobody was told.

### 6. The 30 s default was chosen without measuring the longest `System::loop()` iteration — `SystemConfig.h`
- **Trigger**: the soak measured idle. Nothing measured OTA's
  `Update.end()` verification, a synchronous WiFi scan, or LittleFS's first
  format under `System::loop()` on either ESP32.
- **Guard**: a max-iteration counter on the FullStack example through an
  OTA cycle and a scan on the WROOM-32D and the C3; write the number beside
  the default.
- **Consequence**: the default is an assumption dressed as a measurement.

### 7. The tracked-minimum branch is never executed natively — `Platform_Stub.h` `tracksMinFreeHeap`, `test_system_persistence.cpp`, `test_system_lifecycle.cpp`
- **Trigger**: the stub's `tracksMinFreeHeap()` is `constexpr false`, so
  `boot_minheap` is never written and `bootdiag` never prints "%lu bytes"
  in any native test; only the WROOM-32D exercised that branch.
- **Guard**: make the stub's value a seam (`minFreeHeapTrackedForTest`) and
  assert both branches.
- **Consequence**: the ESP32 half of OBS-6 has board-only coverage.

### 8. `resetReasonCaveatForTest` is a seam with no test — `Platform_Stub.h`, `SystemInfo.h`
- **Trigger**: the caveat path in `initBootDiagnostics()` is unexercised
  natively; logging the empty string, or the caveat twice, would pass.
- **Guard**: a test that scripts a caveat, captures the log through
  `LoggerCallbacks::addCallback`, and asserts exactly one INFO line — and
  zero when the caveat is empty.
- **Consequence**: a dead seam, and a path that only the nodemcuv2 saw.

### 9. `char buf[768]` on the ESP8266 `cont` stack — `System.h` `getBootDiagnostics`
- **Trigger**: 512 → 768 in a console handler; the ESP8266 user stack is
  4 KB and `vsnprintf` has its own frame. The added blocks never print on
  ESP8266 (no core dump), so the extra 256 B buy nothing there.
- **Guard**: measure the high-water mark with `ESP.getFreeContStack()`
  around the command on the nodemcuv2 before keeping 768; or build the
  report by appending sections to the `String`.
- **Consequence**: a stack overflow in the one command meant to explain
  crashes.

### 10. The reset registers are formatted from a second source — `SystemInfo.h` `initBootDiagnostics`, `Platform_Stub.h` `getResetInfoString`
- **Trigger**: the struct holds `resetDetail` and the log line calls
  `getResetInfoString()`, which re-reads the SDK; on the stub the two
  disagree (the string is the reason name), so the native "Reset detail:"
  line says "Panic/Exception" while the struct says `epc1=0x40201297`.
- **Guard**: format the log line from `resetDetail` (one source), keep
  `getResetInfoString()` for `bootdiag` only or drop it.
- **Consequence**: two truths about one death.

### 11. `valid = true` for a hardware WDT reset overstates certainty — `Platform_ESP8266.h` `getResetDetail`
- **Trigger**: `REASON_WDT_RST` (1) runs no code; the registers are whatever
  RTC held. One sample on the nodemcuv2 looked right (F7).
- **Guard**: two more measurements, or say "as reported by the SDK" in the
  line for reason 1.
- **Consequence**: an `epc1` trusted on one sample.

### 12. `persistBootDiagnostics` rewrites the LittleFS file on every call — `SystemPersistence.h`
- **Trigger**: `putInt` and `remove` each `save()` (`Storage_ESP8266.h:174-179`):
  three full-file rewrites per boot (four before this lot), up to six on the
  first boot after the upgrade. A crash loop pays them every cycle, as it
  already did.
- **Guard**: none in this lot; a sentence in OBS-3's design, where the dedup
  and the once-per-boot rule live.
- **Consequence**: unchanged wear, undocumented.

### 13. `loopWatchdogSeconds` splits the Storage block — `SystemConfig.h`
- **Trigger**: the field sits between `enableStorage` and
  `storageNamespace`.
- **Guard**: move it after the Storage fields.
- **Consequence**: a reader groups it with Storage.

---

## Verified, no finding

Constitution IX after `9b33e36` (no platform `#if` outside HAL files, counted);
PROGMEM for the caveat (`F()`); the ESP8266 `getResetDetail()` range 1–3;
`persistBootDiagnostics` under the same `__has_include` guard as its caller;
the two mutation checks (feed removed, removal dropped) both caught; C3
compatibility of `enableLoopWDT`/`feedLoopWDT`; the stub defaults
byte-identical to the previous behaviour.

---

## Findings as JSON (canonical shape)

```json
[
 {
  "lens": "adversarial",
  "location": "`Platform_ESP32.h` `enableLoopWatchdog` / `feedLoopWatchdog`",
  "trigger_condition": "The watchdog is armed on `loopTask` and fed from whatever task calls `System::loop()` — `enableLoopWDT()` subscribes the Arduino `loopTaskHandle`; `feedLoopWDT()` calls `esp_task_wdt_reset()`, which resets the **calling** task's entry (`esp32-hal-misc.c:110-115`). A sketch that runs `System::loop()` in its own task never feeds `loopTask`: `log_e(\"Failed to feed WDT\")` on every call, then a panic 30 s after boot. Default on.",
  "guard_snippet": "subscribe the current task — `esp_task_wdt_add(NULL)` — in `enableLoopWatchdog()`, which is called from the same task that will call `loop()`, and reset it with `esp_task_wdt_reset()`; drop the Arduino wrappers.",
  "potential_consequence": "the release announces a safety feature that panics the"
 },
 {
  "lens": "adversarial",
  "location": "`Platform_ESP32.h` `enableLoopWatchdog`",
  "trigger_condition": "Arming is not verified — `enableLoopWDT()` returns `void` and silently does nothing when `loopTaskHandle == NULL` (`CONFIG_AUTOSTART_ARDUINO` off); the HAL returns `true` regardless and System logs \"armed\".",
  "guard_snippet": "check `esp_task_wdt_add()`'s return and `esp_task_wdt_status(NULL) == ESP_OK` before returning true; at System level, WARN when the config asked and the platform supports it but it did not arm — which needs `supportsLoopWatchdog()` so ESP8266 stays quiet.",
  "potential_consequence": "a log line that says \"armed\" over a watchdog that is not."
 },
 {
  "lens": "adversarial",
  "location": "`Platform_ESP32.h`",
  "trigger_condition": "`inline bool loopWatchdogEnabled_` is C++17 in a header the examples compile as gnu++14 and Arduino defaults to gnu++11 — verified with the xtensa g++: `warning: inline variables are only available with -std=c++17`. Compiles today by extension; a user with `-Werror` or a stricter toolchain does not.",
  "guard_snippet": "a function-local static accessor (`static bool& armed() { static bool v = false; return v; }`).",
  "potential_consequence": "a build break for some users of a published library."
 },
 {
  "lens": "adversarial",
  "location": "`Platform_ESP32.h`",
  "trigger_condition": "The IDF 4.4 signature of `esp_task_wdt_init(uint32_t, bool)` is gone in IDF 5 / Arduino 3.x — IDF 5 takes an `esp_task_wdt_config_t*`; this header will not compile there. The library targets 2.0.x today and pins nothing.",
  "guard_snippet": "an `#if ESP_IDF_VERSION_MAJOR >= 5` branch inside the HAL (allowed there) using `esp_task_wdt_reconfigure`, or at least a recorded port point in the roadmap entry.",
  "potential_consequence": "the first user on the 3.x core is stopped by this lot."
 },
 {
  "lens": "adversarial",
  "location": "`Platform_ESP32.h`, `SystemConfig.h`, CHANGELOG (#58)",
  "trigger_condition": "The TWDT timeout is changed globally and the CHANGELOG does not say so — `esp_task_wdt_init(seconds, true)` reconfigures the timeout for every subscribed task: on ESP32, `IDLE0` goes from 5 s to 30 s. The `SystemConfig` comment says \"globally\"; the release note does not.",
  "guard_snippet": "one sentence in the CHANGELOG's top note.",
  "potential_consequence": "a starved idle task now takes 30 s to be noticed, and"
 },
 {
  "lens": "adversarial",
  "location": "`SystemConfig.h`",
  "trigger_condition": "The 30 s default was chosen without measuring the longest `System::loop()` iteration — the soak measured idle. Nothing measured OTA's `Update.end()` verification, a synchronous WiFi scan, or LittleFS's first format under `System::loop()` on either ESP32.",
  "guard_snippet": "a max-iteration counter on the FullStack example through an OTA cycle and a scan on the WROOM-32D and the C3; write the number beside the default.",
  "potential_consequence": "the default is an assumption dressed as a measurement."
 },
 {
  "lens": "adversarial",
  "location": "`Platform_Stub.h` `tracksMinFreeHeap`, `test_system_persistence.cpp`, `test_system_lifecycle.cpp`",
  "trigger_condition": "The tracked-minimum branch is never executed natively — the stub's `tracksMinFreeHeap()` is `constexpr false`, so `boot_minheap` is never written and `bootdiag` never prints \"%lu bytes\" in any native test; only the WROOM-32D exercised that branch.",
  "guard_snippet": "make the stub's value a seam (`minFreeHeapTrackedForTest`) and assert both branches.",
  "potential_consequence": "the ESP32 half of OBS-6 has board-only coverage."
 },
 {
  "lens": "adversarial",
  "location": "`Platform_Stub.h`, `SystemInfo.h`",
  "trigger_condition": "`resetReasonCaveatForTest` is a seam with no test — the caveat path in `initBootDiagnostics()` is unexercised natively; logging the empty string, or the caveat twice, would pass.",
  "guard_snippet": "a test that scripts a caveat, captures the log through `LoggerCallbacks::addCallback`, and asserts exactly one INFO line — and zero when the caveat is empty.",
  "potential_consequence": "a dead seam, and a path that only the nodemcuv2 saw."
 },
 {
  "lens": "adversarial",
  "location": "`System.h` `getBootDiagnostics`",
  "trigger_condition": "`char buf[768]` on the ESP8266 `cont` stack — 512 → 768 in a console handler; the ESP8266 user stack is 4 KB and `vsnprintf` has its own frame. The added blocks never print on ESP8266 (no core dump), so the extra 256 B buy nothing there.",
  "guard_snippet": "measure the high-water mark with `ESP.getFreeContStack()` around the command on the nodemcuv2 before keeping 768; or build the report by appending sections to the `String`.",
  "potential_consequence": "a stack overflow in the one command meant to explain"
 },
 {
  "lens": "adversarial",
  "location": "`SystemInfo.h` `initBootDiagnostics`, `Platform_Stub.h` `getResetInfoString`",
  "trigger_condition": "The reset registers are formatted from a second source — the struct holds `resetDetail` and the log line calls `getResetInfoString()`, which re-reads the SDK; on the stub the two disagree (the string is the reason name), so the native \"Reset detail:\" line says \"Panic/Exception\" while the struct says `epc1=0x40201297`.",
  "guard_snippet": "format the log line from `resetDetail` (one source), keep `getResetInfoString()` for `bootdiag` only or drop it.",
  "potential_consequence": "two truths about one death."
 },
 {
  "lens": "adversarial",
  "location": "`Platform_ESP8266.h` `getResetDetail`",
  "trigger_condition": "`valid = true` for a hardware WDT reset overstates certainty — `REASON_WDT_RST` (1) runs no code; the registers are whatever RTC held. One sample on the nodemcuv2 looked right (F7).",
  "guard_snippet": "two more measurements, or say \"as reported by the SDK\" in the line for reason 1.",
  "potential_consequence": "an `epc1` trusted on one sample."
 },
 {
  "lens": "adversarial",
  "location": "`SystemPersistence.h`",
  "trigger_condition": "`persistBootDiagnostics` rewrites the LittleFS file on every call — `putInt` and `remove` each `save()` (`Storage_ESP8266.h:174-179`): three full-file rewrites per boot (four before this lot), up to six on the first boot after the upgrade. A crash loop pays them every cycle, as it already did.",
  "guard_snippet": "none in this lot; a sentence in OBS-3's design, where the dedup and the once-per-boot rule live.",
  "potential_consequence": "unchanged wear, undocumented."
 },
 {
  "lens": "adversarial",
  "location": "`SystemConfig.h`",
  "trigger_condition": "`loopWatchdogSeconds` splits the Storage block — the field sits between `enableStorage` and `storageNamespace`.",
  "guard_snippet": "move it after the Storage fields.",
  "potential_consequence": "a reader groups it with Storage."
 }
]
```
