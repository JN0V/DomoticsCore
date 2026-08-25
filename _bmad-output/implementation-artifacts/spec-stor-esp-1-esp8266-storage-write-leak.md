---
title: 'STOR-ESP-1 — the ESP8266 Storage write leak does not exist'
type: 'chore' # test-suite correction; no product code changes. Commit as `test(storage):`, never `fix(storage):` — a fix prefix would re-assert the leak this spec retracts.
created: '2026-08-23'
status: 'in-review'
review_loop_iteration: 1
baseline_commit: '07cb37a9b3a4e7aadcc7a5bb10982935a7ec617d'
context:
  - '{project-root}/.specify/memory/constitution.md'
  - '{project-root}/docs/components/storage/project-context.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** STOR-ESP-1 was filed as a Storage write leak of ~122 B per operation, unbounded, exhausting a D1 mini's heap in about seven hours. It is not one. `StorageComponent` emits `storage/changed` on every put and remove; `EventBus` queues each event at roughly 122 B and releases it only when `poll()` dispatches it, which firmware reaches through `Core::loop()` every pass. **The heap suite never called `core.loop()`**, so it measured queue occupancy and charged it to Storage. The queue is capped at 32 and drops the oldest to make room (`EventBus.h:238-244`), so the growth is bounded — but 20 iterations never reached that ceiling, which is why bounded looked unbounded.

**Approach:** Leave `Storage_ESP8266.h` alone — there is nothing in it to fix. Correct the suite to drive the lifecycle firmware actually runs, and add tests that pin the bounded behaviour in place so the same misreading cannot be filed twice.

## Boundaries & Constraints

**Always:**
- `Storage_ESP8266.h` stays byte-identical to `07cb37a9`. Any diff there is the defect this spec exists to undo.
- Thresholds stay at 64 B (128 B for the namespace test). The fix is to measure the right thing, never to widen the tolerance.
- Every claim is verified on the board. The suite is excluded from native runs (`platformio.ini:6`), so a green host run says nothing about it.
- No version bump, no CHANGELOG entry — the lot rule.
- Files and commit messages in English; conventional commits.

**Ask First:**
- **Reclassifying STOR-ESP-1 in `docs/CODE-ROADMAP.md`.** It is one of two HIGH items on the Priority-2 row; retiring it moves that row and the total. The file also carries uncommitted edits from a concurrent session (BUG-27, DC-11, DC-12) that must not be swept into this lot.
- **`EventBus::enqueue` does not decrement `pendingByTopic` when it drops an event on overflow** (`EventBus.h:238-251` against `poll()`'s decrement at `:203-207`). The counter drifts up and never returns, which inhibits sticky replay for that topic. Real, bounded, and in another component.
- **`HeapTracker` charges its own checkpoint map node to every measured window** (`HeapTracker.h:96-101`). It did not matter here — the signal was 30-60× the threshold — but it will for anything landing near 64 B.

**Never:**
- Do not "fix" `Storage_ESP8266.h`. Candidates A (`shrinkToFit` per mutation) and B (per-operation load) were both measured on the board and moved the number by zero bytes.
- Do not delete the undrained-path tests to keep the suite simple. They are the guard against re-filing this.
- Do not touch the concurrent session's roadmap edits.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Writes with the bus drained | 20 puts, `core.loop()` each pass | Heap delta ≤ 64 B | N/A |
| put/get/remove with drain | 20 cycles, distinct keys | Heap delta ≤ 64 B | N/A |
| Writes with no drain, past the cap | 80 puts, never drained | First 40 grow; **second 40 cost ≤ 64 B** — occupancy plateaus at the 32-entry cap | N/A |
| Drain after filling the queue | 40 puts, then 10 `core.loop()` | Heap returns to within 64 B of baseline | N/A |
| Namespace open/use/close | 5 full cycles | Heap delta ≤ 128 B | N/A |
| Storage never opened | `opened == false` | Every put returns `false`; the liveness assert fires before any measurement | Test fails loudly, not silently |

</frozen-after-approval>

## Code Map

- `DomoticsCore-Storage/test/test_heap_esp8266/test_heap_esp8266.cpp` — the whole change. Header explains why the drain is load-bearing; `core.loop()` added to the three measuring loops; two new tests appended before the runner and registered in `setup()`.
- `DomoticsCore-Core/include/DomoticsCore/EventBus.h` — read-only evidence. `enqueue` :236-252 (cap 32, drops oldest, increments `pendingByTopic` :249); `poll` :174-219 (dispatches ≤ 8 per call, decrements `pendingByTopic` :203-207). The asymmetry on the drop path is the new finding under Ask First.
- `DomoticsCore-Storage/include/DomoticsCore/Storage.h` — read-only. `emit(EVENT_CHANGED, ev)` on every put (:211, :239, :267, :295, :323, :352) and on remove/clear (:530, :555). One event per mutation is what fills the queue.
- `DomoticsCore-Storage/include/DomoticsCore/StorageEvents.h` — `StorageChangedEvent` is `char key[64]` :21-23; copied into a `std::vector<uint8_t>` alongside a `String topic`, which is where ~122 B per event comes from.
- `DomoticsCore-Storage/include/DomoticsCore/Storage_ESP8266.h` — **unchanged, and must stay so.**

## Tasks & Acceptance

**Execution:**
- [x] `test_heap_esp8266.cpp` — carry the measured delta into each assertion message via `snprintf`, so a failure names the number instead of "Expected TRUE Was FALSE". No threshold changed.
- [x] Measure candidates A and B on the board under a fixed protocol. Both returned the baseline figures to the byte; C is subsumed by B, which already holds no resident document.
- [x] `test_heap_esp8266.cpp` — call `s.core.loop()` in the three measuring loops, with the reason stated where it is called.
- [x] `test_heap_esp8266.cpp` — add `test_storage_undrained_writes_plateau`: 40 writes, checkpoint, 40 more, assert the second half costs ≤ 64 B. Encodes the boundedness the original finding could not see.
- [x] `test_heap_esp8266.cpp` — add `test_storage_drain_reclaims_queue_memory`: fill the queue, drain it, assert heap returns to baseline. The other half of the claim.
- [x] `test_heap_esp8266.cpp` — register both in `setup()`; explain the drain in the file header.
- [ ] `docs/CODE-ROADMAP.md` — reclassify STOR-ESP-1 and repair the Priority-2 row and total. **Gated under Ask First**, and blocked on the concurrent session's edits.

**Acceptance Criteria:**
- Given `Storage_ESP8266.h` identical to `07cb37a9`, when `pio test -e esp8266dev` runs on the board (`platformio.ini` `[env:esp8266dev]`, `board = nodemcuv2`, an ESP8266 D1 mini on `/dev/ttyUSB0`), then all seven tests pass — **met: 7/7, 46.29 s**.
- Given a queue that never fills, when either new test runs, then it fails rather than reporting success — **met by construction: floor assertions at `firstHalf >= 1000` and `held >= 1000`, both exercised at ~4160 B on the board.**
- Given the suite before this change, when it ran on the same board, then three tests failed at 3856 / 3904 / 2448 B — **met: reproduced twice, bit-identical, run-to-run spread 0 B**.
- Given only `core.loop()` added and no library change, when the suite ran, then every previously failing test passed — **met: 5/5 at that step**.
- Given the cap is later removed from `EventBus::enqueue`, when the suite runs, then `test_storage_undrained_writes_plateau` fails — by construction; not separately exercised.
- Given the native targets, when the Storage suites are built from a clean `.pio`, then they still pass — **met: 54/54**.
- Given the library headers, when compared against the baseline commit, then the diff is empty — **met: `git diff 07cb37a9 -- DomoticsCore-Storage/include/` returns 0 lines**.

## Spec Change Log

- **Trigger:** step-03 implementation reported that candidates A, B and C all moved the measured figure by zero bytes, and that adding `core.loop()` to the suite turned every failing test green against unmodified library code. That falsified the Problem statement inside `<frozen-after-approval>` — an `intent_gap`, not a `bad_spec`, since the root cause sat in human-owned intent.
- **Verification before amending:** the report was not taken at face value. Confirmed independently: the suite contained no `core.loop()` (zero grep hits); `EventBus.h:238` caps the queue at 32 and drops the oldest; `StorageChangedEvent` is `char key[64]`; `Storage.h` emits on all eight mutation paths. The arithmetic closes — 2448 B over 20 undrained events is 122.4 B each, and 3904 B is 32 of them at the queue cap. Then re-measured on hardware twice: first with the drain on one test (that test flipped to PASSED, the other two unchanged to the byte), then on all three (5/5 PASSED).
- **Amended:** Intent, Boundaries and the I/O Matrix rewritten. The frozen block was reopened on explicit human instruction.
- **Known-bad state avoided:** shipping a change to `Storage_ESP8266.h`. Candidate A would have added a reallocation per write, and B a full parse per operation — both on top of the full-file `save()` already performed — to fix a leak that was never there. The roadmap would then have recorded a fictitious fix.
- **KEEP:** the `snprintf` assertion messages from task 1. They are what made the campaign legible, and they survive re-derivation. Also keep the differential design of the original suite — distinct-keys versus single-key was the right instrument; it was simply read against too short a run.

- **Trigger (review round 1, four layers):** three of the four independently reported that the two new tests assert only their "after" side, so both would pass green if events ever stopped being queued — the concrete scenario being an `enqueue` that skips topics with no subscriber, which this suite has none of. Classified `patch`: caused by the change, fixable without renegotiating intent.
- **Amended:** a floor assertion added ahead of each conclusion (`firstHalf >= 1000`, `held >= 1000`), and both new tests now drain the bus before opening their measurement window, since `begin()` and the warm-up put queue events whose release would otherwise offset real growth.
- **What that uncovered:** with a clean baseline the reclaim test failed at **128 B held after draining**, against 4160 B queued. The pre-baseline events had been masking it. The residue is one-time — the topic's `pendingByTopic` entry plus `HeapTracker`'s own checkpoint nodes — so the test was rebuilt to measure two identical fill-and-drain cycles and assert the second is free. **7/7 on the board after the change.**
- **Known-bad state avoided:** raising the reclaim threshold past 128 B to make the failure go away. That would have hidden a genuine 128 B per-cycle leak behind the same tolerance.
- **Corrected in this round:** `20 × 122 = 2440`, not 2448 — the figure is 122.4 B per event; only single-key churn measured 3904 B, distinct keys measured 3856; `type` changed from `bugfix` to `chore` with the commit prefix stated, since a `fix(storage):` in tracked history would re-assert the retracted leak.
- **Deferred, not fixed:** `EventBus::enqueue` leaves `pendingByTopic` incremented for events it drops on overflow; `HeapTracker::getDelta` returns a multi-kilobyte "leak" rather than an error for a mistyped checkpoint name; the new tests leave two LittleFS namespaces resident and never `shutdown()` their `Core`; and roughly 120 extra flash writes per run.
- **Noted, not actioned:** `EventBus`'s 32-entry cap is already covered natively by `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp:165-187`, which CI does run — so boundedness is not defended solely by a suite no runner executes.

## Design Notes

Why the original reading was defensible and still wrong: the suite's differential design was sound, and per-key versus per-operation was the right question. What it lacked was a run long enough to distinguish a plateau from a slope. Test 4 queued 20 events against a 32-entry cap and never reached it, so the curve looked linear. Tests 2 and 3 queued 40 and sat on the ceiling — single-key churn at 3904 B, which is 32 × 122 B exactly; distinct keys at 3856 B, 48 B under it — and that read as "worse", when it was the bound announcing itself.

The two new tests are built so the mistake cannot be repeated in either direction: the plateau test fails if the cap disappears, and the reclaim test fails if the queue genuinely leaks. Both open with a floor assertion, because a guard that passes when nothing is queued guards nothing.

The reclaim test is measured differentially over two identical fill-and-drain cycles rather than against a single baseline. One cycle does not return to zero: it leaves ~128 B, and widening the threshold to swallow that would blind the test to a real 128 B leak. Two cycles separate the two cases — a genuine loss recurs, a one-time allocation does not. The residue is one-time and has two known sources, both recorded under Ask First: `EventBus` never erases a topic's entry from `pendingByTopic`, and `HeapTracker` charges each checkpoint's own map node to the window that follows it.

## Verification

**Commands:**
- `cd DomoticsCore-Storage && rm -rf .pio && pio test -e esp8266dev` — expected: 7/7 pass. **Run: 7/7, 46.29 s.**
- `cd DomoticsCore-Storage && rm -rf .pio && pio test -e native` — expected: `test_storage_api` and `test_storage_events` green.
- `git diff 07cb37a9 -- DomoticsCore-Storage/include/` — expected: **empty**.
- `git status` — expected: no `.pio/` artefact, no version or CHANGELOG change.

**Manual checks:**
- Read `/dev/ttyUSB0` directly to capture the printed deltas; PlatformIO filters them from its own output.
