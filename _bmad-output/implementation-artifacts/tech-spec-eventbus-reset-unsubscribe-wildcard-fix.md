---
title: 'EventBus reset and unsubscribe wildcard bug fixes'
slug: 'eventbus-reset-unsubscribe-wildcard-fix'
created: '2026-03-05'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++', 'PlatformIO', 'Unity test framework', 'Arduino (ESP32/ESP8266)']
files_to_modify: ['DomoticsCore-Core/include/DomoticsCore/EventBus.h', 'DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp', 'docs/CODE-ROADMAP.md']
code_patterns: ['erase-remove_if idiom for unsubscribe']
test_patterns: ['Unity framework', 'setUp/tearDown with new/delete EventBus', 'subscribe-publish-poll-assert pattern']
---

# Tech-Spec: EventBus reset and unsubscribe wildcard bug fixes

**Created:** 2026-03-05

## Overview

### Problem Statement

Two functional bugs in `EventBus` (DomoticsCore-Core) cause incorrect behavior after `reset()` and when unsubscribing wildcard subscriptions:

1. **M9 — `reset()` incomplete**: `reset()` clears `subscriptions`, `topicSubscriptions`, and `queue`, and resets `nextId` to 1, but does NOT clear `wildcardTopicSubscriptions`, `lastByTopic`, or `pendingByTopic`. After `reset()`, wildcard handlers still fire and sticky events replay stale data.

2. **M10 — `unsubscribe()` / `unsubscribeOwner()` skip wildcards**: Both methods iterate over `subscriptions` and `topicSubscriptions` but ignore `wildcardTopicSubscriptions`. Wildcard subscriptions can never be cleaned up, causing handler leaks and phantom event dispatches.

### Solution

- **M9**: Add `wildcardTopicSubscriptions.clear()`, `lastByTopic.clear()`, `pendingByTopic.clear()` to `reset()`, and add `assert(!dispatching_)` guard. After reset, the EventBus must be in the exact same state as a freshly constructed instance.
- **M10**: Extend `unsubscribe()` and `unsubscribeOwner()` to also scan and remove from `wildcardTopicSubscriptions`.

### Scope

**In Scope:**
- Fix `reset()` to clear the 3 missing internal state containers (M9)
- Add `assert(!dispatching_)` guard to `reset()` (M9)
- Fix `unsubscribe()` to scan `wildcardTopicSubscriptions` (M10)
- Fix `unsubscribeOwner()` to scan `wildcardTopicSubscriptions` (M10)
- Add tests covering all fixed behaviors
- Update CODE-ROADMAP.md status (PARTIAL — `shrink_to_fit()` deferred to R1)

**Out of Scope:**
- R1 (`shrink_to_fit()` after container operations) — separate commit
- EventBus API refactoring
- Cleanup of empty map entries after erase (deferred to R1)
- Early-exit optimization in `unsubscribe(id)` (deferred to R1)
- Any other roadmap items

## Context for Development

### Codebase Patterns

- EventBus uses `std::map<String, std::vector<Subscription>>` for all subscription maps
- Unsubscribe uses erase-remove_if idiom: `vec.erase(std::remove_if(...), vec.end())`
- Tests use Unity framework with `setUp`/`tearDown` creating fresh `EventBus` instances via `new`/`delete` (not via `reset()`)
- All tests follow subscribe -> publish -> poll -> assert pattern
- EventBus depends on `Platform_HAL.h` for `HAL::indexOf`, `HAL::substring`, `HAL::startsWith`, `HAL::endsWith` (used in wildcard matching). Tests use `Platform_Stub.h`.
- **`pendingByTopic` mechanism**: `enqueue()` increments counter, `poll()` decrements after dispatch. `subscribe()` with `replayLast=true` checks this counter — if pending > 0, sticky replay is skipped to avoid duplicate delivery. Tests that involve sticky events MUST call `poll()` to drain the queue before testing replay behavior, otherwise `pendingByTopic` will interfere.
- **Known pre-existing issues**:
  - `pendingByTopic` counter can become incorrect when backpressure drops events (in `enqueue()`, when queue >= 32, `queue.pop()` drops the oldest event but `pendingByTopic` is NOT decremented for the dropped event). This does not affect this fix (we clear the counter entirely in `reset()`) but is an adjacent bug for future work.
  - `if (!queue.empty())` check in `enqueue()` after `queue.push()` is always true — dead logic.

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-Core/include/DomoticsCore/EventBus.h` | EventBus implementation (header-only) — THE file to modify |
| `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp` | Existing EventBus tests (Unity framework) — add new tests here |
| `DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h` | Calls `unsubscribeOwner()` in `shutdownAll()` and `removeComponent()` — affected by M10 fix |
| `docs/CODE-ROADMAP.md` | Roadmap items M9, M10 — update status after fix |
| `docs/components/core/project-context.md` | Core conventions and pitfalls reference |

### Technical Decisions

- `reset()` must produce an EventBus identical to a freshly constructed one. Note: `reset()` already clears `subscriptions`, `topicSubscriptions`, `queue`, and resets `nextId = 1`. The fix adds the 3 missing clears: `wildcardTopicSubscriptions`, `lastByTopic`, `pendingByTopic`.
- `reset()` must have `assert(!dispatching_)` guard — calling reset() during dispatch is undefined behavior and should crash in debug builds, consistent with subscribe/unsubscribe guards. `assert()` is a no-op in release builds (`NDEBUG` defined). In release mode, calling `reset()` during dispatch would silently corrupt state — this is accepted as debug-only protection, consistent with the existing pattern for subscribe/unsubscribe.
- `dispatching_` is NOT explicitly reset in `reset()` because the assert guarantees `dispatching_ == false` at entry. Since `dispatching_` is already `false` when `reset()` executes, no explicit assignment is needed — the constructor-equivalent state is maintained.
- **`nextId` collision hazard**: After `reset()`, `nextId` returns to 1. If any external code holds a subscription ID from before reset, that old ID could collide with a new subscription's ID. Calling `unsubscribe(oldId)` could remove the wrong subscription. This is a known design limitation — callers should discard all subscription IDs after `reset()`. Not in scope for this fix.
- No `shrink_to_fit()` in this changeset (deferred to R1)
- Tests added to existing test file, not a new test file
- No `*this = EventBus()` refactoring — explicit member-by-member clear is safer on embedded (avoids move-assign surprises with captured handler references)
- Order of clears in `reset()` preserves existing code order (queue first, then maps, then counters) — no reordering needed since the function is single-threaded and ordering is cosmetic under current no-exception assumptions
- Add contract comment above `reset()` documenting the invariant
- Add maintenance comment above member declarations
- No cleanup of empty map entries after erase (consistent with existing code; deferred to R1). Note: empty wildcard entries cause unnecessary iterations in `poll()` dispatch since wildcards are checked on EVERY topic event — this is a known performance concern for R1.
- Fix uses same erase-remove_if idiom as existing code — no factorization into helper (YAGNI). Note: `unsubscribe(id)` scans all 3 maps unconditionally even though IDs are unique. An early-exit optimization is deferred to R1.
- `replayLast` parameter is silently ignored for wildcard subscriptions (pre-existing behavior, not in scope). This is a known design gap adjacent to the bugs being fixed.
- Subscriptions registered with `owner = nullptr` (the default) cannot be cleaned up via `unsubscribeOwner()` — only via `unsubscribe(id)` or `reset()`. This is a pre-existing design limitation, not in scope.

## Implementation Plan

### Tasks

**Phase RED — Write failing tests (TDD)**

New tests are added at the END of the existing test list in `main()`, in the order Tasks 1-7.

- [x] Task 1: Add `test_reset_clears_wildcard_subscriptions` to test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test function that subscribes to `"sensor.*"` wildcard, calls `reset()`, publishes `"sensor.temp"`, polls, and asserts handler was NOT called.
  - Register in `main()` with `RUN_TEST(test_reset_clears_wildcard_subscriptions)`.
  - **RED phase**: FAILS because `reset()` does not clear `wildcardTopicSubscriptions`.

- [x] Task 2: Add `test_reset_clears_sticky_events` to test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test that:
    1. Subscribes to `"sticky/topic"` with a counter handler
    2. Calls `publishSticky("sticky/topic", 123)` and `poll()` to drain the queue
    3. Asserts handler was called during poll (setup validation — confirms `pendingByTopic` is now 0)
    4. Calls `reset()`
    5. Calls `subscribe("sticky/topic", newHandler, nullptr, true)` — sticky replay happens inline in `subscribe()`, NOT in `poll()`
    6. Asserts newHandler was NOT called by the subscribe (no stale sticky replay)
  - Add comment in test: `// poll() before reset() is mandatory — without it, pendingByTopic > 0 would skip sticky replay even without the fix, making this test pass in RED phase`
  - Register in `main()` with `RUN_TEST(test_reset_clears_sticky_events)`.
  - **RED phase**: FAILS because `lastByTopic` is NOT cleared by `reset()`, so `subscribe()` with `replayLast=true` finds the stale sticky data and invokes the handler.

- [x] Task 3: Add `test_unsubscribe_wildcard_by_id` to test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test that subscribes to `"sensor.*"` wildcard, captures the subscription ID, calls `unsubscribe(id)`, publishes `"sensor.temp"`, polls, and asserts handler was NOT called.
  - Register in `main()` with `RUN_TEST(test_unsubscribe_wildcard_by_id)`.
  - **RED phase**: FAILS because `unsubscribe()` does not scan `wildcardTopicSubscriptions`.

- [x] Task 4: Add `test_unsubscribe_owner_clears_wildcards` to test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test that subscribes to `"sensor.*"` wildcard with a fake owner pointer (`(void*)0x5678`), calls `unsubscribeOwner(owner)`, publishes `"sensor.temp"`, polls, and asserts handler was NOT called.
  - Register in `main()` with `RUN_TEST(test_unsubscribe_owner_clears_wildcards)`.
  - **RED phase**: FAILS because `unsubscribeOwner()` does not scan `wildcardTopicSubscriptions`.

- [x] Task 5: Add `test_reset_clears_pending_counters` to test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test that:
    1. Calls `publishSticky("pending/topic", 42)` WITHOUT calling `poll()` — leaves `pendingByTopic["pending/topic"] = 1`
    2. Calls `reset()`
    3. Calls `publishSticky("pending/topic", 99)` and `poll()` to drain — `pendingByTopic` goes 0(if cleared)+1(enqueue)-1(poll) = 0
    4. Subscribes to `"pending/topic"` with `replayLast=true`
    5. Asserts handler IS called with value 99 (replay works because `pendingByTopic` is 0)
  - Register in `main()` with `RUN_TEST(test_reset_clears_pending_counters)`.
  - **RED phase**: FAILS because `pendingByTopic` is NOT cleared by `reset()`. Without clearing, the arithmetic is: 1(stale from step 1) + 1(enqueue in step 3) - 1(poll) = 1. Since pending > 0, subscribe skips replay. With the fix: 0(cleared) + 1(enqueue) - 1(poll) = 0, replay works.

- [x] Task 6: Add `test_reset_clears_queued_events` to test file (regression guard)
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test that:
    1. Subscribes to `"queued/topic"`, publishes `"queued/topic"` WITHOUT calling `poll()` (event sits in queue)
    2. Calls `reset()`
    3. Subscribes to `"queued/topic"` again with a new handler
    4. Calls `poll()` — asserts NEITHER the old NOR the new handler was called (queue was cleared)
  - Add comment: `// Regression guard — reset() already clears the queue. This test ensures it stays that way.`
  - Register in `main()` with `RUN_TEST(test_reset_clears_queued_events)`.
  - **Note**: This test PASSES before the fix — it is a regression guard, NOT a RED test.

- [x] Task 7: Add `test_reset_comprehensive` to test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add test that:
    1. Subscribes wildcard topic (`"wild.*"`) and calls `publishSticky("sticky/data", 42)` then `poll()` to drain
    2. Calls `reset()`
    3. Publishes `"wild.test"` and polls — asserts NO wildcard handler fires (RED assertion)
    4. Subscribes to `"sticky/data"` with `replayLast=true` — asserts NO sticky replay (RED assertion)
  - Register in `main()` with `RUN_TEST(test_reset_comprehensive)`.
  - **RED phase**: FAILS on steps 3 and 4.

- [x] Task 8: Run tests in RED phase
  - Command: `pio test -e native -f test_eventbus`
  - Action: Execute tests and verify the following:
    - Tests 1-5 and 7 FAIL (proving the bugs exist)
    - Test 6 PASSES (regression guard — queue clearing already works)
    - All existing tests still PASS
  - Notes: If unexpected results, investigate and fix tests before proceeding.

**Phase GREEN — Apply bug fixes**

- [x] Task 9: Fix M9 — Complete `reset()`
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: Replace the current `reset()` method with:
    ```cpp
    // Contract: reset() must leave the EventBus in the exact same state
    // as a freshly constructed instance. If you add new members, update this method.
    // Note: dispatching_ is not reset because the assert guarantees it is already false.
    void reset() {
        assert(!dispatching_ && "Cannot reset during EventBus dispatch");
        while (!queue.empty()) queue.pop();
        subscriptions.clear();
        topicSubscriptions.clear();
        wildcardTopicSubscriptions.clear();
        nextId = 1;
        lastByTopic.clear();
        pendingByTopic.clear();
    }
    ```
  - Notes: Preserves existing code order (queue first). Adds the 3 missing clears and the dispatch guard.

- [x] Task 10: Fix M10 — Add wildcard scanning to `unsubscribe(id)`
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: After the existing `topicSubscriptions` loop in `unsubscribe(uint32_t id)`, add:
    ```cpp
    for (auto& kv : wildcardTopicSubscriptions) {
        auto& vec = kv.second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const Subscription& s){ return s.id == id; }), vec.end());
    }
    // TODO(R1): early exit after first match — IDs are unique
    ```
  - Location: immediately after the closing `}` of the `topicSubscriptions` loop, before the method's closing `}`.

- [x] Task 11: Fix M10 — Add wildcard scanning to `unsubscribeOwner(owner)`
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: After the existing `topicSubscriptions` loop in `unsubscribeOwner(void* owner)`, add:
    ```cpp
    for (auto& kv : wildcardTopicSubscriptions) {
        auto& vec = kv.second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [owner](const Subscription& s){ return s.owner == owner; }), vec.end());
    }
    ```
  - Location: immediately after the closing `}` of the `topicSubscriptions` loop, before the method's closing `}`.

- [x] Task 12: Add maintenance comment above member declarations
  - File: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
  - Action: Add comment above the `subscriptions` member declaration (search for `std::map<EventType, std::vector<Subscription>> subscriptions;`):
    ```cpp
    // Internal state — if you add a new member, update reset() to clear it.
    ```
  - Location: use the member declaration as anchor, NOT absolute line numbers.

- [x] Task 13: Run ALL tests in GREEN phase
  - Command: `pio test -e native -f test_eventbus`
  - Action: Execute tests and verify ALL tests PASS (existing + new).

- [x] Task 14: Update CODE-ROADMAP.md
  - File: `docs/CODE-ROADMAP.md`
  - Action: In the Tracking table, change M9 and M10 status from `TODO` to `PARTIAL (functional fix done, shrink_to_fit deferred to R1)`.

### Acceptance Criteria

- [x] AC 1: Given an EventBus with active wildcard subscriptions, when `reset()` is called and a matching event is published, then NO wildcard handler fires.
- [x] AC 2: Given an EventBus with sticky events stored via `publishSticky()` and queue drained via `poll()`, when `reset()` is called and a new subscriber with `replayLast=true` subscribes to the same topic, then NO sticky replay occurs (handler is NOT called inline in `subscribe()`).
- [x] AC 3: Given an EventBus with a wildcard subscription, when `unsubscribe(id)` is called with the subscription's ID and a matching event is published, then the handler does NOT fire.
- [x] AC 4: Given an EventBus with a wildcard subscription owned by a component, when `unsubscribeOwner(owner)` is called and a matching event is published, then the handler does NOT fire.
- [x] AC 5: Given an EventBus with stale `pendingByTopic` counters from un-polled events, when `reset()` is called, then sticky replay via `replayLast=true` works correctly for new sticky events (counters cleared).
- [x] AC 6: Given the existing EventBus tests, when the fix is applied, then all existing tests continue to pass (no regression).
- [x] AC 7 (debug-only, code inspection): Given an EventBus in dispatch mode (`dispatching_` is true), when `reset()` is called, then `assert(!dispatching_)` fires. This cannot be tested in Unity (assert aborts process). Verified by code inspection. In release builds (`NDEBUG`), the assert is a no-op — consistent with existing subscribe/unsubscribe guards.

## Additional Context

### Dependencies

EventBus depends on `Platform_HAL.h` (for `HAL::indexOf`, `HAL::substring`, `HAL::startsWith`, `HAL::endsWith` used in wildcard matching). Tests use `Platform_Stub.h` which provides native implementations of these functions. No external library dependencies.

### Testing Strategy

**TDD Red-Green approach (mandatory for bug fixes):**

- **Phase RED**: Write all 7 tests FIRST, run them. Tests 1-5 and 7 must FAIL. Test 6 PASSES (regression guard — queue clearing already works, not a bug being fixed).
- **Phase GREEN**: Apply the fix to EventBus.h, run ALL tests (existing + new), verify they ALL PASS.

Add tests to `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
Run with `pio test -e native -f test_eventbus`

**Tests are purely black-box** — no internal state introspection. All verification through observable behavior.

**Critical test design note for Test 2 (sticky events):** The `publishSticky()` call enqueues an event which increments `pendingByTopic`. The sticky replay logic in `subscribe()` checks `pendingByTopic` and skips replay if pending > 0. Therefore, `poll()` MUST be called after `publishSticky()` and before `reset()` to drain the queue and decrement the counter. Without this poll, the test would pass even without the fix applied, breaking the TDD red-green contract. The test includes both a mandatory comment and a setup assertion (verify handler was called during poll via a pre-subscribed handler) to structurally protect this invariant.

**Test 5 RED-phase arithmetic**: Without the fix, `pendingByTopic` has a stale value of 1 from step 1. Step 3 enqueues again (+1=2), poll decrements (-1=1). Since pending=1 > 0, replay is skipped and the test FAILS. With the fix, reset clears the counter to 0. Step 3 enqueues (+1=1), poll decrements (-1=0). Since pending=0, replay works and the test PASSES.

### Impact Analysis

- **`reset()`**: Verified by codebase grep — not called by any production code. All `.reset()` calls in the codebase are on timer objects, UI state objects, or test utilities — none on `EventBus`. Zero regression risk.
- **`unsubscribeOwner()`**: Called by `ComponentRegistry::shutdownAll()` and `ComponentRegistry::removeComponent()`. Bug M10 is latent — no component currently subscribes to EventBus wildcards. But if any future component did, `unsubscribeOwner()` would leave phantom handlers that could trigger use-after-free on destroyed component pointers.
- **`unsubscribe(id)`**: Used in examples but not in production component code currently.
- **No component currently uses EventBus wildcards in production** — HomeAssistant uses MQTT wildcards (different mechanism). The fix is preventive but essential for safety.

### Notes

- EventBus is header-only, so changes only affect `EventBus.h`
- All dependent components (MQTT, HomeAssistant, etc.) are consumers, not modified
- No HeapTracker or stress tests needed in this scope (deferred to R1)
- No integration tests needed — EventBus is tested in isolation, with `setUp`/`tearDown` using `new`/`delete` for full test isolation
- Empty map entries after `unsubscribe()` cause unnecessary iterations in wildcard dispatch (wildcards are checked on EVERY topic event). `unsubscribe(id)` scans all 3 maps unconditionally even though IDs are unique — an early-exit optimization is deferred to R1.
- `replayLast` parameter is silently ignored for wildcard subscriptions — this is a pre-existing design gap, not in scope for this fix. Test authors should NOT write tests expecting `replayLast=true` to work with wildcard topics.
- Subscriptions registered with `owner = nullptr` cannot be cleaned up via `unsubscribeOwner()` — only via `unsubscribe(id)` or `reset()`. Pre-existing limitation, not in scope.
- `pendingByTopic` counter can become incorrect under backpressure (when queue >= 32, dropped events don't decrement the counter). Pre-existing bug — `reset()` clears the counter entirely, so it does not matter for the reset path.
- `nextId` collision hazard: After `reset()`, subscription IDs restart from 1. External code holding old IDs must discard them — calling `unsubscribe(oldId)` after reset could remove the wrong subscription.

## Review Notes
- Adversarial review completed
- Findings: 7 total, 2 fixed (F1: TODO markers, F5: cross-map test), 5 skipped (pre-existing/out-of-scope/by-design)
- Resolution approach: auto-fix for real in-scope findings
- Tests: 18/18 passed (10 existing + 8 new)
