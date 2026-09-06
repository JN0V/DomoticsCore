# Adversarial review — OBS Lot C plan (`spec-obs-lot-c-oom-moment.md` v1)

**Reviewed**: `_bmad-output/implementation-artifacts/spec-obs-lot-c-oom-moment.md`
v1, 2026-09-06, **before any slice is built** — the order Lot B kept. Every
"read" claim was checked against the installed cores
(`framework-arduinoespressif8266` 3.1.2, `framework-arduinoespressif32`
2.0.17 with `libheap.a` disassembled) and against `main` at `c2e5db1`.
**Lens**: adversarial, alone. 19 findings; three of them mean the ESP32 half
of the lot, as written, can lose exactly the failure it exists to keep.

**Verdict: the platform reading is largely right — the call order, the
sections, the ESP8266 recording paths, the postmortem order all check out —
but the ESP32 write protocol is not sound.** The seqlock is described as
five plain stores, and `flush()` still `memcpy`s `w3..w82` over the group,
so a hook write that lands during the tick's flush is overwritten by the
tick's stale copy; excluding `w56-60` from the CRC stops the *record*
tearing, not the *group* being clobbered. The plain stores themselves are
subject to dead-store elimination of the "seq odd" write (same TU, inlined),
and "count is exact" under two writers is asserted, not achieved. On the
ESP8266 side the headline over-promises: the components contain **zero**
`nothrow` sites, and every `String`, ArduinoJson and lwIP failure goes
through umm's own `malloc`/`realloc`, which records nothing on a stock
build — so the latch will mostly see newlib-internal callers. Two of the
lot's proofs would fail for reasons unrelated to what they test: the
"`-g` is byte-identical" comparison, because the build id embeds
`__DATE__ " " __TIME__`, and the latch-clear mutation, whose predicted
outcome ("counts 1") is the opposite of what the mechanism does.

---

## Findings

### 1. `flush()` and `writeAll()` still `memcpy` `w56-60` from RAM — the CRC exclusion does not stop the clobber — D1, `FlightRecorder.cpp:105-118`
- **Trigger**: `flush()` is `rtcWrite(W_BUILD, &current_.w[W_BUILD], WORDS - W_BUILD)` — a `memcpy` of `w3..w82` on ESP32 (`FlightRecorder.cpp:20-24`). The hook on the other core stores the group straight to RTC; if that lands after the tick's `memcpy` has read `current_.w[56..60]` and before (or while) it writes them, RTC ends up with the *previous* group. `writeAll()` (`acknowledge()`, `recordCrash()`, `begin()`) copies the whole record (`FlightRecord copy = r`) and has the same window. The plan says the hook "must not race the tick's flush" and never says how; D1 only moves the group outside the CRC, which prevents "torn record", not "lost failure". The measured shape — one firing, panic microseconds later — is precisely a hook write with no later tick to repair it.
- **Guard**: `flush()` writes two ranges, `w3..w55` and `w61..w82`, and never touches `w56-60` (the phase already has this treatment); `writeAll()` likewise, or it re-reads the group under the protocol after the copy. Pin it natively with a stub seam that fires `noteFailedAlloc()` from inside `rtcWrite()` (a `rtcWriteHookForTest`) and asserts the group in RTC afterwards is the fired one, not the copied one; this test goes red on today's `memcpy`.
- **Consequence**: the terminal failure the lot exists to record is silently replaced by the stale group whenever the panic follows a hook firing inside a flush — the case the design chose RTC-direct stores for.

### 2. The seqlock is five plain stores in one translation unit — the compiler may reorder or delete the "seq odd" store — D1/D2, `FlightRecorder.cpp:25-27`
- **Trigger**: `rtcStoreWord()` is a plain store to a non-`volatile` array defined in the same file as `noteFailedAlloc()` will be; after inlining, the sequence "store seq odd; store four fields; store seq even" has two stores to `w56` with nothing observable between them, so `-Os` may elide the first (dead store) or reorder the fields around it. Nothing in the plan names a barrier, `volatile`, or an atomic. The reader on the other core (or, on the C3, a preempting task) then sees a consistent-looking seq over half-written fields.
- **Guard**: either a `portMUX_TYPE` critical section around the hook's stores and around every reader of the group (IRAM-safe, works on both cores and on the C3, replaces the seqlock entirely — this is the simpler design), or `__atomic_store_n(..., __ATOMIC_RELEASE)` / `__atomic_load_n(..., __ATOMIC_ACQUIRE)` on the seq word with the array declared `volatile`, plus a compiler barrier between the seq stores and the field stores. Say which in D1 and pin the encoding (finding 4).
- **Consequence**: a protocol that looks correct in the plan and in a single-threaded native test, and does not exist in the binary.

### 3. "Count is exact" under two concurrent writers is asserted, not achieved — Residuals, third bullet
- **Trigger**: the plan says two ESP32 tasks failing in the same microsecond "can interleave their stores. Count is exact, the last fields may mix." With a plain load-increment-store on the count, a concurrent second writer loses an increment; nothing makes it exact. On the C3 (`CONFIG_FREERTOS_UNICORE 1`, RISC-V without the A extension) `__atomic_fetch_add` goes through IDF's interrupt-masking shim rather than an instruction.
- **Guard**: the critical section of finding 2 makes both claims true at once; if the seqlock is kept, increment a RAM counter with `__atomic_fetch_add` and derive the seq from it. State the residual as "two writers serialise on the spinlock; the second's fields win", which is the honest sentence.
- **Consequence**: the count — the datum D6 uses to tell weather from a death signal — under-reports exactly in the burst case it is meant to measure.

### 4. The `w56` encoding contradicts itself — D1 layout block
- **Trigger**: the layout says `w56 = (count << 16) | (~count & 0xFFFF)`, "odd while a write is in progress", and "count = seq >> 1". With the complement in the low half, the low bit of the word is the low bit of `~count` — odd whenever the count is even — so "odd while writing" and "count = seq >> 1" describe a different word (`v = count << 1 | busy`, `w56 = v << 16 | (~v & 0xFFFF)`, saturating `v >> 1` at `0x7FFF`).
- **Guard**: write the encoding once, as a formula and a worked example, and pin it in S1 with a literal the way `test_phase_marker_is_one_store_with_its_complement` pins `0x1234EDCB` (`test_flight_recorder.cpp:370-373`): count 3 idle → one hex value, count 3 busy → another, and the decoder's `torn` bit from the second.
- **Consequence**: two implementers (or the same one on two days) produce two incompatible decoders for the same word, and a torn flag that fires on every even count.

### 5. `IRAM_ATTR` on the trampoline is decoration: `noteFailedAlloc()`, `rtcStoreWord()` and `packHeap()` are in flash — D2, third bullet
- **Trigger**: only the trampoline can carry `IRAM_ATTR` in platform-specific code; the entry it calls is platform-free (`FlightRecorder.cpp`, `.text`), as are `rtcStoreWord()` and the inline helpers. The plan's own second bullet then argues the two heap reads (`.text`, confirmed by `objdump -t`: `.text.heap_caps_get_free_size`, `.text.heap_caps_get_largest_free_block`, `.text.heap_caps_get_info`, `multi_heap_get_info` and `multi_heap_free_size` likewise) are fine because a failed allocation never runs cache-off. If that argument holds, `IRAM_ATTR` on the trampoline buys nothing; if it does not, the stores are not protected either. Verified on the way: `millis()` is **not** IRAM on this core (`ARDUINO_ISR_ATTR` is empty because `CONFIG_ARDUINO_ISR_IRAM` is not set in `sdkconfig.h`), and `esp_timer_get_time` is `.iram1` in `libesp_timer.a` — the plan's choice there is right.
- **Guard**: pick one: (a) drop `IRAM_ATTR` and keep the cache-on argument written next to the hook, or (b) give the HAL a `DOMOTICS_IRAM_ATTR` macro (Constitution IX allows the HAL to define attributes, `Platform_HAL.h` already fills in `PROGMEM`/`PSTR`) and put `noteFailedAlloc()`'s store path behind it with the reads behind the flag the plan already foresees. Either way, the words "hook body: `IRAM_ATTR`" should not describe a flash function.
- **Consequence**: a false sense of safety in the comment above the one function that runs in the worst context the firmware has.

### 6. On a stock ESP8266 build the latch sees almost nothing from this framework — "What the cores do", D3, release note
- **Trigger**: the plan is right that the C wrappers exist only under `DEBUG_ESP_OOM`/poison/integrity (`heap.cpp:223`) and that the recording macro is emptied for `heap_pvPort*` on a stock build (`heap.cpp:209-217`). What it does not say: the components contain **zero** `nothrow` sites (`grep -rn nothrow DomoticsCore-*/include DomoticsCore-*/src` → 0), `String` grows through `realloc` (`WString.cpp:246`), ArduinoJson 7 allocates through `malloc`/`realloc`, and lwIP/SDK go through `pvPortMalloc` (`heap.cpp:355`). None of those set `umm_last_fail_alloc_addr` on a stock build (no `umm_last_fail` anywhere under `umm_malloc/`). What *does* record is `_malloc_r/_realloc_r/_calloc_r` (`heap.cpp:137-165`) — newlib's callers (`strdup`, the `printf` family) — and there the site is the address of the newlib routine that called `_malloc_r`, which `addr2line` resolves to a libc function; the plan calls it "a constant, useless caller", which is neither.
- **Guard**: say it in D3 and in the release note: on a stock ESP8266 build the group counts `nothrow new` (none in the framework today) and newlib-internal failures; `String`, JSON and network failures reach it only under `-DDEBUG_ESP_OOM` — which is the argument for D4, and should be stated as such. S3's board row (`crash nothrow` → count 1) proves the mechanism, not the coverage; add one board check that a `String` growth failure on a stock build reads count 0 and under the diag profile reads count 1 — that is the removal check for the profile's value.
- **Consequence**: the entry announces "which allocation was survived" on ESP8266 and the first user to read a `bootdiag` after a JSON OOM sees `failed allocs: 0`.

### 7. The latch-clear mutation is predicted backwards — S3, S5 "Removal checks"
- **Trigger**: S3 says "leave the clear out — the second identical failure no longer counts 2, red"; S5 says "the latch's clear removed → `nothrow` twice counts 1". With the clear removed, `takeLastFailedAlloc()` returns the same non-null pair on **every** millisecond sample, so one failure counts once per millisecond for the rest of the run — the count explodes, it does not stick at 1. "Counts once per distinct pair" is the *alternative* design (Decision 1), not this mechanism's failure mode.
- **Guard**: the discriminating test is "one failure, then N samples → count 1; two failures with a sample between → count 2"; the mutation without the clear turns the first into count N. Write the predicted outcome that way in both rows. While there: read `addr` and `size` under `xt_rsil(15)` in the seam — a failure in an ISR between the two loads pairs one failure's address with another's size (small, but the seam is two lines either way).
- **Consequence**: a mutation check whose expected result is wrong is a check nobody can run honestly: whichever number appears will be explained.

### 8. Two extra `umm_info` walks per interval, and they coincide — D3 versus Lot B's D3 (`FlightRecorder.cpp:211-217`)
- **Trigger**: Lot B budgets **one** extra largest-block walk per tick interval, on a cliff (`extraWalkDone_`). The plan adds a second gate for the latch — "one per tick interval, the same budget as D3 of Lot B" — which is a second budget of the same size, not the same budget. And the two fire together: an allocation failure *is* a drop below the cliff threshold in the same millisecond, so the sample that latches also walks for the cliff. `umm_info` walks the whole heap with interrupts off (`umm_info.c:180-183`, "can negatively impact WiFi operations").
- **Guard**: share the gate — if `extraWalkDone_`, the latch reuses `largestAtMin_`; else it walks once and sets both; the ESP32 hook's RAM flag can be the same flag (the tick clears it; the cross-core clear is a benign race worth one line). The S1 test "one largest walk per interval under a burst of five" should count the cliff walk too: burst *and* cliff, one walk total.
- **Consequence**: the moment the heap runs out on an ESP8266 gets two interrupts-off walks back to back — the worst time for the WiFi stack to lose its interrupt.

### 9. `LAYOUT` 1 → 2 discards a death that a one-branch migration would keep — D1, Decision 2, `FlightRecorder.cpp:131-137`, `test_flight_recorder.cpp:236-243`
- **Trigger**: every word other than `w56-60` keeps its meaning, and a Lot B record has zeros there. Without a bump, a Lot C reader verifying a Lot B record with the new `bodyCrc()` (which skips `w56-60`) gets a mismatch → **torn**, and torn records are still promoted with reason, phase and callback fields readable (`begin()` line 139; `test_torn_record_still_reports_its_phase`). So the alternative to "lose the previous firmware's death on the first boot" is "report it once, flagged torn" — or, for ten lines, accept `layout() == 1` with the old CRC rule for that one boot. The plan lists neither under Decision 2. Also: `test_a_record_with_another_layout_reads_as_power_on` writes `(2u << 24)` as "another layout"; with `LAYOUT = 2` that write becomes the *current* layout with flags zeroed, and the test flips from proving rejection to being promoted (reason Panic → `UnexpectedReset`). It must move to `1u` or `3u`, and the plan's "the existing native test pins that shape" should say so.
- **Guard**: offer the migration branch as Decision 2's third option — `bodyCrcForLayout(previous_.layout())` — and keep `LAYOUT = 2` for the *next* incompatible change; or keep the bump and say plainly that the torn-but-promoted path was available. Update the header comment (`FlightRecorder.h:18`) and the `bodyCrc()` doc (`:102-107`) whichever way.
- **Consequence**: the one boot a user is most likely to be watching — the first after an OTA to the new release — is the one that reports "none recorded" over a real death.

### 10. The "`-g` is byte-identical" proof fails on its own build id — D4 table, S4, `FlightRecorder.cpp:63-67`
- **Trigger**: without `DOMOTICS_BUILD_ID`, `buildIdCrc()` embeds `__DATE__ " " __TIME__` in `.rodata`; two builds a minute apart differ in those bytes, so the stock and `-g` `firmware.bin` will **not** compare equal and the table's cell is unprovable as written. The wrong conclusion ("`-g` changes the image") is the likely one, or the difference gets explained away.
- **Guard**: pass `-DDOMOTICS_BUILD_ID=\"diag\"` to all four S4 builds (the probe env), or compare `firmware.bin` with the id's bytes masked; write the flag into the `nodemcuv2-diag` env so the comparison is repeatable. Check `sha256sum` on both, and record the two hashes in the table.
- **Consequence**: a measured claim in a published table that no one can reproduce, on the row whose whole point is "proven by comparing".

### 11. `crash nothrow` and `crash squeeze` will be elided or unreachable unless the plan says how they are kept alive — D7
- **Trigger**: `malloc(1 << 20)` with an unused result is dead code to GCC (`BUILT_IN_MALLOC` in DCE); a `new (std::nothrow)` whose result is unused is elidable under the C++14 allocation-elision rule the ESP8266's GCC 10 implements. Lot B hit exactly this twice on `oom` and fixed it with a `volatile` sink (`Platform_ESP8266.h:362-366`); D7 does not carry the lesson over. `squeeze` "holds allocations until ~12 KB remain and returns" — held where? A local array is gone at return; a chain through the blocks themselves needs a reachable head; and nothing releases them. On the ESP8266, 12 KB allocatable leaves lwIP's `pvPortMalloc` failing and the Telnet console possibly unable to answer the next command — which may be the measurement, but then the console's own death must not be read as the device's.
- **Guard**: both commands keep their pointer in a `static volatile` sink (`crash nothrow` frees it if non-null — the 1 MiB request cannot succeed on either board: `umm_blocks()` saturates at `INT16_MAX` blocks, `umm_malloc.cpp:360`); `squeeze` keeps a `static` array of chunk pointers with a `crash release`, stops on `getAllocatableFreeHeap()` rather than a raw figure, and D6 states the chunk size (fragmentation shape) and what happens to the loads when the SDK itself starts failing.
- **Consequence**: the two commands that drive D6 and S5 either do nothing (elided, as Lot B's did) or leave a board that cannot be talked to.

### 12. `crash_check.sh nothrow` cannot pass by its own design — D7, `tools/on-device/crash_check.sh:38-58`
- **Trigger**: the script sends the command with `--expect-drop`, waits for the console to *return*, then greps `bootdiag`. `nothrow` drops nothing, and `bootdiag` prints the **promoted** record (`format()` at `System.h:606`), which carries the previous death's group, not this run's live count. The only live signal the plan gives is the 60 s WARN on serial, and the script greps the console output.
- **Guard**: a no-drop mode that greps the serial log (`$LOG`) for the WARN line within 60 s, or a console line that prints the current group (`bootdiag` gaining a "this boot: failed allocs N" line — cheap, and it is what D6 reads on FullStack, where the probe's `LOOPCOST` line does not exist).
- **Consequence**: the S5 row "`crash nothrow` → the WARN line within 60 s" has no automated check, and the D6 table has no source on FullStack.

### 13. The format line loses the number it is for, and its example does not match the measured caps — D5
- **Trigger**: `w58` packs free and largest in 16-byte units (1 MiB range), and the line prints them in KB — a failure at 1 488 B free (Lot B's measured `oom` floor) prints "free 1 KB"; the rest of `format()` prints bytes. The example `site 0x00000806` decodes as `INTERNAL|8BIT|32BIT`; the measured caps are `INTERNAL|DEFAULT` = `0x1800` (`esp_heap_caps.h:33-34`). "site" invites `addr2line` on a caps word. Line length is fine: 105 characters with every field saturated (computed), 85 for the example.
- **Guard**: bytes, like the neighbouring lines; the platform byte (`w1 >> 16`) chooses the word — `from 0x4020b41c` on ESP8266, `caps 0x1800` on ESP32; the example uses the measured value. Pin the worst-case length in the S1 test with saturated values.
- **Consequence**: the operator reads "free 1 KB" for a heap that had 1 488 B and "site" for a bitmask.

### 14. The new line pushes two existing buffers past their edge, and one of them wraps — D5, `Core.cpp:45`, `System.h:604-616`
- **Trigger**: `format()` today is 669 characters with typical values and 837 with saturated ones (computed from the format strings); `Core::begin()` prints it from `char text[768]` ("~600 characters at most", `Core.cpp:45`) and the new line adds up to 105. `System::getBootDiagnostics()` has `char buf[1024]` and grows `pos += snprintf(buf + pos, sizeof(buf) - pos, …)` **unclamped**: `snprintf` returns the would-be length, so once `pos` passes 1024 the next `sizeof(buf) - pos` wraps to a huge `size_t` and `SystemInfo::formatBootDiagnostics()` writes past the buffer. `format()` itself clamps; its callers do not.
- **Guard**: clamp `pos` after every append in `getBootDiagnostics()` (a `min`), print the failed-alloc line **before** the ring so truncation eats samples rather than the failure, and add the saturated-values test for the total length against both buffers.
- **Consequence**: a stack overflow in the command meant to explain crashes, reachable the first time a long record meets a long SystemInfo block.

### 15. The 60 s WARN belongs in `Core::loop()`, not in the recorder — D5, `Core.cpp:104-111`
- **Trigger**: "the tick logs one WARN" puts a `DLOG_W` inside `FlightRecorder::tick()`; `FlightRecorder.cpp` is deliberately log-free and allocation-free because it is linked into the crash path and the hook. The EventBus drop line the plan cites as the model lives in `Core::loop()`, reading a counter.
- **Guard**: `Core::loop()` reads `recorder.current()`'s count beside the drop counter and logs with the same once-per-60-s pattern; the recorder exposes the count, nothing else.
- **Consequence**: a logger dependency in the one file that must stay callable from a dying context.

### 16. The ESP32 "one firing per failed `malloc`" holds for a reason the plan does not give, and the PSRAM residual has the wrong shape — "What the cores do", Residuals
- **Trigger**: the shipped `sdkconfig.h` has `CONFIG_SPIRAM 1`, `CONFIG_SPIRAM_USE_MALLOC 1`, `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 4096` (lines 239-241). One firing holds because `esp_spiram_init()` fails on a board without the chip and `psramInit()` returns before `heap_caps_malloc_extmem_enable()` (`esp32-hal-psram.c:69-93`), leaving `.data.malloc_alwaysinternal_limit` at `0xffffffff` — confirmed in the object. On a PSRAM board `heap_caps_malloc_default` (disassembled: `bnei a3, -1` → the two-attempt path) tries `DEFAULT|SPIRAM` first for sizes over 4 096, then `DEFAULT`: two firings, and the group's caps word records `0x1400` (SPIRAM) for the first — so the "site" on ESP32 changes meaning on the board that is not on the bench. `heap_caps_malloc_prefer` (BT's `osi_malloc`) fires once per candidate too.
- **Guard**: write the mechanism (init failed, not "disabled") into the entry so the next reader with a PSRAM board knows what to expect: count doubles, caps of the *last* firing is what the group keeps, and the WARN's "N" is attempts, not requests.
- **Consequence**: the first PSRAM user reads twice the failures with a caps word that says SPIRAM, and files a bug against the hook.

### 17. The diagnostic profile's costs are wider than the table's three cells — D4, `heap_api_debug.h`, `umm_malloc_cfg.h:663`, `heap.cpp:20-40`
- **Trigger**: `-DDEBUG_ESP_OOM` also (a) defines `MEMLEAK_DEBUG` (`heap_api_debug.h:23`, used by `LwipDhcpServer.cpp:151`); (b) switches `DBGLOG_FUNCTION` to `ets_uart_printf` for **all** of umm's debug prints (`umm_malloc_cfg.h:663-667`), which do not consult `system_get_os_print()`; (c) turns `STATIC_ALWAYS_INLINE` off (`heap.cpp:25-31`) so `heap_pvPort*` become real `IRAM_ATTR` functions — that is where the IRAM goes; (d) rewrites every `malloc(`/`calloc(`/`realloc(` token in every TU that includes `Arduino.h` into a statement-expression with a `PROGMEM` string — ArduinoJson's allocator included. `DEBUG_ESP_HWDT` prints a greeting at **every** boot by default (`DEBUG_ESP_HWDT_PRINT_GREETING (1)`, `hwdt_app_entry.cpp:183`), which the on-device readers will see. And the two HWDT variants cost differently: plain `DEBUG_ESP_HWDT` keeps the extra 4 KB of heap, `_NOEXTRA4K` gives it back to the `sys` stack for a fuller dump (`hwdt_app_entry.cpp:118-135`) — the table has one row for both.
- **Guard**: two HWDT rows with the expected heap delta (0 and −4 096) and the greeting noted; the OOM row lists (a)–(d) as what the flag changes so the measured IRAM/flash numbers have an explanation; the recipe says `-DDEBUG_ESP_OOM` must reach every library (PlatformIO `build_flags` do; a sketch-only `#define` does not).
- **Consequence**: a cost table that is right in its numbers and wrong about where they come from, and a bench reader that trips on a greeting.

### 18. Hook registration order inside `begin()` is unstated — D2, `FlightRecorder.cpp:120-167`
- **Trigger**: `begin()` `memset`s `current_` in `startFresh()` and then `writeAll()`s it. If the ESP32 hook is registered before that (next to `installRestartHook()` at line 125, the natural place), a failure on another task during those lines — `esp_timer`, `ipc`, or a WiFi task a sketch started before `System::begin()` — writes a group that the `memset`/`writeAll` then erase, and the hook may run against a recorder whose `begun_` is true but whose RAM group is mid-clear.
- **Guard**: register last, after the final `writeAll()`/hold decision, and say so in D2; the stub test "registration observed" should assert it happens after RTC is written (order visible through the stub's write log).
- **Consequence**: a failure during the first milliseconds of bring-up — the boot loop this recorder exists for — is the one that is lost.

### 19. The docs the plan lists leave two roadmap sentences wrong and one contract stale — "Docs in the PR", `CODE-ROADMAP.md:3350, 3362-3381`, `FlightRecorder.h:18`
- **Trigger**: the OBS-4 entry's "Fix" paragraph still says the ESP8266 half is "the crash callback copies the two words" (done in Lot B) and describes the ESP32 group as five plain words; OBS-3's residual bullet says "Lot C latches and clears it"; `FlightRecorder.h:18` documents `w56-60` as "seq, size, free, largest, uptime ms". The counts the plan moves (42M → 41M, 78 → 79, Observability `3M` → `2M`) are consistent with the summary at `:3536-3537`; the Observability row's parenthetical still has to name OBS-4 among the closed. "CI's job" for the opt-out cross-compile is real — `ci.yml:154-158` builds FullStack with `-DDOMOTICS_CRASH_HOOKS=0 -DDOMOTICS_FLIGHT_RECORDER_TICK=0 -DDOMOTICS_FLIGHT_RECORDER_PHASE=0` on all three targets — but only with all three off together, so an `#if` that breaks with hooks off and the tick on is not covered.
- **Guard**: rewrite the OBS-4 entry rather than append DONE; strike the OBS-3 bullet; update the header layout line and `bodyCrc()`'s doc; add the hooks-off/tick-on combination to S2's cross-compiles once (not to CI).
- **Consequence**: the roadmap's sweep rule ("read the sections against the code") finds a DONE entry that describes a different fix.

---

## Verified, no finding

`heap_caps_alloc_failed` in `.iram1.27` calling the `.bss` slot with no getter; `heap_caps_malloc` (`.iram1.29`) calls `heap_caps_malloc_base` then the failure path only on NULL with `size > 0` — the lock is released by then; no `CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS` in `sdkconfig.h`; `esp_timer_get_time` in `.iram1`; `operator new`/`new[]`/both `nothrow` forms set the globals on every build (`abi.cpp:38-78`); the globals plain and non-static (`heap.cpp:77-78`); the postmortem prints `last failed alloc call` at `:258-266` and `last failed alloc caller` at `:270-274` **before** `custom_crash_callback` at `:276`; `print_loc` gated on `system_get_os_print()` (`heap.cpp:178`); the `malloc(s)` macro carries `PROGMEM __FILE__` and `__LINE__` (`heap_api_debug.h:36-38`) via `Arduino.h:314-318`; `_NOEXTRA4K` semantics as read; RTC budget unchanged at 83 of 96 words (ESP8266 words 32–114 of 128, `Esp.cpp:175-190` bounds hold); `rtcUserMemoryWrite` at SDK block `64 + offset` matches `RTC_USER_MEM` (0x60001200) as Lot B measured; the C3 is `CONFIG_FREERTOS_UNICORE` and keeps `RTC_NOINIT_ATTR` across the JTAG reset, so the group is readable there without a callback; the `EXPECT` for `oom` on the nodemcuv2 stays `last failed alloc 1024 B` because the fatal `new` sets the globals with no sample in between.

---

## Findings as JSON (canonical shape)

```json
[
 {
  "lens": "adversarial",
  "location": "D1 — `FlightRecorder.cpp:105-118` `flush()` / `writeAll()`",
  "trigger_condition": "`flush()` still `memcpy`s `w3..w82` (including `w56-60`) from `current_` to RTC and `writeAll()` copies the whole record; a hook store to RTC that lands during that copy is overwritten by the tick's stale group. Excluding the group from the CRC prevents a torn record, not a clobbered group.",
  "guard_snippet": "`flush()` writes `w3..w55` and `w61..w82` and never touches `w56-60`; `writeAll()` likewise. Pin with a stub seam that fires `noteFailedAlloc()` from inside `rtcWrite()` and asserts the group in RTC is the fired one.",
  "potential_consequence": "the terminal failure the lot exists for is replaced by the stale group whenever the panic follows a hook firing inside a flush."
 },
 {
  "lens": "adversarial",
  "location": "D1/D2 — `FlightRecorder.cpp:25-27` `rtcStoreWord`, the seqlock",
  "trigger_condition": "Five plain stores to a non-volatile array in the same TU as the hook: the 'seq odd' store is a dead store before 'seq even' and may be elided or reordered around the field stores; no barrier, volatile or atomic is named.",
  "guard_snippet": "a `portMUX_TYPE` critical section around the hook's stores and every reader (replaces the seqlock, IRAM-safe, works on the C3), or `__atomic_store_n(RELEASE)`/`__atomic_load_n(ACQUIRE)` on the seq word with the array `volatile` and a compiler barrier between seq and field stores.",
  "potential_consequence": "a protocol that is correct in the plan and in the single-threaded native test, and absent from the binary."
 },
 {
  "lens": "adversarial",
  "location": "Residuals — third bullet, 'Count is exact'",
  "trigger_condition": "Two tasks failing concurrently do load-increment-store on the count; an increment is lost. Nothing in the plan makes the count exact, and the C3 has no atomic instruction for `__atomic_fetch_add`.",
  "guard_snippet": "the critical section of finding 2, or `__atomic_fetch_add` on a RAM counter the seq derives from; restate the residual as 'two writers serialise, the second's fields win'.",
  "potential_consequence": "the count D6 uses to tell weather from death under-reports exactly in the burst case it measures."
 },
 {
  "lens": "adversarial",
  "location": "D1 — the `w56` layout block",
  "trigger_condition": "`(count << 16) | (~count & 0xFFFF)`, 'odd while a write is in progress' and 'count = seq >> 1' describe two different encodings: with the complement in the low half the word is odd whenever the count is even.",
  "guard_snippet": "`v = count << 1 | busy`, `w56 = v << 16 | (~v & 0xFFFF)`, count `= v >> 1` saturating at `0x7FFF`; pin idle and busy values for count 3 as hex literals in S1, the way the phase marker test pins `0x1234EDCB`.",
  "potential_consequence": "two incompatible decoders for one word, and a torn flag that fires on every even count."
 },
 {
  "lens": "adversarial",
  "location": "D2 — 'The hook body: `IRAM_ATTR`'",
  "trigger_condition": "Only the trampoline can be IRAM; `noteFailedAlloc()`, `rtcStoreWord()` and the inline helpers are flash, as are both heap reads (`objdump -t`: `.text.heap_caps_get_free_size`, `.text.heap_caps_get_largest_free_block`). The plan's own cache-on argument makes the attribute decoration; if the argument fails, the stores are unprotected too. (`millis()` is indeed not IRAM here: `CONFIG_ARDUINO_ISR_IRAM` unset; `esp_timer_get_time` is `.iram1`.)",
  "guard_snippet": "either drop `IRAM_ATTR` and keep the cache-on argument beside the hook, or add a HAL `DOMOTICS_IRAM_ATTR` (Constitution IX allows attribute macros in HAL, `Platform_HAL.h` already fills `PROGMEM`) on the store path with the reads behind the planned flag.",
  "potential_consequence": "a false sense of safety in the comment above the function that runs in the worst context the firmware has."
 },
 {
  "lens": "adversarial",
  "location": "'What the cores do' / D3 / release note — ESP8266 stock coverage",
  "trigger_condition": "The components have zero `nothrow` sites; `String` (`WString.cpp:246`), ArduinoJson and lwIP (`pvPortMalloc`, `heap.cpp:355`) allocate through umm's `malloc`/`realloc`, which record nothing on a stock build; what records is `_malloc_r` and friends (`heap.cpp:137-165`) with a newlib-routine address as site — resolvable, not 'a constant'.",
  "guard_snippet": "state in D3 and the release note that on stock ESP8266 the group sees `nothrow new` (none in the framework) and newlib-internal failures only; add a board check: a `String` growth failure reads count 0 stock and count 1 under `-DDEBUG_ESP_OOM` — the removal check for D4's value.",
  "potential_consequence": "the entry announces 'which allocation was survived' on ESP8266 and the first `bootdiag` after a JSON OOM says `failed allocs: 0`."
 },
 {
  "lens": "adversarial",
  "location": "S3 and S5 — the latch-clear mutation",
  "trigger_condition": "Both rows predict that without the clear a repeated failure 'counts 1'; with the clear removed the seam returns the same non-null pair every millisecond, so one failure counts once per sample for the rest of the run. The prediction describes the alternative design, not this mechanism.",
  "guard_snippet": "tests: one failure then N samples → 1; two failures with a sample between → 2; mutation → the first reads N. Read `addr`/`size` under `xt_rsil(15)` in the seam.",
  "potential_consequence": "a mutation check with the wrong expected result is a check nobody can run honestly."
 },
 {
  "lens": "adversarial",
  "location": "D3 versus Lot B's D3 — `FlightRecorder.cpp:211-217` `extraWalkDone_`",
  "trigger_condition": "The latch adds a second once-per-interval `umm_info` walk beside the cliff walk, and the two coincide: a failed allocation is a drop below the cliff threshold in the same millisecond. `umm_info` runs with interrupts off (`umm_info.c:180-183`).",
  "guard_snippet": "share the gate: if `extraWalkDone_` reuse `largestAtMin_`, else walk once and set both; the ESP32 hook's flag can be the same flag. S1's burst test counts the cliff walk too: burst and cliff, one walk total.",
  "potential_consequence": "two interrupts-off heap walks back to back at the moment the ESP8266 heap runs out."
 },
 {
  "lens": "adversarial",
  "location": "D1 / Decision 2 — `LAYOUT` 1 → 2; `test_flight_recorder.cpp:236-243`",
  "trigger_condition": "Without a bump a Lot B record reads torn under the new CRC rule and is still promoted with reason, phase and callback fields (`begin()` :139, `test_torn_record_still_reports_its_phase`); a ten-line `bodyCrcForLayout(1)` keeps it clean. Neither option is under Decision 2. And `test_a_record_with_another_layout_reads_as_power_on` writes `2u << 24`, which becomes the current layout with flags zeroed and is promoted as `UnexpectedReset`.",
  "guard_snippet": "offer the migration branch as the third option; change the test's constant to 1 or 3 and say so in S1; update `FlightRecorder.h:18` and the `bodyCrc()` doc either way.",
  "potential_consequence": "the first boot after the OTA to the new release — the one being watched — reports 'none recorded' over a real death."
 },
 {
  "lens": "adversarial",
  "location": "D4 table / S4 — `FlightRecorder.cpp:63-67` `buildIdCrc()`",
  "trigger_condition": "Without `DOMOTICS_BUILD_ID` the build embeds `__DATE__ \" \" __TIME__`; the stock and `-g` `firmware.bin` differ in those bytes, so 'byte-identical, proven by comparing' cannot come out true.",
  "guard_snippet": "`-DDOMOTICS_BUILD_ID=\\\"diag\\\"` in the `nodemcuv2-diag` env and all four S4 builds; record both `sha256sum`s in the table.",
  "potential_consequence": "a measured claim in a published table that cannot be reproduced, on the row whose point is the comparison."
 },
 {
  "lens": "adversarial",
  "location": "D7 — `crash nothrow`, `crash squeeze`",
  "trigger_condition": "`malloc(1 << 20)` with an unused result is dead code to GCC; an unused `new (std::nothrow)` is elidable under C++14; Lot B hit this twice (`Platform_ESP8266.h:362-366`). `squeeze` holds allocations nowhere reachable, has no release, and leaves an ESP8266 whose lwIP is failing and whose console may not answer.",
  "guard_snippet": "`static volatile` sinks for both (free after `nothrow`; the 1 MiB request cannot succeed: `umm_blocks()` saturates at `INT16_MAX`, `umm_malloc.cpp:360`); `squeeze` keeps a static chunk array with `crash release`, stops on `getAllocatableFreeHeap()`, and D6 states chunk size and what the SDK's own failures do to the loads.",
  "potential_consequence": "the commands that drive D6 and S5 do nothing (elided) or leave a board that cannot be talked to."
 },
 {
  "lens": "adversarial",
  "location": "D7 — `tools/on-device/crash_check.sh:38-58`",
  "trigger_condition": "The script sends with `--expect-drop`, waits for the console to return, then greps `bootdiag`; `nothrow` drops nothing and `bootdiag` prints the promoted (previous) record, not this run's count. The only live signal is the 60 s WARN on serial.",
  "guard_snippet": "a no-drop mode that greps `$LOG` for the WARN within 60 s, or a 'this boot: failed allocs N' line in `bootdiag` — which is also what D6 reads on FullStack, where `LOOPCOST` does not exist.",
  "potential_consequence": "the S5 row for `nothrow` has no automated check and the D6 table has no source on FullStack."
 },
 {
  "lens": "adversarial",
  "location": "D5 — the `format()` line",
  "trigger_condition": "Free and largest are printed in KB from a 16-byte packing — 1 488 B (Lot B's measured floor) prints 'free 1 KB' while neighbouring lines print bytes; the example `0x00000806` is `INTERNAL|8BIT|32BIT`, the measured caps are `INTERNAL|DEFAULT` = `0x1800`; 'site' invites `addr2line` on a bitmask. Length is fine: 105 saturated, 85 example.",
  "guard_snippet": "bytes; `from 0x…` on ESP8266 and `caps 0x…` on ESP32 chosen by the platform byte; the example uses `0x1800`; pin the saturated length in S1.",
  "potential_consequence": "the operator reads 'free 1 KB' for a heap that had 1 488 B and tries to resolve a caps word."
 },
 {
  "lens": "adversarial",
  "location": "D5 — `Core.cpp:45` `text[768]`, `System.h:604-616` `getBootDiagnostics()`",
  "trigger_condition": "`format()` is 669 characters typical and 837 saturated today (computed); the line adds up to 105. `Core::begin()` prints from 768. `getBootDiagnostics()` grows `pos += snprintf(...)` unclamped in `buf[1024]`: once `pos` passes 1024, `sizeof(buf) - pos` wraps and `formatBootDiagnostics()` writes past the buffer.",
  "guard_snippet": "clamp `pos` after each append; print the failed-alloc line before the ring so truncation eats samples; a saturated-values test for the total against both buffers.",
  "potential_consequence": "a stack overflow in the command meant to explain crashes, reachable when a long record meets a long SystemInfo block."
 },
 {
  "lens": "adversarial",
  "location": "D5 — the 60 s WARN; `Core.cpp:104-111`",
  "trigger_condition": "'The tick logs one WARN' puts a logger call inside `FlightRecorder::tick()`, in the file that is log-free and allocation-free because it is linked into the crash path and the hook; the drop line the plan cites as the model lives in `Core::loop()`.",
  "guard_snippet": "`Core::loop()` reads the count from `recorder.current()` beside the drop counter and logs with the same once-per-60-s pattern; the recorder exposes the count only.",
  "potential_consequence": "a logger dependency in the one file that must stay callable from a dying context."
 },
 {
  "lens": "adversarial",
  "location": "'What the cores do' (ESP32) and Residuals — PSRAM",
  "trigger_condition": "`sdkconfig.h:239-241` has `CONFIG_SPIRAM 1`, `CONFIG_SPIRAM_USE_MALLOC 1`, `ALWAYSINTERNAL 4096`; one firing holds because `esp_spiram_init()` fails on the bench boards and `psramInit()` returns before `heap_caps_malloc_extmem_enable()` (`esp32-hal-psram.c:69-93`), leaving `malloc_alwaysinternal_limit` at `0xffffffff` (object dump). On a PSRAM board `heap_caps_malloc_default` (disassembled two-attempt path) fires twice for sizes over 4 096 with caps `SPIRAM` first; `heap_caps_malloc_prefer` fires per candidate.",
  "guard_snippet": "write the mechanism into the entry: count doubles on PSRAM boards, the group keeps the last firing's caps, the WARN's N counts attempts.",
  "potential_consequence": "the first PSRAM user reads twice the failures with a caps word that says SPIRAM and files a bug against the hook."
 },
 {
  "lens": "adversarial",
  "location": "D4 — the cost table; `heap_api_debug.h:23`, `umm_malloc_cfg.h:663-667`, `heap.cpp:25-31`, `hwdt_app_entry.cpp:118-135, 183`",
  "trigger_condition": "`-DDEBUG_ESP_OOM` also defines `MEMLEAK_DEBUG`, turns every umm debug print into `ets_uart_printf` regardless of `system_get_os_print()`, makes `heap_pvPort*` real IRAM functions (that is the IRAM cost), and rewrites every `malloc(`/`calloc(`/`realloc(` token in every TU including ArduinoJson's allocator. `DEBUG_ESP_HWDT` prints a greeting at every boot by default; its two variants differ by 4 KB of heap and share one table row.",
  "guard_snippet": "two HWDT rows with expected heap delta 0 / −4 096 and the greeting noted; the OOM row lists the four effects so the measured numbers have their explanation; the recipe says the flag must reach every library (PlatformIO `build_flags` do, a sketch `#define` does not).",
  "potential_consequence": "a cost table right in its numbers and wrong about where they come from, and a serial reader that trips on a greeting."
 },
 {
  "lens": "adversarial",
  "location": "D2 — registration order in `FlightRecorder::begin()` (`FlightRecorder.cpp:120-167`)",
  "trigger_condition": "`begin()` memsets `current_` and `writeAll()`s it; a hook registered beside `installRestartHook()` (line 125) can fire from another task during those lines and have its group erased, or run against a half-cleared RAM group.",
  "guard_snippet": "register last, after the final `writeAll()`/hold decision; the stub's 'registration observed' test asserts the order through the stub's write log.",
  "potential_consequence": "a failure in the first milliseconds of bring-up — the boot loop the recorder exists for — is the one lost."
 },
 {
  "lens": "adversarial",
  "location": "'Docs in the PR' — `CODE-ROADMAP.md:3350, 3362-3381`, `FlightRecorder.h:18`, `ci.yml:154-158`",
  "trigger_condition": "The OBS-4 entry's Fix paragraph still says the ESP8266 half is 'the crash callback copies the two words' (Lot B) and the group is five plain words; OBS-3's residual says 'Lot C latches and clears it'; the header documents the old `w56-60`. CI's opt-out build sets all three defines off together, so hooks-off/tick-on is not compiled anywhere.",
  "guard_snippet": "rewrite the OBS-4 entry rather than append DONE; strike the OBS-3 bullet; update the header line and `bodyCrc()` doc; cross-compile hooks-off/tick-on once in S2.",
  "potential_consequence": "the roadmap sweep finds a DONE entry that describes a different fix."
 }
]
```
