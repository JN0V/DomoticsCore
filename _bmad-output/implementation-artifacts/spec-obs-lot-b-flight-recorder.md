# OBS Lot B — the flight recorder (OBS-3, BUG-36, ARCH-1's residue)

Status: **v2, 2026-09-06 — plan, nothing implemented.** v1 was reviewed
adversarially the same day (22 findings, folded below; the four claims the
review said were wrong were checked against the sources and were wrong).
Derived from `spec-obs-crash-observability.md` v2 §L1, §5, §6, §8. Every
design claim is **read** (a line in the installed cores or this
repository), **measured** (2026-09-05 probes), or **to measure in the
lot**, and says which.

## What the lot delivers

On the boot after a death, the device knows *which* death it was and what
the heap looked like in the last 160 s before it — on an ESP8266, where the
reset reason alone says "Software/System restart" for an `abort()`, an
`assert` and an out-of-memory `new` alike (measured). The record lives in
RTC memory, is promoted first thing at boot, **stays in RTC until it has
been persisted**, and a compact summary is stored once per distinct death.
Nothing leaves the device yet (Lot D). Nothing captures the OOM moment on
ESP32 yet (Lot C).

Items: **OBS-3**, **BUG-36** (whose fix adds the drop counter the record
wants), **ARCH-1's residue** (the boot diagnostics leave `System.h`), and
the spec's residual **9** — restated honestly in D4, not closed.

## Design decisions

### D1 — Where the code lives, and the instance model

- `DomoticsCore-Core/include/DomoticsCore/FlightRecorder.h`: layout, CRC,
  rings, promotion, sampling, staging, formatting. Platform-free.
- `DomoticsCore-Core/src/FlightRecorder.cpp`: the one definition of the
  RTC storage (ESP32 `RTC_NOINIT_ATTR` array), of the ESP8266
  `custom_crash_callback`, and of the ESP32 shutdown handler. **A `.cpp`,
  not a header**: each needs exactly one definition, and the core compiles
  these headers as gnu++11/14 on ESP32 (Lot A), so C++17 inline variables
  are out. **The callback and the storage stay in the same translation
  unit as `FlightRecorder::begin()`**, which `Core::begin()` always
  references — under PlatformIO's default `lib_archive = true` (the
  registry consumer's case; every example here sets `false`) a strong
  symbol in an archive member only overrides the core's weak one if that
  member is linked, and this is what guarantees it. A comment says so.
- **One instance, process-wide**: `FlightRecorder::instance()` returns a
  function-local static (the gnu++11/14 constraint again). The callback,
  the handler, `ComponentRegistry::loopAll()`, `Core` and `System` all go
  through it. `resetForTest()` clears staging, minima and the `begun`
  state, and `clearRtcForTest()` the stub's RTC array; **every suite's
  `setUp()` calls both**, or a promotion decided in one test leaks into
  the next.
- Platform primitives in `Platform_HAL.h`, three, not two:
  `rtcRead(wordOffset, dst, words)`, `rtcWrite(...)` (bulk, SDK call on
  ESP8266: `system_rtc_mem_write`, read), and **`rtcStoreWord(wordOffset,
  value)`** — a volatile 32-bit store to `RTC_USER_MEM + 32 + n`
  (`0x60001200`, the address `eboot_command.h` uses, read) on ESP8266, a
  plain store on ESP32, an array store on the stub. The phase marker uses
  the third; an SDK call per component per loop iteration was the v1
  shape and is not acceptable at several kHz.
- Two heap seams: `getAllocatableFreeHeap()` and `getLargestFreeBlock()`.
  ESP32: `heap_caps_get_free_size` / `heap_caps_get_largest_free_block`
  with `MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL` — never `ESP.getFreeHeap()`,
  which said 91 KB "free" while a 4 KB `malloc` failed (measured).
  ESP8266: `ESP.getFreeHeap()` and `ESP.getMaxFreeBlockSize()`. Stub:
  `setLargestFreeBlockForTest()` beside `setFreeHeapForTest()`.
- **CRC32: the bitwise loop in the header**, identical on the three
  targets and testable natively; ~10 k iterations over 330 bytes in the
  crash callback, a few hundred µs, stated here. Not `coredecls.h`'s nor
  `esp_rom_crc32_le`: three variants that differ by reflection would be
  discovered by a record one build wrote and another read.

### D2 — The record

The spec's layout, 83 words = 332 bytes, from ESP8266 word 32 (96 words
available, read; the Lot A probe left its step in user word 40 on the
bench boards — inside the record, discarded by the magic check). Whole
words; CRC32 over `w3..end` **written last**; phase marker one store of
`(v << 16) | (~v & 0xFFFF)`. `w1` flags: `crash-callback-ran`,
`ours` (D5), `promoted-unpersisted` (D4), `torn` — **set by promotion**
when magic and layout match but the CRC does not, in which case `w0/w1/w5`
(magic, flags, phase) are still read and reported as "torn record: phase
N" rather than discarded. BUG-36's drop counter is the high half of `w4`
(boot sequence keeps the low half; both saturate). Thirteen spare words
(115–127) stay unused.

### D3 — Sampling cost, a stated deviation from the spec

The spec reads free and largest every `Core::loop()`. `getMaxFreeBlockSize()`
is `umm_max_block_size()` → `umm_info()`, which **walks the whole heap with
interrupts off** and whose own comment says it "can negatively impact WiFi
operations" (read, `umm_info.c:181`). So: **free every loop** (a maintained
counter, running minimum kept in RAM), **largest at tick time only** (every
10 s) plus **at most one extra walk per tick interval**, taken when the
running minimum has dropped more than a threshold below the last tick's
free (4 KB on ESP8266, 16 KB on ESP32 — to adjust on measurement). S5
measures the loop-iteration cost with and without the sampler on both
boards, the marker included, and the walk's duration at the ESP8266's
typical fragmentation; the numbers go into the roadmap entry before the
default is defended.

### D4 — Promotion first; the RTC record stays until persisted

`FlightRecorder::begin()` is the first statement of `System::begin()` and,
idempotently, of `Core::begin()`. It copies RTC into the staging struct,
validates magic/layout/CRC (torn handling per D2), and decides:

> promoted when the crash-callback flag is set (any reason, 253/254
> included); **or** the reset reason is unexpected and the flag is not
> (hardware WDT, brownout); **or the reason is `Software` and `ours` is
> not set** (ESP32's leg: a software reset the firmware did not ask for —
> on ESP8266 the callback flag already separates `abort`/OOM from
> `ESP.restart()`, and `ours` is set there too for symmetry).

**It does not start a fresh RTC record.** It sets `promoted-unpersisted`
in `w1`, samples into RAM only, and writes the fresh record to RTC on the
first successful persistence. A device that dies again before Storage is
up keeps the first death's record — the spec's promise, which v1 broke.
Clean boots (nothing promoted) start the fresh record at once.

The decision is silent; **the one INFO line ("promoted: <reason>, phase N,
uptime U") is emitted from `Core::begin()` right after
`initializeLogging()`**, reading the staged decision — before that point
there is no logger.

Persistence is Storage's, and Core cannot depend on Storage, so `System`
keeps one thin step where `initBootDiagnosticsPersistence()` was:
`SystemHelpers::persistCrashSummary(storage, recorder)`. **Write count,
honestly**: on ESP8266 every changed `putInt`/`putBytes` rewrites the whole
JSON file (`Storage_ESP8266.h:89-93,141-152`, read). Today a boot costs
`boot_count` + `boot_heap` (+ `last_reset` when it changes) = 2–3 rewrites.
The lot **folds `last_reset`, `boot_heap`, `boot_minheap` and the crash
summary into one blob `bootdiag`**, so a boot costs `boot_count` + blob =
**2, always**, and a crash boot the same 2. Residual 9 is restated to that
figure, measured natively by counting `save()` calls on the stub backend.
Dedup key `(build id, reason, epc1 ? epc1 : failCaller)`: an identical
death increments the count in the blob (still one rewrite — dedup keeps
history, it does not save a write, and the plan says so). The fold is the
third rename of these keys in three releases (OBS-6 was the second); the
release note carries it.

### D5 — The discriminator per platform

- **ESP8266**: `custom_crash_callback` stores the reason the callback
  received, exccause/epc1/excvaddr, the failed-allocation caller and size
  `operator new` recorded (measured, stock build — **documented as "last
  failed allocation since boot"**, since `nothrow` failures set the same
  globals and nothing clears them, read `abi.cpp`), sixteen stack words,
  **the RAM running minima and the current free/largest into `w6-7`** (so
  a burst OOM inside one tick is on the record — without this the per-loop
  read of D3 changes no reported number), the flag; recomputes the CRC;
  one `rtcUserMemoryWrite`. No allocation, no log.
- **ESP32**: no panic hook in IDF 4.4 (read), none needed — `PANIC`,
  `TASK_WDT`, `INT_WDT` are unambiguous and the core dump is the record.
  `esp_register_shutdown_handler` sets `ours` on every `esp_restart()` the
  firmware calls. **The handler writes the single `w1` word and nothing
  else**: it runs on whichever task called `esp_restart()` (the async
  server task on OTA), and a full flush would race the tick's `memcpy`
  from `loopTask` — the CRC-last rule plus torn handling in D2 covers a
  panic landing mid-tick. Its return value is checked (IDF has a fixed
  number of handler slots): on failure, one WARN once logging is up and
  the `ours` leg reports "handler not registered" rather than
  "unexplained".
- **Hooks on by default, opt-out `-DDOMOTICS_CRASH_HOOKS=0`.** A header
  cannot set a define for a `.cpp` in another library, so opt-in would be
  a build flag every user — the fleet firmware first — has to know about.
  Opt-out is for EspSaveCrash users **and for any sketch that already
  defines `custom_crash_callback`: with the default on, that sketch fails
  at link on upgrade, a duplicate strong symbol**. Release note, at the
  top. `FlightRecorder::onCrash(fn)` chains a user hook either way.
  **Maintainer's decision, see the list at the end.**

### D6 — Phase marker

`ComponentRegistry::loopAll()` does `rtcStoreWord(5, marker(index))`
before each `component->loop()` and `marker(0xFF)` before `eventBus.poll()`
— straight to RTC, so a hardware WDT (no code runs) still leaves it. The
WebUI/OTA/MQTT sub-phases belong to the lots that touch those handlers.

### D7 — BUG-36

`EventBus::enqueue()` decrements the dropped event's topic before popping,
erases the entry at zero (and `poll()` does too), and increments a
**per-bus** `droppedEvents` counter — the roadmap entry says per bus *and*
per topic; the per-topic map is declined for a diagnostic on the ESP8266
and **the entry is amended to say so in S2**. `reset()` clears the
counter (its contract: every new member is listed there; a test asserts
it). `EventBus.h` stays logger-free: LO-5's line is emitted by the
recorder's tick from `getDroppedCount()` deltas, at most once per 60 s.
Native test as the entry prescribes: fill past the cap on topic A with a
B event oldest, drain, subscribe to B with `replayLast` and require the
replay — **red against the unfixed code first**.

### D8 — ARCH-1's residue, and where `bootdiag` is formatted

The record's part of `bootdiag` is formatted **in Core**,
`FlightRecorder::format(char*, size_t)`, allocation-free — a bare-`Core`
build has no other way to show what it recorded. `SystemInfo` formats only
its own `BootDiagnostics` struct (the 50 lines leave `System.h`). The
"Persisted Data" block that read Storage from `System.h` reads the new
blob through the same thin `SystemHelpers` step, so it stays where
Storage is reachable. `boot_heap` (raw free at boot) and the record's
allocatable figures appear side by side, each labelled.

**ARCH-1 will restate, not close**: `begin()` is ~70 lines and this lot
adds one and removes none; the XIII indicator is on `begin()`. Said now
rather than at closure. Shortening `begin()` (the six heap-guarded steps
into one helper) is not this lot's — it is the coordination question with
`esp32-ethernet`, whose System.h diff against today's `main` is
+47/−104 over ten hunks, three of them in the register-method region.
**Before S4, the four `System.h` hunks this lot touches — top of `begin()`,
step 6, `registerConsoleComponent()`, the two removed methods — are
checked against his by line range and the result recorded in the PR.**

### D9 — Crash commands

`crash abort | oom | null | swdt | hwdt | hang`, registered under
`DOMOTICS_ENABLE_CRASH_COMMANDS`. **Gate location**: the `#if` sits in
`System.h`'s `registerConsoleComponent()` and is registered in that
file's Constitution IX comment beside `__has_include`; the flag goes in
`test_build_flags` of the System on-device environments and in the harness
probe envs — **never in an example's `[env]`**, and the PR checklist greps
for it. Bodies: `HAL::Platform::crashForTest(kind)`. `oom` is a `new` loop
on ESP8266 (aborts, reason 254) and `malloc`-until-NULL-then-dereference
on ESP32 (the probes' shapes). The stub records the call.

### D10 — Build id

`-DDOMOTICS_BUILD_ID=\"<string>\"` (escaped quotes in `build_flags`,
documented), a `.rodata` string, printed at boot, CRC32 in `w3`. The
harness sets the git short SHA through an `extra_scripts` file — an ini
has no shell — listed in S3's deliverables.

## Slices — one branch, one PR, each slice green on its own

| # | content | proof — and what would still pass with the mechanism removed |
|---|---|---|
| **S1** | HAL primitives (RTC ×3, allocatable heap, largest block) on three platforms; `FlightRecorder.h/.cpp`: layout, CRC, rings, running minimum, tick, promotion with torn handling and the three-leg rule, staging, keep-until-persisted, `format()`, singleton + `resetForTest()` | `test_flight_recorder` (native): the seven ESP8266 rows and the four ESP32 rows of the spec's tables scripted through the stub RTC array (**rule tests**, labelled so; the callback itself is S3's); SW-without-`ours` promoted, SW-with-`ours` not; torn record → phase still reported; dedup key with `epc1 = 0`; ring wrap; running minimum through the heap seam; tick timing through the millis seam; **die-before-persist**: crash → boot without persistence → crash → boot with persistence → the *first* death is what is persisted; three cross-compiles |
| **S2** | `Core::begin()` promotion + the INFO line; `Core::loop()` tick; registry marker via `rtcStoreWord`; BUG-36 + counter + LO-5 line in the tick; entry amended | `test_eventbus`: BUG-36 replay test **red on unfixed code first**, counter, `reset()` clears it; ordering test: a component whose `begin()` asserts from inside that promotion already happened — **run once with the call moved after `core.begin()`, red recorded** |
| **S3** | `custom_crash_callback` (with the minima fold), ESP32 handler (`w1` only, return checked), `onCrash()`, `DOMOTICS_CRASH_HOOKS`, build id + the harness `extra_scripts` | the callback's body exercised natively through a stub entry point fed a scripted `rst_info`; three cross-compiles, **plus one `lib_archive = true` build** (the registry consumer's shape) whose map file shows the strong symbol linked |
| **S4** | `System::begin()` first act; `persistCrashSummary` (one blob, dedup, boot count); `bootdiag` reassembled (Core part + SystemInfo part + blob); crash commands gated; `initBootDiagnosticsPersistence` shrinks; fork hunk check recorded | System suites: promotion before any component `begin()` (S2's shape), **`save()` count per boot = 2 on the stub backend**, dedup count, `bootdiag` text; `System.h` and `begin()` measured |
| **S5** | The board campaign | FullStack with commands on, both boards (+ C3 if plugged): each command, then the next boot's `bootdiag` carries the reason the callback received, the phase names the console, the fast ring shows the descent; **ESP8266 only**: `last fail` non-zero after `oom`, and `min free since last tick` well below the last ring sample; **ESP32**: PANIC + core dump, fail fields zero until Lot C, `ESP.restart()` reads `ours`. **Removal checks, each run once and recorded**: no callback → empty exception block after `crash abort`; no sampler → empty rings; per-loop sampling compiled out → the minimum stays at the plateau after `oom`; no `ours` → `ESP.restart()` reads unexplained. Loop cost with/without sampler and marker; walk duration; writes per boot; RTC survival per death class with the real record |

Docs in the PR: Core's technical reference (record, seams, defines, the
strong-symbol note), SystemInfo's (`bootdiag` fields), the roadmap (OBS-3
DONE, BUG-36 DONE with the per-bus amendment, ARCH-1 restated on the
number, residual 9 restated, counts moved), and the release note owed:
`DOMOTICS_CRASH_HOOKS` and the strong `custom_crash_callback`,
`DOMOTICS_ENABLE_CRASH_COMMANDS`, the `bootdiag` Storage blob replacing
three keys, the `bootdiag` text change. No version bump in the lot.

## What this lot does not do

Read the ESP32 core dump or publish anything (Lot D); the ESP32
failed-allocation hook and the diagnostic build profile (Lot C); survive
power loss or an EN-pin reset on ESP32 (design §7); the WebUI/OTA/MQTT
sub-phase markers (their lots); shorten `begin()` (ARCH-1's next
restatement).

## Decisions — taken by the maintainer on 2026-09-06

**1 — on by default with opt-out. 2 — one blob. 3 — thresholds as proposed,
measured in S5. 4 — a clean boot writes the blob.** The list as it was put:

1. **D5 — hooks on by default with an opt-out define**, accepting that a
   sketch which already defines `custom_crash_callback` fails at link on
   upgrade (release note at the top). The alternative is opt-in, which
   leaves the fleet firmware without the discriminator unless its build
   flags change.
2. **D4 — the Storage fold**: `last_reset`/`boot_heap`/`boot_minheap`
   become fields of one `bootdiag` blob (two writes per boot, always; the
   third rename of these keys). The alternative keeps the keys and adds
   the blob: three to four rewrites per boot and residual 9 gets worse.
3. **D3 — the thresholds** for the extra largest-block walk (4 KB / 16 KB
   proposed; measured and adjusted in S5).
4. **D4 — a clean boot's blob**: proposed *written* (reason, heap at boot,
   build id) so `bootdiag` after a clean boot is not "no record"; the
   alternative writes only on promotion.

Settled by the review rather than left open: the record stays in RTC
until persisted; SW-without-`ours` is promoted; the marker is a direct
RTC store; `torn` is set by promotion; the callback folds the minima; the
dedup key falls back to the fail caller; `bootdiag`'s record part is
formatted in Core; ARCH-1 restates.
