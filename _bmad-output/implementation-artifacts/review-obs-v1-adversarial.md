# Adversarial review — OBS spec v1 and roadmap Priority 11

**Reviewed**: `spec-obs-crash-observability.md` v1 and the uncommitted
"Priority 11: Observability" section of `docs/CODE-ROADMAP.md` (OBS-1 to
OBS-6), 2026-09-05, before any code was written. **Lens**: adversarial,
alone, as requested. 22 findings. Every claim below was checked against
the installed cores (arduino-esp32 2.0.17, arduino-esp8266 3.1.2) or the
repository; the ones that need a board say so.

**Verdict: the premise holds and the facts table is mostly right, but the
design's central rule would skip the failure it exists for.** On ESP8266 an
out-of-memory `new` does not produce a `Panic` reset; it produces the
abort path, which the next boot reports as something `wasUnexpectedReset()`
does not recognise. Promotion keyed on the reset reason is therefore blind
to OOM-by-`new`. Three other findings change lots or ownership: promotion
runs last when it must run first, the recorder is proposed inside an
optional component, and two hooks the design registers are single global
slots that collide with what users of a published library may already
have.

---

## Findings

### 1. The promotion rule skips the OOM death on ESP8266 — §3 L1, OBS-3
- **Trigger**: `operator new` on OOM calls `__unhandled_exception("OOM")`
  (`abi.cpp:38-46`) → `raise_exception()` sets
  `s_user_reset_reason = REASON_USER_SWEXCEPTION_RST` (254) and runs the
  postmortem (`core_esp8266_postmortem.cpp:318-338`). `postmortem_report`
  never writes `rst_info` back to RTC (`:132-146`), so the next boot's
  `rst_info.reason` is whatever the SDK stamps for a local restart —
  `Software` or `Unknown` in the HAL, neither in `wasUnexpectedReset()`
  (`Platform_ESP8266.h:255-261`). Same path for `abort()`, `assert`,
  `panic()`, `std::bad_alloc` from `__throw_*`.
- **Guard**: the promotion discriminator is the record's own "crash
  callback ran" flag, carrying the reason the callback *received* (253/254
  included), not the SDK reason. Measure the next-boot reason after
  `abort()` on the nodemcuv2 before finalising; write the measured value
  into F5.
- **Consequence**: the black box stays silent for the exact failure class
  the maintainer described.

### 2. F9 is wrong for `new`: the failed allocation is recorded on every build — §2 F9, OBS-4
- **Trigger**: `abi.cpp:42-43` sets `umm_last_fail_alloc_addr/size`
  unconditionally before aborting. Only the `malloc`/`calloc`/`realloc`
  paths (`heap.cpp:104-127`) are gated on `DEBUG_ESP_OOM`.
- **Guard**: split F9 into two rows. The crash callback copies both words
  on every build; the diagnostic profile adds the C-allocation sites and
  the `file:line` variant, nothing more.
- **Consequence**: the entry sends the maintainer to a diagnostic build for
  information a stock build already has.

### 3. OBS-2 re-implements `ESP.getResetInfo()`, and neither sees the abort class — OBS-2, §3 L3
- **Trigger**: `Esp.cpp:506-512` already formats exccause/epc1-3/excvaddr/
  depc — but only for reasons 1–3 (`REASON_WDT_RST..REASON_SOFT_WDT_RST`).
  Per finding 1 the OOM/abort class arrives under a different reason with
  nothing in `rst_info`.
- **Guard**: OBS-2 is one call for the log line plus the structured fields,
  and its entry must say what it cannot see. Lot A's promise changes from
  "exceptions or watchdogs, and where" to "exceptions or watchdogs;
  OOM-by-`new` stays invisible until Lot B".
- **Consequence**: the first lot is sold on a reading it cannot deliver for
  the suspected cause.

### 4. F3 overstates the fleet risk, in the wrong direction — §2 F3, OBS-1
- **Trigger**: of the 24 stock partition tables only `bare_minimum_2MB.csv`
  lacks `coredump`; PlatformIO's default for `esp32dev` is `default.csv`,
  which has it. The maintainer's fleet runs *his* project, not FullStack.
  Only a custom CSV (FullStack's pre-CI-9 one) loses the partition.
- **Guard**: rewrite F3 accordingly; the OBS-1 boot check is what turns the
  inference into one log line per device.
- **Consequence**: a serial reflash campaign for nothing.

### 5. Promotion runs last; it must run first — §3 L1, OBS-3
- **Trigger**: `SystemInfo` is the last component added (`System.h:396`)
  and `initBootDiagnosticsPersistence()` runs after `core.begin()`
  (`System.h:137-169`), i.e. after WiFi connect, MQTT, HA and OTA began. A
  device that dies again during bring-up — the crash loop — never
  promotes, and the next run's sampler overwrites the record.
- **Guard**: read RTC into a RAM staging copy as the first act of
  `System::begin()` (before any component), freeze sampling until
  promotion is done, write Storage when Storage is up, publish when MQTT
  connects.
- **Consequence**: the devices you most need to read are the ones that
  lose the record.

### 6. `SystemInfo` cannot own it — §7.4
- **Trigger**: §7.4 lists the objection and does not answer it.
  `SystemInfo` is optional (`__has_include`) and last; the recorder must be
  mandatory and first.
- **Guard**: RTC primitives in the Core HAL; sampler, crash hook and
  promotion in Core (or a mandatory piece of System); `SystemInfo` and the
  console only *display*.
- **Consequence**: a black box that vanishes when the dashboard component
  is left out.

### 7. ESP8266 RTC memory is word-addressable only — §3 L1 layout
- **Trigger**: the layout has `u8`/`u16` fields; the region behind
  `0x60001200` and the core's API (`uint32_t*`, offsets in words,
  `Esp.cpp:175-190`) are 32-bit only.
- **Guard**: pack every field into `u32` words and move whole words; say so
  in the layout.
- **Consequence**: the recorder corrupts itself or faults.

### 8. The phase marker's complement word is two stores, not one — §3 L1
- **Trigger**: a crash between the value store and the complement store
  discards the one datum the design calls "the most informative".
- **Guard**: one 32-bit store, `(v << 16) | (~v & 0xFFFF)`; the reader
  checks the halves.
- **Consequence**: the marker is unreliable at the moment it matters.

### 9. A 10 s sample misses the cliff — §3 L1
- **Trigger**: a heap that dies inside a 1 s burst (one JSON build, one
  schema chunk) shows a healthy sample 9 s before death. The repository's
  own lesson (`measure-transient-heap-two-samples`) applies.
- **Guard**: a RAM running minimum of free heap and largest block, updated
  every loop iteration; each tick flushes `{current, min-since-last}`.
- **Consequence**: the ring says "plateau, then reboot" for a cliff.

### 10. ESP32 heap "in KB" loses the resolution that matters — §3 L1 layout
- **Trigger**: below 1 KB every byte counts; PSRAM boards exceed the field.
- **Guard**: `u16` in 16-byte units, internal heap only
  (`MALLOC_CAP_INTERNAL`) — that is what dies.
- **Consequence**: three trailing samples that read "0 KB".

### 11. The ESP32 failed-alloc hook fires on attempts, not only on death — §3 L2, OBS-4
- **Trigger**: the hook signature carries `caps` and `function_name`
  (`esp_heap_caps.h:44-48`) because capability-based allocation tries
  regions in turn; WiFi/LWIP retry allocations routinely.
- **Guard**: a counter plus the last few `(size, caps, free)`; measure how
  often it fires on a healthy WROOM-32D before treating one call as a
  death signal.
- **Consequence**: false "OOM" verdicts.

### 12. Both hooks are single global slots; one is a link error for some users — §3 L1/L2
- **Trigger**: a strong `custom_crash_callback` in a library collides with
  every user of EspSaveCrash (multiple definition);
  `heap_caps_register_failed_alloc_callback` silently replaces a user's
  own hook. This library is installed by version.
- **Guard**: opt-in (`DOMOTICS_CRASH_HOOKS`) or a forwarding hook the user's
  own callback can chain; say it in the roadmap entry.
- **Consequence**: a minor release that fails to link.

### 13. Forced-crash commands are a remote reboot behind a brute-forceable password — §4, OBS-3
- **Trigger**: SEC-4 (open) — plaintext compare, no rate limit — and the
  commands are proposed as a production feature.
- **Guard**: compile-time gate, on in the test environments and the
  diagnostic profile, off by default; or make SEC-4 a dependency of OBS-3.
- **Consequence**: a DoS primitive with a debug label.

### 14. Promotion to Storage on ESP8266 costs heap for the life of the process — §3 L1
- **Trigger**: `LittleFSStorage::putBytes` hex-encodes into the resident
  JSON document (`Storage_ESP8266.h:141-150`): a 324-byte record becomes
  ~650 bytes of heap, permanently, and the whole file is rewritten.
- **Guard**: promote a compact summary (reason, build, uptime, epc1, phase,
  last-fail: ~40 bytes) or write the raw record as its own LittleFS file;
  keep the rings in RTC and publish them once.
- **Consequence**: the black box takes ~1.5 % of an ESP8266's free heap.

### 15. OBS-6's rename strands keys on deployed devices — OBS-6
- **Trigger**: `last_heap`/`last_minheap` remain in NVS/LittleFS forever;
  NVS has an entry budget.
- **Guard**: remove the old keys on the first boot of the new build; say so.
- **Consequence**: clutter that later reads as data.

### 16. OBS-5 ignores the HomeAssistant component — OBS-5
- **Trigger**: the consumer already exists — `addSensor()`
  (`HomeAssistant.h:189`) — and a `diagnostic`-category sensor for heap
  plus a "last crash" sensor with attributes gives history graphs and a
  device page with no Node-RED. The component has no entity-category
  support yet.
- **Guard**: route through HA discovery when the component is present,
  raw topics otherwise; per-entity opt-in as HA users expect.
- **Consequence**: a second ad-hoc topic scheme beside the discovery
  integration the same library ships.

### 17. Telemetry at 60 s can be the observer that kills the patient — OBS-5
- **Trigger**: a JSON build every minute on an ESP8266 near exhaustion,
  queued under BUG-29's rate limit, adds allocations exactly when none are
  left.
- **Guard**: fixed `char[]` + `snprintf`, no `String`; skip the tick under a
  heap floor and record that it skipped; "on by default" only if this
  holds.
- **Consequence**: STOR-ESP-1's lesson in reverse.

### 18. "On the next reboot" is at least one OTA campaign away — §5
- **Trigger**: Lot A's promise assumes the new build is on the fleet;
  devices rebooting every N hours must take an OTA inside the window.
- **Guard**: say it in §5.
- **Consequence**: expectations.

### 19. Build id ↔ ELF is asserted, not specified — §3 L1, §3 L3
- **Trigger**: "hash of the build string" is not recoverable from an ELF
  unless the same string is embedded in it and printed at boot.
- **Guard**: `-DDOMOTICS_BUILD_ID=<git short sha>` via an extra script,
  stored as a `.rodata` string, printed at boot, its CRC32 in the record.
- **Consequence**: a record nobody can decode.

### 20. Brownout survival is "to verify" with no plan, and brownout is a top candidate — §2 F11
- **Trigger**: `RTC_NOINIT_ATTR` is documented for restart and deep sleep
  only (`esp_attr.h:99-102`). The ESP32-CAM browns out on FTDI 3.3 V
  (CLAUDE.md) — a brownout rig for free.
- **Guard**: measure it there.
- **Consequence**: a blank record for the most likely cause on cheap
  supplies.

### 21. `crash wdt` is two commands on ESP8266 and an open decision on ESP32 — §4, F12
- **Trigger**: soft WDT (~3 s) reaches the postmortem and the callback;
  hardware WDT runs nothing. On ESP32 `enableLoopWDT()` is one line that
  turns a silent hang into a panic and therefore a core dump — arguably the
  largest single gain on that platform, and not filed.
- **Guard**: `crash swdt` / `crash hwdt`; measure F12; file the loop-WDT
  decision as its own item in Lot A.
- **Consequence**: an unmeasurable definition of done.

### 22. Lot A and the severities — §5, §7.7, roadmap intro
- **Trigger**: OBS-1's endpoint (chunked partition streaming, auth, erase)
  is not an afternoon; the boot check is. §7.7 asks the review to argue the
  severity instead of arguing it. The section intro says "adversarially
  reviewed before these entries were finalised" of text written before the
  review.
- **Guard**: Lot A = OBS-2 line + OBS-6 + OBS-1 *check only* + the ESP32
  loop-WDT decision; endpoint to Lot D. Severity: OBS-3/OBS-4 HIGH by
  consequence (the instrument for Constitution XIV against a live
  production defect) or all MEDIUM with priority carried by lot order —
  pick one, write why. Date the intro after amendment.
- **Consequence**: the first lot slips, and the roadmap gains another
  sentence that was true later.

---

## Verified independently, no finding

F1, F2, F5 (fields), F6, F8, F10 (eboot 32 words at the shared base, no
other RTC user in the core), F13, F14, F15, and the tracking-summary
arithmetic (6+4+6+4+3+1+5+10+0+5 = 44; 34+1 = 35; 132+6 = 138).

---

## Findings as JSON (canonical shape)

```json
[
 {
  "lens": "adversarial",
  "location": "§3 L1, OBS-3",
  "trigger_condition": "The promotion rule skips the OOM death on ESP8266 — `operator new` on OOM calls `__unhandled_exception(\"OOM\")` (`abi.cpp:38-46`) → `raise_exception()` sets `s_user_reset_reason = REASON_USER_SWEXCEPTION_RST` (254) and runs the postmortem (`core_esp8266_postmortem.cpp:318-338`). `postmortem_report` never writes `rst_info` back to RTC (`:132-146`), so the next boot's `rst_info.reason` is whatever the SDK stamps for a local restart — `Software` or `Unknown` in the HAL, neither in `wasUnexpectedReset()` (`Platform_ESP8266.h:255-261`). Same path for `abort()`, `assert`, `panic()`, `std::bad_alloc` from `__throw_*`.",
  "guard_snippet": "the promotion discriminator is the record's own \"crash callback ran\" flag, carrying the reason the callback *received* (253/254 included), not the SDK reason. Measure the next-boot reason after `abort()` on the nodemcuv2 before finalising; write the measured value into F5.",
  "potential_consequence": "the black box stays silent for the exact failure class"
 },
 {
  "lens": "adversarial",
  "location": "§2 F9, OBS-4",
  "trigger_condition": "F9 is wrong for `new`: the failed allocation is recorded on every build — `abi.cpp:42-43` sets `umm_last_fail_alloc_addr/size` unconditionally before aborting. Only the `malloc`/`calloc`/`realloc` paths (`heap.cpp:104-127`) are gated on `DEBUG_ESP_OOM`.",
  "guard_snippet": "split F9 into two rows. The crash callback copies both words on every build; the diagnostic profile adds the C-allocation sites and the `file:line` variant, nothing more.",
  "potential_consequence": "the entry sends the maintainer to a diagnostic build for"
 },
 {
  "lens": "adversarial",
  "location": "OBS-2, §3 L3",
  "trigger_condition": "OBS-2 re-implements `ESP.getResetInfo()`, and neither sees the abort class — `Esp.cpp:506-512` already formats exccause/epc1-3/excvaddr/ depc — but only for reasons 1–3 (`REASON_WDT_RST..REASON_SOFT_WDT_RST`). Per finding 1 the OOM/abort class arrives under a different reason with nothing in `rst_info`.",
  "guard_snippet": "OBS-2 is one call for the log line plus the structured fields, and its entry must say what it cannot see. Lot A's promise changes from \"exceptions or watchdogs, and where\" to \"exceptions or watchdogs; OOM-by-`new` stays invisible until Lot B\".",
  "potential_consequence": "the first lot is sold on a reading it cannot deliver for"
 },
 {
  "lens": "adversarial",
  "location": "§2 F3, OBS-1",
  "trigger_condition": "F3 overstates the fleet risk, in the wrong direction — of the 24 stock partition tables only `bare_minimum_2MB.csv` lacks `coredump`; PlatformIO's default for `esp32dev` is `default.csv`, which has it. The maintainer's fleet runs *his* project, not FullStack. Only a custom CSV (FullStack's pre-CI-9 one) loses the partition.",
  "guard_snippet": "rewrite F3 accordingly; the OBS-1 boot check is what turns the inference into one log line per device.",
  "potential_consequence": "a serial reflash campaign for nothing."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1, OBS-3",
  "trigger_condition": "Promotion runs last; it must run first — `SystemInfo` is the last component added (`System.h:396`) and `initBootDiagnosticsPersistence()` runs after `core.begin()` (`System.h:137-169`), i.e. after WiFi connect, MQTT, HA and OTA began. A device that dies again during bring-up — the crash loop — never promotes, and the next run's sampler overwrites the record.",
  "guard_snippet": "read RTC into a RAM staging copy as the first act of `System::begin()` (before any component), freeze sampling until promotion is done, write Storage when Storage is up, publish when MQTT connects.",
  "potential_consequence": "the devices you most need to read are the ones that"
 },
 {
  "lens": "adversarial",
  "location": "§7.4",
  "trigger_condition": "`SystemInfo` cannot own it — §7.4 lists the objection and does not answer it. `SystemInfo` is optional (`__has_include`) and last; the recorder must be mandatory and first.",
  "guard_snippet": "RTC primitives in the Core HAL; sampler, crash hook and promotion in Core (or a mandatory piece of System); `SystemInfo` and the console only *display*.",
  "potential_consequence": "a black box that vanishes when the dashboard component"
 },
 {
  "lens": "adversarial",
  "location": "§3 L1 layout",
  "trigger_condition": "ESP8266 RTC memory is word-addressable only — the layout has `u8`/`u16` fields; the region behind `0x60001200` and the core's API (`uint32_t*`, offsets in words, `Esp.cpp:175-190`) are 32-bit only.",
  "guard_snippet": "pack every field into `u32` words and move whole words; say so in the layout.",
  "potential_consequence": "the recorder corrupts itself or faults."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1",
  "trigger_condition": "The phase marker's complement word is two stores, not one — a crash between the value store and the complement store discards the one datum the design calls \"the most informative\".",
  "guard_snippet": "one 32-bit store, `(v << 16) | (~v & 0xFFFF)`; the reader checks the halves.",
  "potential_consequence": "the marker is unreliable at the moment it matters."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1",
  "trigger_condition": "A 10 s sample misses the cliff — a heap that dies inside a 1 s burst (one JSON build, one schema chunk) shows a healthy sample 9 s before death. The repository's own lesson (`measure-transient-heap-two-samples`) applies.",
  "guard_snippet": "a RAM running minimum of free heap and largest block, updated every loop iteration; each tick flushes `{current, min-since-last}`.",
  "potential_consequence": "the ring says \"plateau, then reboot\" for a cliff."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1 layout",
  "trigger_condition": "ESP32 heap \"in KB\" loses the resolution that matters — below 1 KB every byte counts; PSRAM boards exceed the field.",
  "guard_snippet": "`u16` in 16-byte units, internal heap only (`MALLOC_CAP_INTERNAL`) — that is what dies.",
  "potential_consequence": "three trailing samples that read \"0 KB\"."
 },
 {
  "lens": "adversarial",
  "location": "§3 L2, OBS-4",
  "trigger_condition": "The ESP32 failed-alloc hook fires on attempts, not only on death — the hook signature carries `caps` and `function_name` (`esp_heap_caps.h:44-48`) because capability-based allocation tries regions in turn; WiFi/LWIP retry allocations routinely.",
  "guard_snippet": "a counter plus the last few `(size, caps, free)`; measure how often it fires on a healthy WROOM-32D before treating one call as a death signal.",
  "potential_consequence": "false \"OOM\" verdicts."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1/L2",
  "trigger_condition": "Both hooks are single global slots; one is a link error for some users — a strong `custom_crash_callback` in a library collides with every user of EspSaveCrash (multiple definition); `heap_caps_register_failed_alloc_callback` silently replaces a user's own hook. This library is installed by version.",
  "guard_snippet": "opt-in (`DOMOTICS_CRASH_HOOKS`) or a forwarding hook the user's own callback can chain; say it in the roadmap entry.",
  "potential_consequence": "a minor release that fails to link."
 },
 {
  "lens": "adversarial",
  "location": "§4, OBS-3",
  "trigger_condition": "Forced-crash commands are a remote reboot behind a brute-forceable password — SEC-4 (open) — plaintext compare, no rate limit — and the commands are proposed as a production feature.",
  "guard_snippet": "compile-time gate, on in the test environments and the diagnostic profile, off by default; or make SEC-4 a dependency of OBS-3.",
  "potential_consequence": "a DoS primitive with a debug label."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1",
  "trigger_condition": "Promotion to Storage on ESP8266 costs heap for the life of the process — `LittleFSStorage::putBytes` hex-encodes into the resident JSON document (`Storage_ESP8266.h:141-150`): a 324-byte record becomes ~650 bytes of heap, permanently, and the whole file is rewritten.",
  "guard_snippet": "promote a compact summary (reason, build, uptime, epc1, phase, last-fail: ~40 bytes) or write the raw record as its own LittleFS file; keep the rings in RTC and publish them once.",
  "potential_consequence": "the black box takes ~1.5 % of an ESP8266's free heap."
 },
 {
  "lens": "adversarial",
  "location": "OBS-6",
  "trigger_condition": "OBS-6's rename strands keys on deployed devices — `last_heap`/`last_minheap` remain in NVS/LittleFS forever; NVS has an entry budget.",
  "guard_snippet": "remove the old keys on the first boot of the new build; say so.",
  "potential_consequence": "clutter that later reads as data."
 },
 {
  "lens": "adversarial",
  "location": "OBS-5",
  "trigger_condition": "OBS-5 ignores the HomeAssistant component — the consumer already exists — `addSensor()` (`HomeAssistant.h:189`) — and a `diagnostic`-category sensor for heap plus a \"last crash\" sensor with attributes gives history graphs and a device page with no Node-RED. The component has no entity-category support yet.",
  "guard_snippet": "route through HA discovery when the component is present, raw topics otherwise; per-entity opt-in as HA users expect.",
  "potential_consequence": "a second ad-hoc topic scheme beside the discovery"
 },
 {
  "lens": "adversarial",
  "location": "OBS-5",
  "trigger_condition": "Telemetry at 60 s can be the observer that kills the patient — a JSON build every minute on an ESP8266 near exhaustion, queued under BUG-29's rate limit, adds allocations exactly when none are left.",
  "guard_snippet": "fixed `char[]` + `snprintf`, no `String`; skip the tick under a heap floor and record that it skipped; \"on by default\" only if this holds.",
  "potential_consequence": "STOR-ESP-1's lesson in reverse."
 },
 {
  "lens": "adversarial",
  "location": "§5",
  "trigger_condition": "\"On the next reboot\" is at least one OTA campaign away — Lot A's promise assumes the new build is on the fleet; devices rebooting every N hours must take an OTA inside the window.",
  "guard_snippet": "say it in §5.",
  "potential_consequence": "expectations."
 },
 {
  "lens": "adversarial",
  "location": "§3 L1, §3 L3",
  "trigger_condition": "Build id ↔ ELF is asserted, not specified — \"hash of the build string\" is not recoverable from an ELF unless the same string is embedded in it and printed at boot.",
  "guard_snippet": "`-DDOMOTICS_BUILD_ID=<git short sha>` via an extra script, stored as a `.rodata` string, printed at boot, its CRC32 in the record.",
  "potential_consequence": "a record nobody can decode."
 },
 {
  "lens": "adversarial",
  "location": "§2 F11",
  "trigger_condition": "Brownout survival is \"to verify\" with no plan, and brownout is a top candidate — `RTC_NOINIT_ATTR` is documented for restart and deep sleep only (`esp_attr.h:99-102`). The ESP32-CAM browns out on FTDI 3.3 V (CLAUDE.md) — a brownout rig for free.",
  "guard_snippet": "measure it there.",
  "potential_consequence": "a blank record for the most likely cause on cheap"
 },
 {
  "lens": "adversarial",
  "location": "§4, F12",
  "trigger_condition": "`crash wdt` is two commands on ESP8266 and an open decision on ESP32 — soft WDT (~3 s) reaches the postmortem and the callback; hardware WDT runs nothing. On ESP32 `enableLoopWDT()` is one line that turns a silent hang into a panic and therefore a core dump — arguably the largest single gain on that platform, and not filed.",
  "guard_snippet": "`crash swdt` / `crash hwdt`; measure F12; file the loop-WDT decision as its own item in Lot A.",
  "potential_consequence": "an unmeasurable definition of done."
 },
 {
  "lens": "adversarial",
  "location": "§5, §7.7, roadmap intro",
  "trigger_condition": "Lot A and the severities — OBS-1's endpoint (chunked partition streaming, auth, erase) is not an afternoon; the boot check is. §7.7 asks the review to argue the severity instead of arguing it. The section intro says \"adversarially reviewed before these entries were finalised\" of text written before the review.",
  "guard_snippet": "Lot A = OBS-2 line + OBS-6 + OBS-1 *check only* + the ESP32 loop-WDT decision; endpoint to Lot D. Severity: OBS-3/OBS-4 HIGH by consequence (the instrument for Constitution XIV against a live production defect) or all MEDIUM with priority carried by lot order — pick one, write why. Date the intro after amendment.",
  "potential_consequence": "the first lot slips, and the roadmap gains another"
 }
]
```
