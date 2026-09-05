# OBS — post-mortem observability for devices that reboot on their own

Status: **v2 — amended after adversarial review and two board sessions,
2026-09-05.** The review (`review-obs-v1-adversarial.md`, 22 findings) is
folded in; its first finding was confirmed on the nodemcuv2 within the
hour and reshaped the design. Design only, nothing implemented. Roadmap
items OBS-1 to OBS-7, Priority 11.

v1 (same day, before the review) proposed the recorder inside
`SystemInfo`, promotion keyed on the reset reason, and Lot A as "the
afternoon that answers the fleet question". All three were wrong, in ways
the board made plain. What v1 got right — the console is structurally
blind, most of the capture machinery already exists, three layers — is
kept.

## The problem, as the maintainer states it

Devices in production for a long time reboot at a regular interval. The
suspected cause is memory: buffers that accumulate, allocations that are
never given back, until something fails. Nobody has ever seen one of these
deaths. The RemoteConsole was built for exactly this and shows nothing at
the moment that matters: a heap exhaustion is not in the console.

Every claim below carries its source: **measured** (the nodemcuv2 or the
WROOM-32D, 2026-09-05, probes in the session scratchpad), **read** (a line
in the installed cores — arduino-esp32 2.0.17 / ESP-IDF 4.4,
arduino-esp8266 3.1.2 — or in this repository), or **to verify**.

## 1. Why the console is blind, structurally

`RemoteConsoleComponent` is a Telnet stream fed from a `LoggerCallbacks`
callback, with a circular buffer in RAM — 5 entries on ESP8266, 100 on
ESP32.

- **A crash never goes through `DLOG`.** The panic handler runs below the
  Logger; the TCP connection dies with the firmware.
- **The buffer is RAM.** Five lines, on the platform that crashes most.
- **Nobody is connected at 3 a.m.**

None of this is a defect in the console. It is the wrong shape for a black
box.

## 2. What the boards said

### ESP8266 (nodemcuv2, stock build, no debug defines)

Seven deaths in sequence, each boot reading what the previous one left in
RTC user memory from word 32, with a `custom_crash_callback` that copies
`rst_info` and eight stack words:

| death | next boot's SDK reason | our HAL enum | `wasUnexpectedReset()` | crash callback ran | callback received |
|---|---|---|---|---|---|
| `abort()` | 4 Software/System restart | `Software` | **no** | yes | reason 254, stack |
| OOM in `new` (1 KB loop) | 4 Software/System restart | `Software` | **no** | yes | reason 254, **failed-alloc caller `0x40201287`, size 1024** |
| null dereference | 2 Exception | `Panic` | yes | yes | reason 2, exccause 28, epc1 |
| busy loop (soft WDT) | 3 Software Watchdog | `TaskWatchdog` | yes | yes | reason 3, exccause 4, **epc1 = the loop** |
| `wdtDisable()` + busy loop (HW WDT) | 1 Hardware Watchdog | `Watchdog` | yes | **no** — no code runs | SDK `rst_info` still carries epc1 = the loop |
| `ESP.restart()` | 4 Software/System restart | `Software` | no | no | — |
| RTS/EN reset | 6 External | `External` | no | no | — |

**RTC user memory survived all seven**, hardware watchdog and external
reset included. `ESP.getResetInfo()` formatted the exception and both
watchdogs correctly, and said only "Software/System restart" for the two
that matter most.

The first two rows are the finding. **An out-of-memory `new` and an
`abort()` are indistinguishable from `ESP.restart()` by reset reason.**
The only thing that separates them is that the crash callback ran — and
it ran with the failed allocation's caller and size already filled in,
on a stock build, because `operator new` records them before aborting
(`abi.cpp:38-46`). v1 said that needed `DEBUG_ESP_OOM`; it does not. The
`malloc`/`realloc` paths do (`heap.cpp:104-127`).

### ESP32 (WROOM-32D, `esp32dev`, stock `default.csv`)

| event | `esp_reset_reason()` | `RTC_NOINIT_ATTR` record | core dump afterwards |
|---|---|---|---|
| EN-pin reset (esptool, `readserial.py`) | 1 POWERON | **lost** | — |
| null dereference | 4 PANIC | kept | 8 964 B, `image_check` OK |
| `malloc` until NULL, then dereference | 4 PANIC | kept | 8 996 B |
| `abort()` | 4 PANIC | kept | 9 188 B |
| busy loop in `loop()` | **no reset in > 40 s** (TWDT is 5 s) | — | — |

The coredump partition was present at `0x3F0000`, `esp_core_dump_image_get`
returned each dump, `esp_core_dump_image_erase` cleared it. The
failed-allocation hook fired **once**, at the terminal failure — 4 096 B
requested, caps `INTERNAL|DEFAULT` — and **reported 91 264 B "free"
internal heap at that moment**: the free figure counts 32-bit-only IRAM
that `malloc` cannot use. It fired zero times in 60 s idle, without WiFi.

### ESP32-C3 (`esp32-c3-devkitm-1`, native USB-Serial-JTAG, stock `default.csv`)

Run the same afternoon, the first code ever executed on the CI's third
target. Same probes as the WROOM-32D:

| event | `esp_reset_reason()` | `RTC_NOINIT_ATTR` record | core dump afterwards |
|---|---|---|---|
| USB-Serial-JTAG reset pulse (DTR low, RTS pulse) | **0 UNKNOWN** | **kept** | — |
| null dereference | 4 PANIC | kept | 2 916 B (probe) / 7 044 B (System) |
| `malloc` until NULL, then dereference | 4 PANIC | kept | 2 916 B |
| `abort()` | 4 PANIC | kept | 3 044 B |
| busy loop in `loop()` | **no reset in > 55 s** | — | — |
| busy loop, Lot A watchdog at 5 s | 6 TASK_WDT in ~5 s | kept | 9 476 B |

The hang is silent for a different reason than on the ESP32: the C3's
precompiled sdkconfig has no `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` at
all, so with one core nothing is on the task watchdog. Lot A's
`enableLoopWatchdog()` works unchanged. The failed-alloc hook fired once at
the terminal failure (73 × 4 KB, then NULL, 10 220 B "free"), zero times
idle. The port re-enumerates on every reset, so a reader must reopen.

`addr2line` on the ESP32 ELF gave `file:line` for every address. On the
ESP8266 it gave `setup at ??:?` — the default PlatformIO build carries
symbols but no line tables. Two operational facts fall out: keep the ELF
of every deployed build, and build the ESP8266 with `-g` if you want
lines.

## 3. What already exists, corrected

| # | Claim | Status |
|---|---|---|
| F1 | ESP32 panics write an ELF core dump to the `coredump` partition; `esp_core_dump_image_check/get/erase` are available | **measured** |
| F2 | 23 of the 24 stock partition tables carry the partition (`bare_minimum_2MB.csv` does not); PlatformIO's default for `esp32dev` is `default.csv`. CI-9 switched the examples to `min_spiffs.csv` and listed "64 KB coredump" as a reason; nothing followed up | read |
| F3 | **A project loses the partition only by choosing a custom CSV** — as FullStack did before CI-9. The maintainer's fleet runs his own project; unless it set a custom table, the dumps have been accumulating. The OBS-1 boot check turns that inference into a log line per device | read (v1 had this backwards) |
| F4 | Nothing in the repository reads the dump | read |
| F5 | ESP8266 `rst_info` keeps exccause/epc1-3/excvaddr/depc; `ESP.getResetInfo()` formats them for reasons 1–3; our HAL keeps only the enum. **Reasons 253/254 (abort, OOM-`new`, assert, panic, `bad_alloc`) never reach `rst_info` — the next boot sees reason 4** | **measured** |
| F6 | `custom_crash_callback` (weak) runs for exceptions, soft WDT and the user-exception class, with the stack, before the restart | **measured** |
| F7 | A hardware WDT runs no code; RTC survives it; the SDK's `rst_info` still carries a usable epc1 | **measured** (epc1 part is one sample — treat as likely, not proven) |
| F8 | `heap_caps_register_failed_alloc_callback()` fires with size and caps; once at the terminal failure in the probe | **measured** (rate under WiFi load: to verify) |
| F9 | `operator new` records the failed caller/size on every build; C allocations only under `DEBUG_ESP_OOM` | **measured** / read |
| F10 | ESP8266 RTC user memory: 512 B, word-addressable, words 0–31 are `eboot_command`, nothing else in the core touches it; survives every reset but power loss | read + **measured** |
| F11 | `RTC_NOINIT_ATTR` survives panics and software resets, **not an EN-pin reset**, which reports POWERON. Brownout: to verify (the ESP32-CAM on FTDI 3.3 V is the rig) | **measured** / to verify |
| F12 | **A stuck `loop()` on ESP32 does not reboot**: `loopTask` is not on the task WDT (`main.cpp:34,69`), the TWDT watches `IDLE0` only, the loop runs on core 1 | **measured** |
| F13 | `last_heap`/`last_minheap` describe the new boot; on ESP8266 the two are equal by definition | read |
| F14 | No heap figure leaves the device; the HomeAssistant component has `addSensor()` and no entity-category support | read |
| F15 | The EventBus drops silently on overflow (LO-5) | read |

## 4. Design v2 — three layers, each useful without the next

### L1 — Flight recorder in RTC memory (OBS-3)

**Ownership: Core, not SystemInfo.** The recorder must run first and
always; `SystemInfo` is optional and last (review 5, 6). RTC primitives go
in `Platform_HAL.h` (the stub keeps a static array so the native suites
drive the whole logic); sampler, crash hook and promotion in Core;
`SystemInfo`, the console and the WebUI only display.

**Promotion is the first act of `System::begin()`**, before any component:
copy RTC to a RAM staging struct, decide, freeze sampling until done.
Storage is written when Storage is up; MQTT publishes when MQTT connects.
A device that dies again during WiFi bring-up still gets its record out on
the boot that finally connects.

**The discriminator is the record, not the SDK.** A record is promoted
when its "crash callback ran" flag is set (any reason, 253/254 included),
or when the reset reason is unexpected and the flag is not (hardware WDT,
brownout). `wasUnexpectedReset()` alone would have skipped the two rows
that matter (review 1, measured).

**Layout: whole 32-bit words, ≤ 384 B, from ESP8266 word 32** (review 7).
The ESP32 uses the same struct in `RTC_NOINIT_ATTR`.

```
w0     magic                w1  layout version | platform | flags (crash-callback-ran, torn)
w2     crc32 (over w3..end of rings)           w3  build id (CRC32 of the build string, see below)
w4     boot sequence (RTC-local)               w5  phase marker: (v << 16) | (~v & 0xFFFF), one store
w6-7   last write: uptime_ms, min-free-since-last-tick
w8-39  fast ring 16 × {uptime_s, free16 | largest16}     one tick / 10 s   → last 160 s
w40-55 slow ring  8 × {uptime_s, free16 | largest16}     one tick / 10 min → last 80 min
w56-60 last failed alloc: seq, size, free, largest, uptime_ms   (seqlock: seq odd while writing)
w61-66 exception: cb reason, exccause, epc1, excvaddr, fail caller, fail size
w67-82 16 stack words from the crash callback
```

Heap fields are 16-bit in **16-byte units** (1 MB range) and measure
**allocatable** heap — `MALLOC_CAP_8BIT|INTERNAL` on ESP32, never the
raw internal free that reported 91 KB while a 4 KB `malloc` failed
(review 10, measured). Largest block sits beside free in every sample,
because fragmentation is the other way a device dies with heap "free".

**Sampling with a running minimum** (review 9): every `Core::loop()` reads
free and largest into RAM minima; each 10 s tick writes `{current,
min-since-last}` so a one-second cliff between ticks is not smoothed into
a plateau. Cost per loop: two heap reads. Cost per tick: a dozen stores
and a CRC over ~250 bytes.

**Phase marker: one store** (review 8). Set in `ComponentRegistry::loop()`
before each `component->loop()` and in the WebUI/OTA/MQTT handlers that
do real work; the value is component index in the low byte, sub-phase in
the next. The `last write` uptime is a measurement in itself: the gap to
the death says when the loop stopped.

**Build id** (review 19): `-DDOMOTICS_BUILD_ID="<git short sha or user
string>"` embedded as a `.rodata` string (so `strings firmware.elf` finds
it), printed at boot, its CRC32 in `w3`. Without it a record cannot be
matched to an ELF; with it the operator can.

**Storage: a compact summary, not the record** (review 14). The ESP8266
backend hex-encodes blobs into a resident JSON document, so 330 bytes of
record would cost ~660 bytes of heap for the life of the process. Persist
~40 bytes — reason, build id, uptime, phase, epc1, fail size, count of
identical crashes — once per boot, deduplicated on (build, reason, epc1).
The rings live in RTC and are published once (L3), not stored.

**Hooks are opt-in or chainable** (review 12). `custom_crash_callback` is
a strong symbol; defining it unconditionally breaks every EspSaveCrash
user at link time. `heap_caps_register_failed_alloc_callback` is one slot.
Both are enabled by a define that `System` sets and a bare-`Core` user
can leave off, and both forward to a user hook if one is registered.

### L2 — The moment memory ran out (OBS-4)

- **ESP8266**: the crash callback already receives the failed `new`'s
  caller and size on every build (measured). Copy them. The diagnostic
  build profile (`-DDEBUG_ESP_OOM`, optionally `-DDEBUG_ESP_HWDT`, and
  `-g`) adds the C-allocation sites and line tables; its cost is measured
  and written into the entry before it is recommended.
- **ESP32**: register the hook; it writes `w56-60` under the seqlock with
  plain stores, no log, no allocation, from whichever task failed. A
  counter, not a boolean, because the hook fires per failed attempt
  (review 11); how often it fires on a healthy device under WiFi load is
  measured in the lot before one call is read as death.
- **ESP32, the silent hang (OBS-7)**: a busy `loop()` never reboots
  (measured). `enableLoopWDT()` is one line that turns it into a panic and
  therefore a core dump with the backtrace of the hang. Whether the
  library turns it on is a behaviour decision — it changes what happens to
  every user's blocking `delay()`-free loop — so it is filed as its own
  item, default proposed **on**, with the timeout configurable.

### L3 — Getting it off the device (OBS-1, OBS-2, OBS-5)

- **Boot log and `bootdiag` (OBS-2)**: on ESP8266, `ESP.getResetInfo()`
  for the line and the six words for the record. **It cannot see the
  abort/OOM class** — that is L1's callback — and the entry says so.
- **The ESP32 dump (OBS-1)**: at boot, `esp_core_dump_image_check()` and a
  WARN with the size — that half is Lot A. The transport —
  `GET /api/system/coredump` streaming the partition, `POST …/erase`
  under SEC-10's token, both behind `enableAuth` — is Lot D (review 22).
  Decoded on the host with `esp-coredump info_corefile` and the build's
  ELF.
- **Home Assistant first, raw MQTT second (OBS-5)** (review 16). When the
  HA component is present, the record and the telemetry are
  `diagnostic`-category entities: free heap, largest block, uptime, boot
  sequence, RSSI, and a "last crash" sensor whose attributes carry the
  decoded record — history graphs and a device page with no Node-RED.
  Without HA, `{clientId}/crash` retained QoS 1 and `{clientId}/telemetry`
  QoS 0. **The publisher is allocation-free**: fixed `char[]`, `snprintf`,
  and it skips the tick under a heap floor and says so (review 17). Only
  under that rule is "on by default at 60 s" defensible.

**Keep the ELF.** The user documentation says, in those words: keep
`.pio/build/<env>/firmware.elf` for every build you flash on a device you
cannot walk to, build the ESP8266 with `-g`, and set a build id. The
on-device harness keeps the ELF beside each log it captures.

## 5. Verification — what would still pass with the mechanism removed

**Forced-crash commands, compile-gated** (review 13): `crash abort`,
`crash oom`, `crash null`, `crash swdt`, `crash hwdt` (ESP8266),
`crash hang` (ESP32). Behind `DOMOTICS_ENABLE_CRASH_COMMANDS`, on in the
test environments and the diagnostic profile, off in every shipped
default — SEC-4 is open and these are a remote reboot.

Definition of done, per platform, per command: the next boot's record
carries the reason the callback received, the phase marker names the
console, `last fail` is non-zero for `oom`, the fast ring shows the
descent, and the decode locates the site from the ELF. The probe results
in §2 are the expected values.

Removal checks, each run once and recorded: no crash callback → empty
exception block after `crash abort`; no sampler → empty rings; no hook →
zero `last fail` after `crash oom` on ESP32.

Native: the stub's RTC array drives promotion-first ordering, the
callback-flag discriminator, dedup, the seqlock torn-read rejection and
the JSON, with the heap floor and the running minimum scripted through
`Platform_Stub.h`'s heap seam (TEST-4's).

## 6. Lots

| Lot | Items | Content |
|---|---|---|
| **A — read what is already there** | OBS-2, OBS-6, OBS-1 (check), OBS-7 | `ESP.getResetInfo()` in the boot log and `bootdiag`; the persisted fields renamed and the old keys removed; the ESP32 boot check that says whether a dump is waiting and whether the partition exists; the loop-WDT decision. No recorder yet |
| **B — the recorder** | OBS-3 | Core-owned RTC record, promotion-first, callback discriminator, crash commands and the three removal checks |
| **C — the OOM moment** | OBS-4 | Both hooks, the hook-rate measurement under WiFi, the diagnostic profile with its cost |
| **D — off the device** | OBS-5, OBS-1 (transport) | HA diagnostic entities, raw topics, allocation-free publisher, the coredump endpoint |

**Recommended first: Lot A, with its promise stated honestly.** On the
next boot of a re-flashed ESP8266 it will say whether the death was an
exception or a watchdog, and where — and it will say "Software/System
restart" for an OOM in `new`, which is itself information once the entry
has taught the reader what that reason hides. On an ESP32 it will say
whether a dump is waiting. And it is at least one OTA campaign away:
nothing here observes a device until the build that contains it is on
that device (review 18).

## 7. What this design does not do

- Prevent the reboot. It makes it explainable.
- Survive power loss, or an EN-pin reset on ESP32.
- Capture a hardware WDT on ESP8266 beyond the phase marker, the last
  tick, and the SDK's own epc1.
- Retrofit a core dump onto an ESP32 flashed with a custom table that has
  no `coredump` partition.
- Decode anything without the ELF of the deployed build.

## 8. Residuals — to verify in the lots

Lot A shipped on 2026-09-05 with two reviews behind it: the maintainer's
(one Constitution IX `#if` in a component, removed) and an adversarial one
run after the PR opened (`review-obs-lot-a-adversarial.md`, 13 findings —
the watchdog armed `loopTask` and fed the calling task, fixed and
re-measured on both ESP32s). Its open items are folded below as 6–9.


1. `RTC_NOINIT_ATTR` across a brownout reset. **Attempted 2026-09-05 on the
   ESP32-CAM from FTDI 3.3 V**: AP+STA at 19.5 dBm with back-to-back scans ran
   112 s, then the FTDI adapter dropped off the USB bus and did not return —
   the brownout takes the observer down with it, as CLAUDE.md records. A
   replug is a power cycle, which loses RTC by definition. **This rig cannot
   measure it**; it needs the CAM on its own 5 V with the adapter on TX/RX/GND
   only, and a sag provoked on that supply. Until then the design treats a
   brownout as reason-only, no record — the worst case — so nothing blocks.
2. The failed-alloc hook's rate on a healthy ESP32 under WiFi + MQTT.
3. F7's epc1 on hardware WDT — one sample.
4. The ESP8266 `-g` cost in flash, and `DEBUG_ESP_OOM`'s in speed.
5. Whether `heap_caps_get_largest_free_block` inside the failed-alloc hook
   is safe from every calling context (it takes the heap lock).
6. ~~The loop watchdog's 30 s default against the longest `System::loop()`
   iteration~~ — answered on the WROOM-32D: 0.37 s idle, 0.80 s during an
   upload, scan 8.1 s; the commit runs on the async task. C3 unmeasured
   (unplugged). The upload's own failure is BUG-37.
7. `enableLoopWatchdog()` on IDF 5 / Arduino 3.x (`esp_task_wdt_reconfigure`);
   today it compiles with a `#warning` and does not arm.
8. The ESP8266 `cont` stack high-water mark during `bootdiag` (`buf[640]`).
9. `persistBootDiagnostics` costs three LittleFS file rewrites per boot on
   ESP8266 (four before Lot A); OBS-3's once-per-boot rule and dedup are
   where that number should fall.
