# OBS Lot C — the moment memory ran out (OBS-4)

Status: **v2, 2026-09-06 — executed. S1 committed (`91baecb`); S2, S3, S4 built and awaiting their commits; S5 run on both boards; the three-layer code review (`review-obs-lot-c-code.md`, 41 raw findings, 24 patched) folded before the PR.** What the boards changed against the plan: `-g` is not byte-identical (−416 B of flash code, ICF); the `_NOEXTRA4K` heap delta is 0, not −4 096; the HWDT greeting prints at the ROM baud rate; the migration check reads the boot sequence, not a death; loop cost on ESP8266 is link-layout-bound (46–88 µs for one source) so no per-loop cost is resolvable; on the WROOM the healthy rate is zero over ten minutes idle, an upload and MQTT, and a squeeze to 12 KB kills the device by task watchdog after 168 survived failures. What the review changed: a survived failure meeting an exception before the latch is routed to the group, not the death; an opt-out no longer warns; `nothrow`/`squeeze` allocate internal heap only; four unobserved mechanisms got their tests.
Derived from `spec-obs-crash-observability.md` v2 §L2, §6 and §8
(residuals 2, 4, 5), the OBS-4 entry and the OBS-3 entry's residuals in
`docs/CODE-ROADMAP.md`, and the code on `main` at `c2e5db1`. Every design
claim is **read** (a line in the installed cores or this repository),
**measured** (the 2026-09-05 probes or Lot B's campaign), or **to measure
in the lot**, and says which.

## What the lot delivers

Today the record says how the heap *trended* before a death and, on ESP8266
only, which allocation *killed* the process. It says nothing about an
allocation that failed and was survived — a `malloc` that returned NULL and
was handled — which is how a WiFi stack or a JSON document degrades for
minutes before something finally dies. After this lot:

- On **ESP32**, the failed-allocation hook records every failure the heap
  reports — size, requested capabilities, allocatable free and largest
  block at that moment, uptime, and a count — straight into RTC, so the
  measured shape (one firing at the terminal failure, a panic microseconds
  later) is on the next boot's `bootdiag`. Today `last fail` is always zero
  there.
- On **ESP8266**, the core's `umm_last_fail_alloc_addr/size` — "since boot"
  today, so a handled failure early in the run is what a later unrelated
  death reports (Lot B's recorded residual) — is latched into the same
  group with the same count and **cleared**, so the crash callback's
  `failCaller/failSize` mean "the allocation that killed us, or nothing".
  **What that latch can see on a stock build is narrower than the
  sentence suggests, and D3 says exactly what** (review 6).
- A **diagnostic build profile** for ESP8266 (`-DDEBUG_ESP_OOM`, `-g`,
  optionally `-DDEBUG_ESP_HWDT`) with its cost measured and written down
  before it is recommended — and it is the profile, not the stock build,
  that makes a `String`, JSON or lwIP failure visible to the latch.
- The **hook's firing rate on a healthy device under WiFi load**, measured
  before one firing is ever read as a death (spec residual 2, review 11).
- Two defects found on the way, fixed here: `System::getBootDiagnostics()`
  grows its cursor past its buffer unclamped, and `Core::begin()`'s text
  buffer is smaller than `format()`'s saturated output (review 14).

Item: **OBS-4**. Also closes spec residuals **2**, **4** and **5**, and the
OBS-3 residual on `umm_last_fail_alloc`. No version bump.

## What the cores do — read on 2026-09-06, checked again by the review

- **ESP32 (arduino-esp32 2.0.17, IDF 4.4.7, precompiled).**
  `esp_alloc_failed_hook_t (size_t size, uint32_t caps, const char*
  function_name)`; `heap_caps_register_failed_alloc_callback()` stores it in
  a **single** `.bss` slot (`nm libheap.a`: `alloc_failed_callback`, 4
  bytes) with no getter — a second registration silently replaces the
  first (review 12). `heap_caps_alloc_failed`, `heap_caps_malloc`,
  `heap_caps_malloc_base` and `heap_caps_malloc_default` sit in **`.iram1`**;
  `heap_caps_get_free_size` and `heap_caps_get_largest_free_block` sit in
  **`.text`** (flash); `esp_timer_get_time` is `.iram1`, `millis()` is not
  (`CONFIG_ARDUINO_ISR_IRAM` unset). No
  `CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS`: the hook runs and the
  allocation returns NULL. The hook runs on the failing task, on either
  core, after the heap's lock has been released (`heap_caps_malloc`
  disassembled: `heap_caps_malloc_base` returns, then the failure path on
  NULL with `size > 0`; residual 5 answered). **One firing per failed
  `malloc` on the bench boards, and the reason is not "no PSRAM support"**:
  the shipped `sdkconfig.h` has `CONFIG_SPIRAM 1`, `CONFIG_SPIRAM_USE_MALLOC
  1`, `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 4096` (lines 239-241);
  `esp_spiram_init()` fails on a board without the chip, `psramInit()`
  returns before `heap_caps_malloc_extmem_enable()`, and
  `malloc_alwaysinternal_limit` stays at "disabled". **On a PSRAM board
  `heap_caps_malloc_default` tries `DEFAULT|SPIRAM` then `DEFAULT` for
  sizes over 4 096: two firings per failure, the first with caps `0x1400`
  (SPIRAM)** (review 16). `operator new` on failure: the hook fires inside
  the `malloc`, then libstdc++ aborts → panic → core dump. The probe
  measured one firing at the terminal failure, 4 096 B, caps
  `INTERNAL|DEFAULT` = `0x1800`, zero firings in 60 s idle without WiFi.
- **ESP8266 (core 3.1.2).** `operator new`/`new[]` and both `nothrow`
  forms set `umm_last_fail_alloc_addr/size` on **every** build
  (`abi.cpp:38-78`). The C paths: `_malloc_r/_realloc_r/_calloc_r`
  (`heap.cpp:137-165`, newlib's reentrant entries — `strdup`, the `printf`
  family) record on every build, with the newlib routine's address as the
  caller (`addr2line` resolves it to a libc function, which says *what
  kind* of allocation failed, not where); the `malloc/calloc/realloc`
  wrappers user code reaches exist only under `DEBUG_ESP_OOM`, poison or
  integrity checks (`heap.cpp:223-281`), and the stock build compiles the
  recording macro to nothing for them and for `heap_pvPort*`
  (`heap.cpp:209-217`, "It cost 64 more bytes of IRAM to turn on").
  **Nothing under `umm_malloc/` touches the globals.** So on a stock
  build: `String` growth (`WString.cpp:246`, `realloc`), ArduinoJson 7
  (`malloc`/`realloc`), lwIP and the SDK (`pvPortMalloc`) record
  **nothing** when they fail; `new`, `nothrow new` and newlib-internal
  failures record. **This repository has zero `nothrow` sites** (grep,
  review 6). Under `DEBUG_ESP_OOM`, `Arduino.h` pulls `heap_api_debug.h`:
  `malloc/calloc/realloc` become macros carrying a per-call-site
  `PROGMEM __FILE__` and `__LINE__` into `heap_pvPortMalloc()`, in every
  translation unit that includes `Arduino.h` with the flag — libraries
  included when it is a `build_flags` entry, not when it is a sketch-level
  `#define`; `umm_last_fail_alloc_file/line` are set; `:oom(size)@file:line`
  prints on each failure **when `system_get_os_print()` is on**
  (`DEBUG_ESP_PORT` or `Serial.setDebugOutput(true)`); `MEMLEAK_DEBUG` is
  defined; `DBGLOG_FUNCTION` becomes `ets_uart_printf` for all of umm's
  debug prints; `STATIC_ALWAYS_INLINE` turns off so `heap_pvPort*` become
  real `IRAM_ATTR` functions — that is where the IRAM goes (review 17).
  The globals are plain, writable, non-static (`heap.cpp:77-78`). The
  postmortem prints `last failed alloc call` (`:258-266`) and `last failed
  alloc caller` (`:270-274`) **before** `custom_crash_callback` (`:276`).
  `-g` adds `.debug_*` sections to the ELF and nothing to `firmware.bin`
  by construction — confirmed by measuring in S4 with a fixed build id,
  because without one `__DATE__ " " __TIME__` is in `.rodata` and two
  builds never compare equal (review 10). `DEBUG_ESP_HWDT` prints a stack
  dump after a hardware watchdog reset **and a greeting at every boot**
  (`DEBUG_ESP_HWDT_PRINT_GREETING (1)`); plain `DEBUG_ESP_HWDT` keeps the
  extra 4 KB of heap, `_NOEXTRA4K` gives it to the `sys` stack for a
  fuller dump (`hwdt_app_entry.cpp:118-135, 183`).
- **Repository.** `flush()` is one `rtcWrite(W_BUILD, …, WORDS - W_BUILD)`
  — a `memcpy` of `w3..w82` on ESP32; `writeAll()` copies the whole
  record (`FlightRecorder.cpp:105-118`). `rtcStoreWord()` on ESP32 is a
  plain store to a non-`volatile` `RTC_NOINIT_ATTR` array in the same
  translation unit (`:25-27`); on ESP8266 it is a store through
  `RTC_USER_MEM`, which is `volatile` (`esp8266_peri.h:176`). Lot B
  budgets one extra largest-block walk per tick interval on a cliff
  (`extraWalkDone_`, `largestAtMin_`, `:211-217`). `Core::begin()` prints
  `format()` from `char text[768]` (`Core.cpp:45`) while `format()` can
  write 837 characters with saturated values; `System::getBootDiagnostics()`
  does `pos += snprintf(buf + pos, sizeof(buf) - pos, …)` **unclamped**
  over `char buf[1024]` (`System.h:603-624`): once `pos` passes 1024 the
  remaining size wraps and the next append writes past the buffer
  (review 14 — pre-existing, Lot B's).

## Design decisions

### D1 — One group, one shape, both platforms: `w56-60`

The spec reserved `w56-60` as `seq, size, free, largest, uptime_ms`. The
lot packs free and largest the way the rings already do — `free16 |
largest16`, 16-byte units — and uses the freed word for the **site**, which
is the datum each platform has and the other lacks:

```
w56  count word: (count << 16) | (~count & 0xFFFF)   self-validating; count saturates at 0xFFFF; written LAST
w57  size in bytes of the last failure
w58  free16 | largest16 at the moment of the failure (allocatable heap, as the rings)
w59  site:  ESP8266 = caller (umm_last_fail_alloc_addr);  ESP32 = the caps word of the last firing
w60  uptime_ms of the last failure
```

Worked example, pinned by a literal in S1 the way the phase marker is:
count 3 → `0x0003FFFC`; count 0xFFFF (saturated) → `0xFFFF0000`; a word
whose halves are not complements → no group. **There is no busy bit and
no seqlock** (review 2, 3, 4): the cross-core problem is solved by a
critical section (D2), and across a reset the count word written last
validates the group — a panic between the field stores and the count
store (five stores, well under a microsecond) leaves the previous count
over new fields, which is recorded as a residual, not hidden. `count` is
the number of hook firings (ESP32: attempts, so a PSRAM board counts two
per request) or latches (ESP8266) since boot.

**The group is outside the CRC, like the phase marker** (Lot B: "the boards
taught that"), **and the tick's flush never writes it** (review 1): the
ESP32 hook stores the group straight to RTC, because the measured shape is
one firing followed by a panic microseconds later and the tick is up to
10 s away — so `flush()` writes two ranges, `w3..w55` and `w61..w82`, then
the CRC word; `writeAll()` (a fresh record, `acknowledge()`, the ESP8266
callback) refreshes the group from the RAM copy the hook keeps in step
with RTC, inside the section, right before its write. (v2 said "re-reads
from RTC" and "the tick's copy"; the code keeps RAM and RTC together under
the section and the tick never copies the group — the sounder shape, said
here so the PR does not repeat v2.) **S1 found the review's proof shape unreachable natively**: the
hook writes the RAM copy too (under the section), so a hook fired at the
entry of the stub's `rtcWrite()` is copied fresh by the very `memcpy` it
was meant to defeat — the real hazard is the `memcpy`'s per-word
read-then-write window on ESP32, which no stub reproduces. What is
provable, and pinned, is the property itself: a group present in RTC and
absent from RAM survives a flush (`test_the_flush_never_writes_the_group`,
red on the single `memcpy`: `Expected 1 Was 0`). The seam was dropped.

**Layout: `LAYOUT` moves 1 → 2 with a one-boot migration** (review 9). A
Lot B record has zeros in `w56-60` and every other word keeps its meaning;
`begin()` verifies a layout-1 record with the layout-1 CRC rule
(`bodyCrcForLayout(1)` covers the five words, `bodyCrcForLayout(2)` skips
them), promotes it by the same three-leg rule, and the fresh record is
written as layout 2. Ten lines, and the first boot after the OTA — the one
the operator is watching — reports the real death instead of "none
recorded". The existing test that writes `(2u << 24)` as "another layout"
moves to `3u`, or it flips from proving rejection to being promoted.
`FlightRecorder.h:18` and the `bodyCrc()` comment say the new coverage.
**Maintainer's decision, see the list at the end.**

### D2 — ESP32: the hook

- HAL seam `bool installFailedAllocHook(FailedAllocHook)` — ESP32:
  `heap_caps_register_failed_alloc_callback()`, return checked; ESP8266:
  returns false (`supportsFailedAllocHook()` false so `Core::begin()` stays
  quiet there); stub: stores the hook and exposes
  `fireFailedAllocForTest(size, caps)`.
- **Registered last in `FlightRecorder::begin()`**, after the final
  `writeAll()` or the hold decision (review 18): a failure on another
  task during `startFresh()`'s `memset` must not write a group the same
  lines then erase. The stub's write log makes the order assertable.
  Under `DOMOTICS_CRASH_HOOKS` (review 12: the slot is single and silently
  replaced; the define is the opt-out, `onFailedAlloc(fn)` chains a user
  hook after ours, and the roadmap entry says both).
  `failedAllocHookInstalled()` reported like the restart hook; on ESP32
  one WARN from `Core::begin()` when it is not.
- **The group's stores run inside a critical section** (review 2, 3):
  HAL seam `failGroupEnter()/failGroupLeave()` — ESP32
  `portENTER_CRITICAL_SAFE` on a `portMUX_TYPE` in `FlightRecorder.cpp`
  (safe from a task on either core and, should IDF ever fire the hook
  from an interrupt, from there too; on the C3 it masks interrupts);
  ESP8266 and stub: nothing, the writer and the readers are the loop
  task. Every reader of the group — the tick's copy into `current_`, the
  `writeAll()` re-read, `format()` reading `previous_` — takes the same
  section on ESP32. Two writers serialise on the spinlock; the second's
  fields win and the count sees both — that is the honest sentence.
- **The hook body**: no log, no allocation, no `String`. Under the
  section: size, caps, uptime (`esp_timer_get_time()` through the HAL —
  not `millis()`), allocatable free (`heap_caps_get_free_size`, O(heaps)),
  largest block **only if no walk has been done in this tick interval**
  (the same gate as Lot B's cliff walk, D3), the five stores with the
  count word last. **No `IRAM_ATTR` claim** (review 5): the trampoline is
  the only platform-specific code and `noteFailedAlloc()`, `rtcStoreWord()`
  and the heap reads all live in flash, so the safety argument is the one
  IDF itself makes for placing `heap_caps_get_free_size` in `.text` — a
  failed allocation never runs with the flash cache disabled. The comment
  above the trampoline says that, not "IRAM-safe". Should S5 ever show a
  cache-disabled panic from the hook, the reads go behind a flag and the
  stores move behind a HAL `DOMOTICS_IRAM_ATTR`.
- **While a promoted record is held in RTC (`rtcHeld()`), RAM only** —
  the held death's own group must not be overwritten; `acknowledge()`
  writes the fresh record whole, group included, from the RAM copy (the
  one place a RAM copy legitimately lands on `w56-60`).
- `noteFailedAlloc(size, siteOrCaps)` is the platform-free entry the
  trampoline calls, so the native suite drives it through the stub.

### D3 — ESP8266: latch and clear, and what it can see

- HAL seam `bool takeLastFailedAlloc(uint32_t& addr, uint32_t& size)` —
  ESP8266: reads the two core globals under `xt_rsil(15)` (a failure in an
  interrupt between the two loads would pair one failure's address with
  another's size — review 7), returns true and **clears them** when the
  address is non-null; stub: scripted; ESP32: false.
- Called from `tick()` at the millisecond-gated sample (two loads per
  millisecond, nothing when null). A latch is a `noteFailedAlloc(size,
  caller)` in loop context: count, size, free, largest (the `umm_info`
  walk, interrupts off), uptime, the five RTC stores at once — a hardware
  watchdog inside the interval, which runs no code, still finds the group.
- **One walk per interval, shared with the cliff** (review 8): a failure
  *is* a drop below the cliff threshold in the same millisecond, so the
  latch reuses `largestAtMin_` when `extraWalkDone_` is set, else walks
  once and sets both. Burst and cliff together: one walk. The test counts
  it.
- **The crash callback keeps reading the globals.** A fatal `operator new`
  sets them and aborts with no sample in between, so `failCaller/failSize`
  in `w65-66` are fresh for the death that matters; a survived failure has
  been latched and cleared within a millisecond, so those two words read
  zero for an unrelated later death — which is the residual closed.
- **Coverage on a stock build, said plainly** (review 6): the group counts
  `nothrow new` — none in this repository today — and newlib-internal
  failures (`strdup`, the `printf` family), with the libc routine as the
  site. `String`, ArduinoJson, lwIP and SDK failures reach it **only
  under `-DDEBUG_ESP_OOM`** — that is the argument for D4, and the entry,
  the reference and the release note say so, or the first `bootdiag` after
  a JSON out-of-memory reads `failed allocs: 0` under a sentence that
  promised otherwise. S5 proves the coverage, not just the mechanism: a
  `String` growth failure reads count 0 on the stock build and count 1
  under the profile.
- **Consequence to say in the release note**: the core's own postmortem
  line `last failed alloc call` now shows only a failure since the last
  sample, in practice the fatal one or nothing; the survived ones are in
  `bootdiag`'s group instead of on the serial console. **Maintainer's
  decision, see the list at the end.**

### D4 — The diagnostic build profile (ESP8266)

Flags, all `build_flags` entries so they reach every library (a sketch
`#define` does not), none in a tracked example `[env]`:

| flag | what it buys | what it changes | cost, measured in S4 |
|---|---|---|---|
| `-g` | line tables: `addr2line -e firmware.elf 0x4020b41c` gives file:line for `epc1`, the fail caller and the stack words | `.debug_*` sections in the ELF | none in `firmware.bin` — proven by `sha256sum` of the stock and `-g` images built with the same fixed `DOMOTICS_BUILD_ID`, both hashes in the table |
| `-DDEBUG_ESP_OOM` | `umm_last_fail_alloc_file/line` for the C paths; `:oom(size)@file:line` on the console when debug output is on; the C-path recording the stock build compiles out — `String`, JSON, lwIP failures become visible to the latch | (a) `MEMLEAK_DEBUG`; (b) umm's `DBGLOG_FUNCTION` → `ets_uart_printf`; (c) `heap_pvPort*` become real `IRAM_ATTR` functions; (d) every `malloc(`/`calloc(`/`realloc(` token in every TU becomes a statement-expression with a `PROGMEM` string | RAM, IRAM, flash from the linker; `LOOPCOST` mean and max |
| `-DDEBUG_ESP_HWDT` | a stack dump on the boot after a hardware watchdog — F7's `epc1` rests on one sample; this is the tool that would give the backtrace | a greeting on **every** boot (the bench readers will see it); keeps the extra 4 KB of heap | heap at boot (expected 0 delta), flash |
| `-DDEBUG_ESP_HWDT_NOEXTRA4K` | the same, with the fuller dump | gives the 4 KB to the `sys` stack | heap at boot (expected −4 096), flash |

Delivered as `[env:nodemcuv2-diag]` in `probes/obs-lotb-probe/platformio.ini`
(the probe is the ESP8266 vehicle, CI-14) with
`-DDOMOTICS_BUILD_ID='"diag"'` fixed for the comparison, a paragraph in the
Core technical reference with the measured table, and the on-device
README's `EXTRA_BUILD_FLAGS` recipe for any example. The record does not
gain a file/line: no word for it, and `-g` on the kept ELF gives the same
answer from the caller. ESP32 needs nothing: its ELF already resolves
file:line (Lot B, measured).

### D5 — What is shown, where, and what is not persisted

- `FlightRecorder::format()` adds one line when the promoted record's
  count is non-zero, **before the ring** so truncation eats samples rather
  than the failure (review 14), in **bytes** like its neighbours, the
  platform byte of `w1` choosing the site word's spelling (review 13):

  ```
    failed allocs: 3 | last 4096 B caps 0x1800 at 401.2 s | free 5312 B, largest 2048 B      (ESP32)
    failed allocs: 1 | last 1048576 B from 0x4020b41c at 12.0 s | free 31232 B, largest 28672 B   (ESP8266)
  ```

  105 characters with every field saturated (computed); the S1 test pins
  the worst case under 128.
- **`bootdiag` gains a "this boot" line** — `this boot: failed allocs N,
  last S B` from `current()` — so a survived failure is readable on the
  console without a reboot (review 12); that is what D6 reads on
  FullStack, where the probe's `LOOPCOST` line does not exist.
- **The 60 s WARN lives in `Core::loop()`**, beside the EventBus drop
  line, reading the count the recorder exposes (review 15):
  `Allocation failures since boot: N, last S B`. `FlightRecorder.cpp`
  stays log-free.
- **Buffers** (review 14): `Core::begin()`'s text buffer grows to 1024
  with a comment naming the saturated figure; `System::getBootDiagnostics()`
  clamps `pos` after every append; one native test formats a saturated
  record through both paths. Filed as the lot's own defect, fixed in S2.
- **The `bootdiag` Storage blob is not extended** (no version 2, no key
  dance on the first boot): the group lives in RTC and is printed from
  the promoted record; a persisted count and a Home Assistant entity for
  it are Lot D's transport. `SystemInfo` is untouched.

### D6 — The rate, measured before the number is believed

Spec residual 2 and review 11. On the WROOM-32D with FullStack (crash
commands through `EXTRA_BUILD_FLAGS`), the count read from `bootdiag`'s
"this boot" line and the WARN: ten minutes idle on the LAN with the WebUI
open; one full 1.35 MB OTA upload (the async server and the WiFi receive
buffers are where a healthy device is most likely to fail an allocation);
a WebUI session with the schema and the SSE stream; MQTT if a broker is
reachable from the bench, said either way. Then a squeeze: `crash squeeze`
holds 4 KB chunks (1 KB on ESP8266) in a `static` table until
`getAllocatableFreeHeap()` is under 12 KB and returns; `crash release`
frees them. The same three loads run squeezed — the shape of a leaking
device an hour before it dies. **What happens when the SDK itself starts
failing is part of the measurement, not a surprise**: lwIP's `pvPortMalloc`
failing means the Telnet console may stop answering; the serial WARN line
and the next `bootdiag` after `release` are the readings then, and a
console that dies is not read as the device dying. Both numbers go into
the OBS-4 entry; the entry says which reading is a death signal and which
is weather, and that on a PSRAM board the count is attempts (two per
request over 4 096 B). The C3 runs the idle and upload cases if plugged.

### D7 — Crash commands: three new kinds

`crash nothrow` — a survived failure: `new (std::nothrow) uint8_t[1 << 20]`
on ESP8266 and `malloc(1 << 20)` on ESP32, **the pointer kept in a `static
volatile` sink and freed if non-null** — Lot B's lesson, twice paid: an
unused allocation is dead code (review 11). The request cannot succeed on
either board. `crash squeeze` and `crash release` — D6's hold, the chunk
table `static`, the stop condition on allocatable heap. All in
`HAL::Platform::crashForTest()`, the stub records them.
`crash_check.sh` learns a **no-drop mode**: for `nothrow` it sends the
command without `--expect-drop`, then greps `bootdiag`'s "this boot" line
for `failed allocs: 1` (review 12). Gate unchanged:
`DOMOTICS_ENABLE_CRASH_COMMANDS`, test and probe environments only, the PR
checklist greps the examples for it.

## Slices — one branch, one PR, each slice green on its own

| # | content | proof — and what would still pass with the mechanism removed |
|---|---|---|
| **S1** | `FlightRecord` layout 2 with the migration: the `w56-60` group (`FailedAllocView`: count, size, free, largest, site, uptime), outside the CRC, `bodyCrcForLayout()`; `flush()`/`writeAll()` never write the group from RAM; `format()` line before the ring; HAL seams on three platforms (`installFailedAllocHook`, `supportsFailedAllocHook`, `takeLastFailedAlloc`, `failGroupEnter/Leave`, `getMillisAnyContext`, stub `fireFailedAllocForTest`); `noteFailedAlloc()` with the shared walk gate, RTC-direct stores count-last, RAM-only while held | `test_flight_recorder`: the count word by literal (`0x0003FFFC`, saturation `0xFFFF0000`, a non-complement reads as no group); **a firing with no tick, then a boot, finds the group in RTC** (the panic-right-after shape); **a group in RTC that RAM does not have survives the flush** (red on today's `memcpy`: `Expected 1 Was 0`); the group survives the tick's flush without tearing the record (mutation: put it back inside the CRC — red); burst of five **and** a cliff in one interval → one walk total; RAM-only while a promoted record is held, present after `acknowledge()`; **a layout-1 record with a Lot B CRC is promoted with its fields and rewritten as layout 2**; a layout-3 record reads as another layout; `format()` saturated stays under 128 per line and the whole text under the two callers' buffers; `resetForTest()` clears the group. Three cross-compiles |
| **S2** | ESP32 wiring: registration **last** in `begin()` under `DOMOTICS_CRASH_HOOKS`, `failedAllocHookInstalled()`, the `Core::begin()` WARN, `onFailedAlloc()` chain, the trampoline with the cache-on comment; `Core::loop()`'s 60 s WARN; `Core::begin()` buffer 1024; `System::getBootDiagnostics()` clamped and the "this boot" line | stub: registration observed **after** the last RTC write of `begin()`; a scripted failure reaches the recorder; the user hook runs after the record; the WARN at most once per 60 s through the logger callback; registration failure reported not claimed; System: the "this boot" line, a saturated record through `getBootDiagnostics()` cut at 1024 with its marker. **S2 found the unclamped cursor unobservable**: a saturated record (942) plus the phase and this-boot lines reaches 1016 of 1023, eight bytes short of the wrap — the clamp stays as a defence, the test pins the cut, and the mutation is recorded as not red. Cross-compile esp32dev and esp32c3 with the three opt-outs (CI's job) **and once with hooks off, tick on** (review 19) |
| **S3** | ESP8266 latch-and-clear from the sample under `xt_rsil`; the callback's fields kept fresh; `crash nothrow`, `squeeze`, `release` on three platforms; `crash_check.sh` no-drop mode | stub: **one failure, then N samples → count 1; two failures with a sample between → count 2** (mutation: leave the clear out — the first case counts N, red — review 7); a death after a latched failure reports `failCaller = 0` in the callback block and the failure in the group (Lot B's residual, pinned); a latch with no tick, then a boot, finds the group (hardware-watchdog shape); the fatal `new` shape — globals set, no sample, callback — reports the caller in `w65` |
| **S4** | The diagnostic profile: `nodemcuv2-diag` env with the fixed build id, docs (Core reference table, on-device README), the cost table | Five builds of the probe on the nodemcuv2 — stock, `-g`, `-DDEBUG_ESP_OOM`, `+HWDT`, `+HWDT_NOEXTRA4K`, all with `DOMOTICS_BUILD_ID="diag"` — RAM/IRAM/Flash from the linker, `sha256sum firmware.bin` (stock = `-g`), heap at boot for the two HWDT rows, `LOOPCOST` mean and max stock vs OOM; under OOM, `crash nothrow` prints `:oom(1048576)@…` with debug output on; `crash hwdt` under HWDT prints the dump at the next boot and the greeting at every boot is noted; `addr2line` on the `-g` ELF resolves the `oom` caller to file:line where the stock ELF gives a function |
| **S5** | The board campaign | **WROOM-32D (FullStack + commands)**: `crash oom` → next boot's `bootdiag` carries the group (count ≥ 1, 4 096 B, caps `0x1800`, free ≪ the ring's plateau, uptime) beside `unexpected reset`; `crash nothrow` → "this boot: failed allocs 1" and the WARN within 60 s, no death; D6's rate table idle / upload / WebUI / squeezed; `crash restart` and the EN reset unchanged. **nodemcuv2 (probe)**: `crash oom` → callback `last failed alloc 1024 B from 0x…`, group count 0; `crash nothrow` then `crash abort` → group count 1 size 1 048 576, callback fail fields **zero**; `crash nothrow` then `crash hwdt` → the group survives with the phase; **coverage**: a `String` growth to failure (a probe command) reads count 0 stock and count 1 under `nodemcuv2-diag`. **Migration**: Lot C's probe flashed over a running Lot B one — the first boot continues Lot B's boot sequence (`boot #N+1`, where a rejected layout restarts at #0) with no torn flag. (S4 corrected the v2 wording "reports the abort": a Lot B build promotes, persists and acknowledges its own death before any reflash, so no death can be in RTC at flash time.) **Removal checks, each once**: `-DDOMOTICS_CRASH_HOOKS=0` on the WROOM → `oom` reads no group; `-DDOMOTICS_FLIGHT_RECORDER_TICK=0` on the nodemcuv2 → `nothrow` then `abort` reports the stale nothrow failure as the death's (the residual reproduced on demand); the latch's clear removed → one `nothrow` reads a count climbing every sample. Loop cost with the latch in the sample vs Lot B's 46 µs; the walk's duration under a squeeze |

Docs in the PR: Core technical reference (layout line, the group, the
seams, the diagnostic profile with its table, `DOMOTICS_CRASH_HOOKS` now
naming the ESP32 slot, the stock-build coverage sentence), on-device README
(`nothrow`, `squeeze`, `release`, the diag env, the no-drop check), the
roadmap (**the OBS-4 entry rewritten, not appended** — its Fix paragraph
still describes Lot B's half and five plain words; the OBS-3 residual
bullet struck; the rate table and the profile's costs; spec residuals 2, 4,
5 answered; counts moved: 42M → 41M, resolved 78 → 79, Observability `3M`
→ `2M` with OBS-4 named among the closed — review 19), `FlightRecorder.h:18`
and `bodyCrc()`'s comment, and the release note owed: **layout 2 with the
one-boot migration**; the ESP32 failed-allocation slot taken unless
`DOMOTICS_CRASH_HOOKS=0`; the ESP8266 postmortem's `last failed alloc call`
line now meaning "since the last sample"; what a stock ESP8266 build can
and cannot count; the new WARN line and the two `bootdiag` lines; the two
buffer fixes. Order that worked in Lot B, kept: plan → adversarial review
of the plan → build with tests → boards → three-layer review of the diff →
fix → PR.

## What this lot does not do

Persist the count or publish it (Lot D: the blob, HA diagnostic entities,
MQTT); give the ESP8266 record a file/line (no word; `-g` on the kept ELF
answers it); hook the ESP8266 C paths on a stock build (the core compiles
that recording out — the diagnostic profile is the answer, at its cost);
capture the *allocation trace* that led to the failure (heap tracing is
off in the shipped sdkconfig and out of scope); survive power loss or an
EN-pin reset on ESP32 (design §7); the WebUI/OTA/MQTT sub-phase markers.

## Residuals this lot expects to record, unfiled unless the boards say otherwise

- A PSRAM board: two firings per request over 4 096 B, the group's caps
  word saying SPIRAM for the first — no such board on the bench; the
  entry says what to expect.
- A panic between the group's field stores and its count store: the
  previous count over the new fields, in a window of five stores.
- Two ESP32 writers serialise on the spinlock; the second's fields win.
- A failure inside an interrupt-context allocation (illegal in IDF): the
  section is `_SAFE`, the stores are fine, the walk gate makes at most one
  walk there.

## Decisions — taken 2026-09-06

**All six as proposed**, on the maintainer's word ("pour chaque, suis tes
recommandations"): clear the globals after latching; layout 2 with the
one-boot migration; the diagnostic profile in the probe's ini and the docs;
the blob untouched; free|largest packed for the site word; the two buffer
defects fixed inside the lot. The list as it was put:

1. **D3 — clear the core's globals after latching.** The consequence: the
   ESP8266 postmortem's own `last failed alloc call` line shows only a
   failure since the last sample; survived failures move to `bootdiag`.
   The alternative — latch without clearing and count only when
   (addr, size) changes — keeps the console line as it is and cannot count
   a repeat of the same failure, which is the common shape of a leak.
2. **D1 — layout 2 with the one-boot migration** (proposed). Two
   alternatives: bump without migration — the first boot after the
   upgrade reports "none recorded" over a real death; or no bump — a
   Lot B record fails the new CRC rule and is reported once, promoted and
   flagged *torn*, with reason, phase and callback fields readable but the
   `torn` bit set in the blob. The migration is ten lines and the
   difference is the one boot the operator is watching.
3. **D4 — the diagnostic profile lives in the probe's `platformio.ini`
   and the docs**, not as a FullStack environment CI compiles. The
   alternative adds one build per CI run and a tracked env whose only
   purpose is the bench.
4. **D5 — the `bootdiag` blob is not extended in this lot**; the persisted
   count waits for Lot D's transport. The alternative bumps the blob to
   version 2 now and pays the first-boot reset of the "last death on
   record" twice.
5. **D1 — free and largest packed to make a word for the site.** The
   alternative keeps the spec's five plain words and has no caller on
   ESP8266, where the caller is the only thing `addr2line` can use.
6. **D5 — the two buffer defects are fixed inside this lot** (they sit on
   the display path the lot extends) rather than filed as a BUG item and
   fixed in their own PR. The alternative keeps the lot single-purpose and
   leaves a reachable overrun in `bootdiag` until that PR.
