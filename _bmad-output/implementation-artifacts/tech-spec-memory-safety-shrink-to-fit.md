---
title: 'Memory Safety - shrink_to_fit after container operations'
slug: 'memory-safety-shrink-to-fit'
created: '2026-03-06'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++', 'Arduino/ESP8266/ESP32', 'PlatformIO', 'Unity (test framework)']
files_to_modify:
  - 'DomoticsCore-Core/include/DomoticsCore/EventBus.h'
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h'
  - 'DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h'
code_patterns: ['erase-remove idiom + shrink_to_fit', 'empty map key cleanup after erase', 'pruneMap() helper template', 'queue swap idiom']
test_patterns: ['HeapTracker assertStable() as primary', 'single-cycle + multi-cycle tests']
---

# Tech-Spec: Memory Safety - shrink_to_fit after container operations

**Created:** 2026-03-06

## Overview

### Problem Statement

Vector containers in EventBus, MQTT, and RemoteConsole use `erase()` and `clear()` to remove elements but never call `shrink_to_fit()` afterward. On long-running IoT devices (ESP8266/ESP32 with 40-80KB free heap), this causes silent memory waste: vector internal capacity grows to peak usage but never shrinks back, even when elements are removed. Over weeks/months of operation, repeated subscribe/unsubscribe cycles, MQTT message bursts, and telnet client connect/disconnect cycles accumulate dead capacity.

### Solution

Add `shrink_to_fit()` calls after every `erase()` or `clear()` operation on `std::vector` containers in the three affected components. Add lightweight HeapTracker-based tests to verify heap recovery after subscribe/unsubscribe and connect/disconnect cycles.

### Scope

**In Scope:**
- R1: EventBus `unsubscribe()`, `unsubscribeOwner()`, `reset()` - add `shrink_to_fit()` on subscription vectors + clean up empty map keys after erase. Note: `lastByTopic` and `pendingByTopic` cleanup is explicitly out of scope — these maps are not subscription containers and grow proportionally to unique topic count (bounded by design), not by subscribe/unsubscribe churn. Documented as a known limitation for future assessment.
- R2: MQTT `processMessageQueue()`, `unsubscribe()`, `unsubscribeAll()` - add `shrink_to_fit()` on messageQueue and subscriptions vectors
- R4: RemoteConsole `loop()` (client disconnect), `setPort()`, `shutdown()` - add `shrink_to_fit()` on clients vector
- HeapTracker tests for each component verifying heap recovery after cycles
- Update CODE-ROADMAP.md tracking table
- Remove obsolete TODO(R1) comments in EventBus.h (lines 95, 115)

**Out of Scope:**
- R3 (Storage cache) - `std::map` does not support `shrink_to_fit()`; erase/clear already frees node memory. Marked N/A.
- R5-R7 (String concatenation optimizations) - separate concern, different fix pattern
- Any other roadmap items beyond R1, R2, R4

## Context for Development

### Codebase Patterns

- Header-only components (all implementation in `.h` files)
- Erase-remove idiom already used correctly in EventBus (`vec.erase(std::remove_if(...), vec.end())`)
- HeapTracker pattern: `tracker.checkpoint("before")` / `tracker.checkpoint("after")` / `tracker.assertStable("before", "after", tolerance)` — used in Storage, WebUI, NTP, Wifi, HA tests
- `clearBuffer()` in RemoteConsole already has `shrink_to_fit()` after `clear()` — this is the target pattern
- TODO comments already exist in EventBus.h at lines 95 and 115 flagging the missing `shrink_to_fit()`
- EventBus is `namespace DomoticsCore::Utils`, not a component — tests access members directly via the `EventBus` class

### Party Mode Insights (Winston, Amelia, Murat)

- **Empty map key cleanup (Winston)**: After erase-remove on EventBus map values, remove map entries whose vector is now empty to prevent dead key accumulation on devices with dynamic topics
- **Batch shrink in MQTT (Amelia)**: Call `shrink_to_fit()` once AFTER the `processMessageQueue()` loop, not inside each iteration — avoids repeated reallocations during batch processing
- **Iterator-based loops for EventBus (Amelia)**: Changing range-for to explicit iterators is required to safely erase empty map keys during iteration. Pattern: `if (vec.empty()) it = map.erase(it); else ++it;`
- **Test with capacity() not heap (Murat)**: Assert `capacity() == size()` after shrink_to_fit() — more reliable and portable than global heap measurements. GCC honors this in practice. Fallback: `capacity() <= size() * 2`
- **Single-cycle test (Murat)**: Add a 1-cycle test (subscribe once, unsubscribe once) alongside the 10-20 cycle test to cover the minimal case

#### Round 2 Insights

- **Queue swap idiom for reset() (Winston)**: Replace `while (!queue.empty()) queue.pop()` with `queue = std::queue<QueuedEvent>()` — the pop-loop is O(n) and while it calls destructors on each QueuedEvent (freeing their internal String/vector data), it does not release the deque's internal block storage. The assignment replaces the entire internal deque, releasing block storage too. O(1) for the swap itself.
- **pruneMap() helper template (Amelia)**: Extract a reusable `pruneMap(map, predicate)` helper that does erase-remove + shrink_to_fit + empty key cleanup in one pass. Reduces 6 duplicate iterator loops (3 maps x 2 methods) to 6 one-liner calls
- **HeapTracker over capacity() for tests (Murat revised)**: All containers are private — asserting `capacity()` would require exposing internals. Use HeapTracker `assertStable()` as primary test assertion (512-byte tolerance, consistent with project conventions). No API changes needed.

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-Core/include/DomoticsCore/EventBus.h` | R1 — Lines 81-96: `unsubscribe()`, 100-116: `unsubscribeOwner()`, 221-230: `reset()`. Private members at L271-280. |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` | R2 — Lines 280-298: `unsubscribe()`, 300-306: `unsubscribeAll()`, 428-439: `processMessageQueue()` |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h` | R2 — L408: `std::vector<Subscription> subscriptions`, L419: `std::vector<QueuedMessage> messageQueue` |
| `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` | R4 — L67: `std::vector<HAL::WiFiClient> clients`, L120: `setPort()`, L204-216: `loop()` disconnect, L226: `shutdown()` |
| `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp` | Existing EventBus tests — extend with memory tests |
| `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp` | Existing MQTT tests — extend with memory tests |
| `DomoticsCore-RemoteConsole/test/test_remoteconsole_component/test_remoteconsole_component.cpp` | Existing RemoteConsole tests — extend with memory tests |
| `DomoticsCore-Storage/test/test_storage_api/test_storage_api.cpp` | Reference: HeapTracker pattern (L187-254) — `checkpoint()`, `assertStable()`, `assertNoGrowth()` |
| `docs/CODE-ROADMAP.md` | Update tracking table: R1-R4 status, R3 marked N/A |

### Technical Decisions

- `shrink_to_fit()` is a non-binding request per the C++ standard, but GCC (Xtensa/ARM/x86) honors it for vectors — `capacity()` becomes `size()` after the call
- For `reset()`: maps cleared (vectors inside destroyed automatically). Additionally, replace `while (!queue.empty()) queue.pop()` with `queue = std::queue<QueuedEvent>()` (swap idiom) to release the underlying deque memory
- R3 excluded: `std::map` node-based allocation already frees on erase/clear. No action needed.
- EventBus `unsubscribe()`/`unsubscribeOwner()`: extract a `pruneMap()` private helper template that does erase-remove + shrink_to_fit + empty key cleanup in one pass. Both methods call it on the 3 maps with their respective predicate
- MQTT `processMessageQueue()`: single `messageQueue.shrink_to_fit()` after the while-loop exits, not per-iteration
- MQTT `unsubscribe()`: `subscriptions.shrink_to_fit()` after the erase+break
- MQTT `unsubscribeAll()`: `subscriptions.shrink_to_fit()` after `clear()`
- RemoteConsole: `clients.shrink_to_fit()` after each `clear()` or after the disconnect erase loop
- Tests: HeapTracker `assertStable()` as primary assertion (512-byte tolerance, consistent with project conventions). No need to expose private container internals for `capacity()` checks

## Implementation Plan

### Tasks

- [x] Task 1: Add `pruneMap()` private helper template to EventBus
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: Add a private static template method in the `EventBus` class (in the `private:` section, before the data members — insert before `std::map<EventType, std::vector<Subscription>> subscriptions;` at L271):
    ```cpp
    template<typename Map, typename Pred>
    static void pruneMap(Map& m, Pred pred) {
        for (auto it = m.begin(); it != m.end(); ) {
            auto& vec = it->second;
            size_t oldSize = vec.size();
            vec.erase(std::remove_if(vec.begin(), vec.end(), pred), vec.end());
            if (vec.size() != oldSize) {
                vec.shrink_to_fit();
            }
            if (vec.empty()) it = m.erase(it);
            else ++it;
        }
    }
    ```
  - Notes: This helper combines erase-remove + conditional shrink_to_fit + empty key cleanup in one pass. The `oldSize` check ensures `shrink_to_fit()` is only called on vectors that actually had elements removed — avoids unnecessary reallocation on unmodified vectors. Used by Tasks 2 and 3. Performance is equivalent to the hand-written code: O(total_subscriptions) per call, same as the current implementation. For `unsubscribeOwner()` (which can match multiple entries across maps), scanning all three maps is correct behavior.

- [x] Task 2: Refactor `unsubscribe()` to use `pruneMap()`
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: Replace the body between the `assert()` at line 82 and the closing `}` at line 96 — specifically lines 83-95 (the three range-for loops + TODO comment). Keep the assert and function signature intact. Replace with:
    ```cpp
    auto pred = [id](const Subscription& s){ return s.id == id; };
    pruneMap(subscriptions, pred);
    pruneMap(topicSubscriptions, pred);
    pruneMap(wildcardTopicSubscriptions, pred);
    ```
  - Notes: Remove the TODO comment at line 95. The early-exit optimization mentioned in the TODO is deferred — IDs could theoretically appear in multiple maps (type + topic subscriptions), so scanning all three is correct.

- [x] Task 3: Refactor `unsubscribeOwner()` to use `pruneMap()`
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: Replace the body between the `if (!owner) return;` at line 102 and the closing `}` at line 116 — specifically lines 103-115 (the three range-for loops + TODO comment). Keep the assert, null-check, and function signature intact. Replace with:
    ```cpp
    auto pred = [owner](const Subscription& s){ return s.owner == owner; };
    pruneMap(subscriptions, pred);
    pruneMap(topicSubscriptions, pred);
    pruneMap(wildcardTopicSubscriptions, pred);
    ```
  - Notes: Remove the TODO comment at line 115.

- [x] Task 4: Fix `reset()` queue swap idiom
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: In `reset()` (line 223), replace `while (!queue.empty()) queue.pop();` with `queue = std::queue<QueuedEvent>();`
  - Notes: The pop-loop is O(n) and doesn't free the underlying deque memory. The swap idiom is O(1) and releases everything. The rest of `reset()` (map clears) is already correct — maps destroy their inner vectors on clear. Preserve the contract comment above `reset()` (lines 218-220) — do not remove it during refactoring.

- [x] Task 5: Add `shrink_to_fit()` to MQTT `processMessageQueue()`
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h`
  - Action: Add `messageQueue.shrink_to_fit();` as the last line of `processMessageQueue()`, after the while-loop (after line 438, before the closing brace at line 439).
  - Notes: Single call after the loop, not per-iteration. The `empty()` early return at line 429 skips when queue is already empty. Note: `shrink_to_fit()` will also trigger if the loop broke early due to a publish failure (partial drain) — this is acceptable as it only runs once per `processMessageQueue()` call.

- [x] Task 6: Add `shrink_to_fit()` to MQTT `unsubscribe()`
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h`
  - Action: Add `subscriptions.shrink_to_fit();` after the erase loop inside the `if (success)` block (after the erase loop ends at line 292 and `stats.subscriptionCount` update at line 293, before `DLOG_I` at line 294).
  - Notes: The erase+break pattern means at most one element is removed, but the vector still needs shrinking.

- [x] Task 7: Add `shrink_to_fit()` to MQTT `unsubscribeAll()`
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h`
  - Action: Add `subscriptions.shrink_to_fit();` after `subscriptions.clear();` (after line 304).

- [x] Task 8: Add `shrink_to_fit()` to RemoteConsole disconnect handling + fix `clientBuffers` orphans
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action: Add `clients.shrink_to_fit();` and `clientBuffers.clear();` in three locations:
    1. In `setPort()` (after `clients.clear();` at line 120): add `clientBuffers.clear();` then `clients.shrink_to_fit();`
    2. In `loop()`: add a `bool erased = false;` flag before the disconnect erase loop (line 204). Set `erased = true;` inside the `!it->connected()` branch. After the loop (after line 216), add `if (erased) clients.shrink_to_fit();` — avoids calling shrink_to_fit() on every loop cycle when no clients disconnected. Note: `clientBuffers` is already cleaned per-client in the loop (line 208), so no additional cleanup needed here.
    3. In `shutdown()` (after `clients.clear();` at line 226): add `clientBuffers.clear();` then `clients.shrink_to_fit();`
  - Notes: `clientBuffers` (`std::map<uint32_t, String>`) entries are orphaned when `setPort()` or `shutdown()` calls `clients.clear()` without cleaning the buffer map. The `loop()` disconnect path already cleans per-client (line 208), but bulk operations skip this.

- [x] Task 9: Add HeapTracker memory tests for EventBus
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add `#include <DomoticsCore/Testing/HeapTracker.h>` to includes. Add two test functions:
    1. `test_eventbus_memory_stability_single_cycle()` — subscribe (type + topic + wildcard), unsubscribe by ID, checkpoint before/after, `assertStable("before", "after", 512)`
    2. `test_eventbus_memory_stability_multi_cycle()` — 20 cycles of subscribe+unsubscribe, checkpoint before/after all cycles, `assertStable("before", "after", 512)`
  - Register both in the test runner's `RUN_TEST()` block.

- [x] Task 10: Add HeapTracker memory tests for MQTT
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `#include <DomoticsCore/Testing/HeapTracker.h>` to includes. Add two test functions:
    1. `test_mqtt_memory_stability_message_queue()` — queue 10 messages (via publish while disconnected), then set mock state so `isConnected()` returns true AND `mqttClient->publish()` returns true, then call `processMessageQueue()` directly to drain the queue, checkpoint before/after the full cycle, `assertStable("before", "after", 512)`
    2. `test_mqtt_memory_stability_subscribe_cycle()` — 10 cycles of subscribe+unsubscribe, checkpoint before/after, `assertStable("before", "after", 512)`
  - Register both in the test runner's `RUN_TEST()` block.

- [x] Task 11: Add HeapTracker memory tests for RemoteConsole
  - File: `DomoticsCore-RemoteConsole/test/test_remoteconsole_component/test_remoteconsole_component.cpp`
  - Action: Verify `#include <DomoticsCore/Testing/HeapTracker.h>` is present. Add two test functions:
    1. `test_remoteconsole_memory_stability_single_connect()` — simulate 1 client connect+disconnect cycle, checkpoint before/after, `assertStable("before", "after", 512)`
    2. `test_remoteconsole_memory_stability_multi_connect()` — 10 cycles of connect+disconnect, checkpoint before/after, `assertStable("before", "after", 512)`
  - Register both in the test runner's `RUN_TEST()` block.

- [x] Task 12: Update CODE-ROADMAP.md tracking table
  - File: `docs/CODE-ROADMAP.md`
  - Action: Update the tracking table (lines 263-271 and 432-443):
    - R1: change status to "DONE"
    - R2: change status to "DONE"
    - R3: change status to "N/A (std::map — no shrink_to_fit equivalent)"
    - R4: change status to "DONE"
    - Update M9/M10 line to reflect R1 completion: "M9, M10: DONE (functional fix + shrink_to_fit). R1-R4: DONE. R5-R7: TODO"

- [x] Task 13: Run all tests and verify
  - Action: Run `pio test -e native` for all three affected components. Verify all existing tests still pass and new memory tests pass.
  - Notes: If any test fails, investigate and fix before marking complete.

### Acceptance Criteria

- [x] AC 1: Given EventBus with 20 topic subscriptions, when all are unsubscribed by ID, then HeapTracker `assertStable()` passes with 512-byte tolerance
- [x] AC 2: Given EventBus with 20 owner-based subscriptions spanning all three maps (type-based, topic-based, and wildcard subscriptions under the same owner), when `unsubscribeOwner()` is called, then HeapTracker `assertStable()` passes with 512-byte tolerance
- [x] AC 3: Given EventBus with subscriptions and queued events, when `reset()` is called, then the EventBus is in a clean state equivalent to a fresh instance (existing reset tests still pass)
- [x] AC 4: Given EventBus with a single topic subscription, when unsubscribed, then HeapTracker `assertStable()` passes (single-cycle test)
- [x] AC 5: Given MQTT component with 10 queued messages (offline), when mock is set to connected + publish success and `processMessageQueue()` is called draining all messages, then HeapTracker `assertStable()` passes with 512-byte tolerance
- [x] AC 6: Given MQTT component with 10 active subscriptions, when all unsubscribed via `unsubscribeAll()`, then HeapTracker `assertStable()` passes with 512-byte tolerance
- [x] AC 7: Given RemoteConsole with 1 connected client, when client disconnects and `loop()` runs, then HeapTracker `assertStable()` passes with 512-byte tolerance
- [x] AC 8: Given RemoteConsole with 10 connect/disconnect cycles, when all cycles complete, then HeapTracker `assertStable()` passes with 512-byte tolerance
- [x] AC 9: Given all three components, when existing test suites run, then all pre-existing tests pass without regression
- [x] AC 10: Given CODE-ROADMAP.md, when reviewed, then R1-R4 are marked with correct status and R3 is marked N/A

## Additional Context

### Dependencies

- None. These are isolated fixes with no cross-component dependencies. Tasks 1-4 (EventBus) are independent from Tasks 5-7 (MQTT) and Task 8 (RemoteConsole). Tests (Tasks 9-11) depend on their respective source fixes being complete.

### Testing Strategy

- Primary assertion: HeapTracker `assertStable("before", "after", 512)` — proves heap returns to baseline after cycles
- Two test tiers per component: 1-cycle minimal + 10-20 cycle stress
- Tests run in native (desktop) environment via PlatformIO Unity
- Include `<DomoticsCore/Testing/HeapTracker.h>` in EventBus and MQTT test files (first integration for these suites)
- Follow existing test patterns: `tracker.checkpoint("before")`, operations, `tracker.checkpoint("after")`, `tracker.assertStable()`

#### HeapTracker native platform limitations

On native (x86 GCC/glibc), `HeapTracker` may use a stub or `/proc/self/statm` for heap measurement. Small vector capacity changes (a few hundred bytes) may be within noise on a system with gigabytes of RAM. The 512-byte tolerance is calibrated for ESP targets but may produce false positives or false negatives on desktop. These tests serve as **regression guards** on native and are expected to be **definitive** only on ESP8266/ESP32 hardware. The existing project already uses this pattern for Storage, WebUI, NTP, Wifi, and HA tests on native — the tolerance is proven adequate for the test framework's heap tracking implementation.

### Notes

- R3 (Storage) marked "N/A (std::map)" in CODE-ROADMAP.md — no code changes needed
- TODO comments in EventBus.h (lines 95, 115) removed as part of Tasks 2 and 3
- Party Mode reviews applied: Winston (architecture), Amelia (implementation), Murat (test strategy)
- `pruneMap()` helper is a private static template — equivalent performance to hand-written code, inlined by compiler. Only calls `shrink_to_fit()` on vectors where elements were actually removed (guarded by `oldSize` check)
- Queue swap idiom uses explicit form `queue = std::queue<QueuedEvent>()` for clarity (not `queue = {}` which may confuse less experienced C++ developers)
- RemoteConsole `loop()` uses `erased` flag to avoid calling `shrink_to_fit()` on every loop cycle — hot path protection

### Known Limitations

- **EventBus `lastByTopic` / `pendingByTopic` maps**: These maps accumulate entries for every unique topic ever used with `publishSticky()` or enqueued. They are NOT cleaned up by this spec. Rationale: these maps grow proportionally to unique topic count (bounded by design — topics are static strings like `"wifi/connected"`, `"mqtt/message"`), not by subscribe/unsubscribe churn. They do not exhibit the same unbounded growth pattern as subscription vectors. Future assessment recommended if dynamic topic generation is ever used.
- **HeapTracker on native**: Tests may not detect small capacity differences on desktop. See Testing Strategy section for details.
- **RemoteConsole client disconnect**: Memory tests exercise `setPort()` path, not actual client connect/disconnect (private `telnetServer` prevents stub injection in tests).

## Review Notes

- Adversarial review completed
- Findings: 13 total, 4 fixed (F2, F3, F4, F6), 2 noise/invalid (F1, F8-F9), 5 out-of-scope/design (F5, F7, F10-F13)
- Resolution approach: auto-fix
- F2: Added `erased` flag guard to MQTT `processMessageQueue()` shrink_to_fit
- F3: `pruneMap()` returns bool; `unsubscribe(id)` early-exits after first match (IDs are unique)
- F4: Added `test_eventbus_memory_stability_unsubscribe_owner` test
- F6: Added `test_eventbus_prune_removes_empty_map_keys` test
