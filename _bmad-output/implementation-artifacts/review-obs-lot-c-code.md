# Code review — OBS Lot C (`obs-lot-c` vs `main` at `c2e5db1`)

**Reviewed**: `git diff c2e5db1` over the working tree on 2026-09-06, after
S1–S4 were built and while S5 (the WROOM-32D campaign) was running —
**before the PR**, the order Lot B's handoff asked for. S1 committed
(`91baecb`), S2 staged, S3/S4 unstaged. Spec: `spec-obs-lot-c-oom-moment.md`
v2 (mode full). **Four layers**, `bmad-code-review`: Blind Hunter (16),
Edge Case Hunter (12), Verification Gap (4 surviving mutations + 4 notes),
Acceptance Auditor (9, plus the 19 folds of the plan review checked one by
one — all honoured). 41 raw findings → 33 after merging → **24 patched,
1 deferred, 4 dismissed, 4 were already done by the time the report was
written** (docs the campaign was still filling).

**Verdict: the mechanism is right and the tests discriminate where they
claim to — two control mutations went red as advertised — but the diff
shipped four silent gaps and one misattribution.** The gaps: the
once-a-minute line's change gate, the count-last store order, the
per-platform site label and the boot-log buffer had no observer, each
mutation surviving green. The misattribution: a survived failure that
reaches the crash callback under an exception (not an abort/OOM) within
the millisecond before the latch was reported as the death's cause — the
very residual the lot closes, reopened by a one-millisecond window. Plus
two things the boards would have shown later: a deliberate
`DOMOTICS_CRASH_HOOKS=0` warned "hook not registered" on every ESP32
boot, and `crash nothrow` / `squeeze` would allocate from PSRAM on a board
that has it.

---

## Findings and what was done

Severity is by consequence for the firmware's user; the reviewers' own
ratings were discarded per the workflow.

### Patched — code

1. **A deliberate opt-out warned on every ESP32 boot** (acceptance + edge +
   blind + verification, `Platform_ESP32.h` `supportsFailedAllocHook`,
   `Core.cpp:44`) — medium. `supportsFailedAllocHook()` now returns
   `DOMOTICS_CRASH_HOOKS != 0` (the header defines the default itself, since
   `FlightRecorder.h`'s comes later); the unused `s_failedAllocHook` moved
   inside the `#if`. The warning stays for IDF's genuine refusal.
2. **A survived failure meeting an exception before the latch** (edge,
   `FlightRecorder.cpp` `recordCrash`) — medium. For any reason other than
   253/254 the callback's fail fields are a survived failure the sampler
   had not latched: `recordCrash()` routes them to the group (no heap
   walk in crash context) and zeroes them. Pinned:
   `test_a_survived_failure_that_reaches_the_callback_with_an_exception_goes_to_the_group`.
3. **The walk gate reset by `tick()` under the hook** (edge,
   `noteFailedAlloc`) — low. The hook reads `largestAtMin_` once; a 0
   (the tick reset it between the two reads) triggers a walk. The
   `runMin_` race is a minimum and stays, said in a comment.
4. **Two live readers outside the section** (acceptance + edge,
   `Core.cpp`, `System.h`) — low. `failedAllocSnapshot(count, size)` takes
   the section; both readers use it.
5. **User hook re-entrancy** (edge) — low. A flag: a user hook that
   allocates and fails re-enters once, not forever.
6. **Latch silently dead under `DOMOTICS_FLIGHT_RECORDER_TICK=0`** (edge +
   blind) — medium. Decision: keep the latch in the sampler (the S5
   removal check depends on it) and say so — `Core::begin()` warns
   "Sampler compiled out: survived allocation failures will not be
   recorded on this platform" when the platform has no hook; the define's
   comment names it.
7. **`crash nothrow` / `squeeze` on a PSRAM board** (blind + edge) —
   medium for the bench. Both allocate `MALLOC_CAP_INTERNAL | 8BIT` on
   ESP32; `nothrow` returns false when the request succeeded (both
   platforms), so the console says "usage" instead of "done".
8. **`squeeze` counting itself** (blind) — low. The loop also stops when
   the largest block is under the chunk, so the failing `malloc` that would
   fire the hook (or the latch under `DEBUG_ESP_OOM`) never happens.
9. **`Core::loop()`'s statics leaking across instances** (blind) — low.
   The two once-a-minute pairs (drops, failures) are `Core` members now.
10. **`<new>` inside `extern "C"`, a nested `extern "C"`, the pair declared
    twice** (acceptance + blind, `Platform_ESP8266.h`, `FlightRecorder.cpp`)
    — low. Hoisted, flattened, declared once in the HAL.
11. **The survived-kind list duplicated by string compare in `System.h`**
    (blind) — low. `HAL::Platform::crashKindSurvives()` in `Platform_HAL.h`.
12. **`text[1024]`'s comment carried a figure** (blind) — low. Names the
    test that sizes it instead.
13. **`onFailedAlloc()` had no contract** (blind) — low. One line: the
    failing task's context, inside the allocator, no allocation, no log.
14. **`noteFailedAlloc`'s five-line doc and the trampoline's four** (acceptance)
    — low. Two and three lines.

### Patched — tests (the Verification Gap's four, plus two)

15. **The once-a-minute change gate unpinned** — the test now loops a
    minute later with no new failure and asserts silence (the time-gate-only
    mutation was green).
16. **Count-last store order unobserved** — stub seam
    `lastRtcStoreOffsetForTest`; `test_the_count_word_is_stored_last`.
17. **The per-platform site label never executed** — records with the
    platform byte at 1 and 2: `from 0x4020b41c` and `caps 0x1800`. Found on
    the way: `%0*lx`'s width is a minimum, not a truncation — the first
    version of the test expected `0xb41c` and was wrong, the code was right.
18. **`Core::begin()`'s buffer never read through** —
    `test_begin_logs_every_ring_line_of_a_saturated_record`: four `ring:`
    lines reach the log (red at 768).
19. **Count saturation at the recorder level** (blind) — 65 537 firings,
    the count reads 65 535 and the group stays valid.
20. **`crash squeeze` / `release` through the console** — the lifecycle
    test asserts `done:` for both.

### Patched — tools and docs

21. **`crash_check.sh nothrow` demanded exactly 1** (blind + edge) —
    reads the count before and passes on +1; keeps the serial capture (the
    `:oom(` line); `squeeze` and `release` pass on the console's reply.
22. **README: `DEBUG_ESP_HWDT` read as part of the diag env** (blind);
    **the ini comment's wrong claim and seven lines** (acceptance + blind);
    **the plan's v1 sentences on the flush** (acceptance); **the roadmap
    counts and the 21/23 slip** (acceptance + blind) — all fixed; the
    `{{WROOM}}` row is filled from the campaign below.
23. **The postmortem line's changed meaning must be in the owed
    announcements** (blind) — added to the handoff's list (seven, not six).

### Deferred

24. **Survived failures during a bring-up that dies before `acknowledge()`
    stay in RAM and are lost** (edge) — by design: while a promoted death is
    held in RTC, writing the new run's group over `w56-60` would attribute
    this run's failures to the old death on the next boot. Recorded as a
    residual in the OBS-4 entry with the Lot B one on uncounted deaths in a
    crash loop. Appended to `deferred-work.md`.

### Dismissed

- `squeeze` returning false when 12 KB is not reached (edge): "usage:" on
  the console would mislead more than "done:" does; the heap line is the
  reading.
- A later registrant replacing the hook (edge): IDF's single slot is
  documented in the Core reference with the two ways around it.
- The saturated `bootdiag` test not pinning the clamp (verification note):
  known and recorded — the overrun is eight bytes short of reachable.
- "`esp32c3` is compiled by nothing until the PR" (verification note):
  S1, S2 and S3 each cross-compiled FullStack on esp32c3 locally (logs in
  the session), and CI will again.

## Verification after the patches

Native after the patches: Core **154** (149 + 5), System **84** (two
assertions added to one test; `crash squeeze`/`release` through the
console, the snapshot path). Cross-compiles after the patches: FullStack
esp32dev, esp8266dev, esp32c3, the CI opt-out triple, hooks-off/tick-on,
tick-off/hooks-on, and the probe on nodemcuv2, esp32dev and
nodemcuv2-diag — results in `HANDOFF-2026-09-06-lotc.md`.

## What the campaign was saying while this review ran

The WROOM-32D under FullStack with the commands: `oom` → the group beside
`unexpected reset`; `nothrow` → the WARN and the "this boot" line; **ten
minutes idle, one 1.35 MB upload: zero firings**; the WebUI reading was
void (Playwright not installed on this host — redone with plain HTTP after
the campaign); **squeezed to 12 KB the device fired 13 times within
seconds, the console stopped answering, and it died of the task watchdog
twelve minutes later after 168 survived failures**, the last at 4 752 B
free and 1 264 B largest — the record of a death by starvation, which is
what the lot was for.
