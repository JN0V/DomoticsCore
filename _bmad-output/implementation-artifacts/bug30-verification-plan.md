# BUG-30 — verification plan

Companion to `spec-bug-30-v2-eventbus-payload-contract.md` §8. Replaces section 6
of the superseded v1, which the review found unable to distinguish a working fix
from a broken one.

---

## 0. What this plan is built on

### 0.1 Two claims, two families

| Claim | Meaning | Testable where |
|---|---|---|
| **Shape** | the bytes a subscriber receives are the payload's *characters*, not the payload *object* | every site, every platform, regardless of the source's lifetime |
| **Lifetime** | those bytes are still correct after the publisher's object is destroyed **and its storage reused** | only where a temporary or local is published |

`storage/ready` publishes a live member. It has a shape bug and will never have a
lifetime bug. Its tests assert shape and **say so in a comment**, so nobody
later cites them as lifetime evidence — which is the trap v1's "11-character
namespace" bullet fell into.

### 0.2 The two assertion primitives, in this order

**P1 — the four-byte shape probe.** Compare `static_cast<const char*>(payload)[0..3]`
against the first four characters of the published text.

- *Always in bounds.* Unfixed, the queued vector is `sizeof(String)`: 12 (ESP8266),
  16 (ESP32), 40 (native). Four bytes never overreads.
- *Decisive.* Unfixed and heap-backed, offset 0 is the low byte of a pointer;
  umm_malloc rounds to 8 and glibc to 16, both little-endian, so byte 0 is a
  multiple of 8. **Test payloads must therefore start with a character whose
  ASCII code is not a multiple of 8** — `{` 0x7B, `t` 0x74, `B` 0x42, `s` 0x73.
- *Payloads must exceed 14 characters* so they are heap-backed on both targets;
  an SSO String stores its characters at offset 0 and would pass P1 unfixed.

**P2 — the full comparison**, only after P1 passed.

Order matters. Unity longjmps on the first failure, so unfixed code aborts at P1
before P2 can dereference a dangling pointer. **That is what stops the red phase
becoming a segfault that takes the following tests down with it** — the cascade
lesson, applied preventively.

### 0.3 The lifetime harness

Publishing from a dead scope is not sufficient: freed storage often still holds
the old bytes, and the test then passes for the allocator's reasons.

```cpp
static void publishFromDeadScope(EventBus& bus, const char* topic, const char* text) {
    String tmp(text);            // >14 chars => heap-backed on every target
    bus.publish(topic, tmp);
}                                // tmp destroyed, buffer freed

static void scribble(size_t len) {
    for (int i = 0; i < 16; ++i) {
        String junk; junk.reserve(len + 8);
        for (size_t j = 0; j < len; ++j) junk += 'Z';   // 0x5A, also not 8-aligned
    }
    volatile char pad[256];
    for (int i = 0; i < 256; ++i) pad[i] = 'Z';         // and the stack, for native
    (void)pad;
}
// only then: poll() / subscribe(replayLast = true)
```

**A lifetime test without step 2 is not in this plan, and one proposed later
should be rejected on sight.**

---

## 1. Test inventory

`[NEW]` new · `[EDIT]` existing changes · `[HW]` hardware · `[CG]` compile guard

### 1.1 The queue path

- **A1 `[NEW]`** `test_eventbus.cpp :: test_publish_delivers_payload_characters_not_the_object`
  — 25-char text, live temporary, `poll()`, floor (`called`), P1, P2. P1 and P2
  run **inside** the callback; the pointer is only valid there.
- **A2 `[NEW]`** `… :: test_publish_payload_survives_publisher_destruction` — the
  §0.3 harness, non-sticky.
- **A3 `[NEW]`** `… :: test_publish_payload_survives_a_full_queue_and_late_drain`
  — 40 payloads from dead scopes (>32 cap), scribble, drain; assert exactly 32
  received and that the first and last survivors pass P1+P2. Covers the
  drop-oldest path nothing else reaches.

### 1.2 Sticky + `replayLast` — the path a partial fix skips entirely

Replay is delivered **inline from `subscribe()`**, never through `poll()`
(`test_eventbus.cpp:245` says so).

- **A4 `[NEW]`** `… :: test_sticky_payload_replayed_to_a_late_subscriber_is_intact`
  — publish sticky from a dead scope, **then `poll()`** (mandatory: otherwise
  `pendingByTopic` suppresses the replay at `EventBus.h:66` and the test passes
  vacuously), scribble, subscribe with `replayLast=true`. Floor
  `replays == 1` **then** P1, P2. Without the floor, option C passes this test.
- **A5 `[NEW]`** `… :: test_sticky_payload_survives_repeated_late_subscribers` —
  three late subscribers with a scribble between each. A fix that copies at
  publish time but hands out a reference passes A4 and fails here.
- **A6 `[NEW]`** `… :: test_sticky_payload_replays_after_the_queue_dropped_it` —
  characterises the `pendingByTopic` overflow leak. **Passes before and after**;
  written as an assertion of current behaviour with the deferred item named, so
  fixing the leak turns it red deliberately. **Not counted as coverage.**

### 1.3 The publisher call sites

- **A7/A8 `[EDIT]`** `test_lifecycle_events.cpp` — `system/ready` (:159) and
  `shutdown/start` (:178). Add
  `TEST_ASSERT_NULL_MESSAGE(payload, "still carries a byte-copied String")`.
  Null rather than P1: `""` has no first character, and byte 0 of a `String("")`
  can legitimately be 0x00. Carry a comment that `on<T>` stops firing once the
  payload is null, because both helpers guard with `if (payload)`.
- **A9/A10 `[EDIT]`** `test_storage_events.cpp:62-87` and `:89-109`. Today's
  `*static_cast<const String*>(payload)` (`:67`, `:94`) would segfault the native
  suite under A/B. Convert to P1 + `String(static_cast<const char*>(payload))`,
  namespaces `"svc_events_abc"` / `"cfg_store_prod"` (14 chars, `s`/`c` not
  8-aligned). **Comment must state: shape test, not lifetime.**
- **A11 `[NEW]`** `… :: test_storage_ready_payload_is_not_the_string_object` —
  `TEST_ASSERT_NOT_EQUAL(0, payload[0] & 0x07)`. States the defect in its own
  terms and survives someone rewriting A9's expected string.
- **A12 `[NEW/CG]`** `broadcastProgress()` is dead and cannot be tested. **Delete
  it**; the build then proves it is gone. The roadmap row must say which was done.
- **A16 `[NEW][HW]`** `test_ota_esp8266.cpp :: test_error_event_payload_is_json_on_silicon`
  — the on-device suites build a bare `OTAComponent` with **no `Core`**, so
  `emit()` is a no-op and they observe nothing. Add an `attachOta()` helper,
  drive `beginUpload(16, "")` → "Firmware hash required" → `EVENT_ERROR` (no
  flash, no network), `core.loop()` ×10, floor + P1. **The only test in the plan
  that exercises the real `emit<String>` site, with the real Arduino `String`, on
  real silicon.** Everything else about the OTA payload is a host claim.

### 1.4 The payload-blind OTA suite

- **A13 `[EDIT]`** `test_ota_component.cpp :: TopicLog` (:420-443) — record
  `bool hadPayload` and a **fixed 8-byte** head per event. Fixed, never
  length-driven: unfixed the source vector is 12/16/40 bytes, so 8 is always in
  bounds. Remove the header comment at :412-415, which documents the defect as a
  constraint and becomes false after the fix.
- **A14 `[EDIT]`** `test_ota_download_emits_start_then_end_then_completed` — keep
  the topic/order assertions, add `hadPayload` and `headIs(topic, "{\"su")` for
  `ota/start`, `ota/end`, `ota/completed`.
- **A15 `[NEW]`** `test_ota_completed_sticky_payload_replays_to_a_late_subscriber`
  — `OTA.cpp:709` publishes sticky and **no test in the tree touches it**. Floor
  `replays == 1`, P1, then `deserializeJson` and `TEST_ASSERT_TRUE(doc["success"])`.

### 1.5 The explicit-template-argument form

The live sites are `emit<String>(...)`, which forces `IComponent::emit<String>`
and relies on resolution one level down inside that template.

- **A17 `[NEW]`** a minimal `IComponent` subclass emitting a >14-char local.
- **A18 `[NEW]`** the same with `sticky = true` plus a late `replayLast`
  subscriber — `publishStatusEvent` reaches `publishSticky` only this way.
- **A19 `[NEW]`** `Core::emit` is a separate template and a separate resolution
  site; both deduced and explicit forms.

### 1.6 Negative-compile checks

No such infrastructure exists in the repo today. `tools/compile_guard.sh` plus
`DomoticsCore-Core/test/compile_guard/`, run in the `native-tests` CI job
(it needs `g++`, not PlatformIO):

| File | Expect | Guards |
|---|---|---|
| `must_fail_publish_nontrivial.cpp` | reject | topic template |
| `must_fail_publishsticky_nontrivial.cpp` | reject | **separate file — a guard on `publish` and not its sticky twin is exactly the partial fix predicted** |
| `must_fail_core_emit_nontrivial.cpp` | reject | `Core::emit` |
| `must_fail_icomponent_emit_nontrivial.cpp` | reject | `IComponent::emit` |
| `must_fail_core_on_string.cpp` | reject | `Core::on<String>` |
| `must_fail_icomponent_on_string.cpp` | reject | `IComponent::on<String>` |
| `must_pass_pod_payload.cpp` | compile | positive control |
| `must_pass_on_pod.cpp` | compile | `on<int>`, `on<MQTTPublishEvent>` still work |

Three controls, each earning its place:

1. **grep the diagnostic text** — otherwise a typo or a bad include makes the file
   fail to compile and the check reports success.
2. **positive controls** — otherwise a broken `-I` makes everything fail and every
   `must_fail` pass.
3. **refuse an empty glob**, as `ci.yml`'s native-tests job already does.

Verified safe against the tree: every in-tree `on<T>` uses `int`, `bool`,
`MQTTPublishEvent`, `MQTTSubscribeEvent`, `MQTTMessageEvent` or
`StorageChangedEvent`, all trivially copyable.

---

## 2. What would still pass without the fix

The rule: **a row with an empty "still passes" column is not a test.**

| # | Fails without the fix because | Still passes |
|---|---|---|
| A1–A3 | P1 sees a pointer's low bytes; the payload's first char is not 8-aligned | the floors; A3's `32` count (backpressure is unrelated) |
| A4, A5 | P1 on the replayed bytes | **the floor `replays == 1`** — replay works today, it replays *wrong* bytes. This is why the floor cannot be the only assertion |
| A6 | **nothing — passes before and after** | everything. Characterisation, not evidence |
| A7, A8 | payload is non-null today; the fix makes it null | the `received` flags |
| A9–A11 | today's byte 0 is a pointer LSB | `storageReadyReceived`, and the *unedited* test — so the edit must be re-verified, not assumed |
| A13–A15 | `headIs(topic, "{\"su")` | **every existing assertion**: `sawTopic`, ordering, `hadPayload`. Precisely the review's complaint |
| A16 | P1 on silicon at the real site | the floor and every other test in the suite |
| A17–A19 | P1 after the scribble | the floors |
| CG must_fail | they compile today — there is no guard at all | the `must_pass` rows, before and after; controls, not evidence |

### 2.1 Red protocol

Reasoning about byte layout is not evidence.

1. Land the tests **first**, fix absent, on a branch.
2. Run each new/edited test **individually**, never as a suite. A cascade red is
   what this project has already published as though it were a demonstration.
3. Record the actual failure message per test. One that does not name
   expected-vs-actual bytes is not usable.
4. `rm -rf .pio` before every run whose result will be recorded.
5. Apply the fix; re-run; check that the **only** things that changed are on the
   list. A test that flipped green and was not on it is a finding.
6. Identify the board before every hardware run — both are FTDI and `ttyUSB0` is
   whichever was plugged in first.

---

## 3. The hardware measurement

### 3.1 It can run before any code is written

`publish(topic, const void*, size_t)` already exists, so both arms are expressible
against unmodified `main`:

```cpp
bus.publish(topic, s);                          // arm: today
bus.publish(topic, s.c_str(), s.length() + 1);  // arm: candidate (A and B are identical here)
```

**This was done on 2026-08-27**; results are in the spec §4. The plan below is
what a *committed* suite must contain, beyond that one-off probe.

### 3.2 Controls reused verbatim from `test_heap_esp8266.cpp`

| Control | Source | Why here |
|---|---|---|
| occupancy floor before any delta assertion | `:343`, `:351`, `:427` | if events stop being queued, both arms collapse to zero and every comparison passes while measuring nothing |
| **more than 32 events** | `:317` (`HALF = 40`, "40 > 32") | 20 never reaches the ceiling; bounded growth then reads as unbounded. The single control v1 omitted and the single reason STOR-ESP-1 was filed |
| two-cycle differential, cycle 1 unmeasured | `:385-398` | one-time costs (deque nodes, the `pendingByTopic` entry, first-touch) are paid by cycle 1 |
| mid-window read via `ESP.getFreeHeap()`, not a third checkpoint | `:396`, `:407` | `HeapTracker::checkpoint()` samples **before** inserting its own node |
| warm-up outside the window | `:85-88` | first-touch is not a leak |
| prove the operation happened before measuring it | `:82-83` | here: the subscriber received events |
| start on a drained queue | `:314-315` | otherwise the measured half frees the setup's events |
| figures carried in the assertion message | `:125-135` | PlatformIO filters serial to Unity lines |

### 3.3 Controls this measurement adds

- **Instrument bias quantified first.** Two adjacent checkpoints with nothing
  between them; report the delta and assert every later threshold exceeds it.
  Keep all checkpoint names ≤10 characters so the name String stays in SSO and
  adds no allocation. Assert `hasCheckpoint()` before every `getDelta()` — an
  unknown name returns a zeroed snapshot and reports a multi-kilobyte leak for a
  typo.
- **Both instruments, always**: free heap **and** largest free block, from the
  same snapshot. An option neutral on free heap and destructive to max-free-block
  scores as neutral, then fails as an OTA that aborts with kilobytes free.
- **No per-event division.** The queue is a deque and grows in node steps.
  Report cap-to-cap occupancy per arm and compare arms; any per-event figure is
  derived arithmetic and labelled as such.

### 3.4 The measurements

1. **queue occupancy, today vs candidate** — floors on both arms, residue after
   drain ≤64 B on both, and the arm-to-arm delta against a budget **written into
   the spec before the run**. If it lands outside, the model is wrong and the
   option analysis must be rebuilt before the number is used. (The 2026-08-27
   probe measured 4.1 KB, against models of 6.0–6.8 KB and ~10 KB. Both models
   were wrong; this control is why that was caught.)
2. **sticky retention** — all six `ota/*` published sticky, drained, then measure
   what `lastByTopic` still holds. The cost v1 never costed at all.
3. **max-free-block under load** — fill to the cap, then attempt `malloc(4096)`
   (an OTA-sized buffer) and assert it succeeds; 20 churning cycles. **The only
   test that can veto A/B on memory grounds.**
4. **undrained plateau** — the STOR-ESP-1 control applied to the candidate.
5. **drain reclaims** — held ≥1000 B, residue ≤64 B on a second identical cycle.

Absolute free heap is **not** asserted: this harness has no WiFi, TLS or
filesystem, so its absolutes are not representative. **The delta is the decision
input; the absolute is context.**

---

## 4. Deliberately not covered

1. **No ESP32 or ESP32-C3 hardware run.** Coverage is cross-compilation on both
   targets plus a compile-time size budget. Acceptable because no decision turns
   on an ESP32 heap figure; if one comes to, this gap reopens.
2. **No network, no real firmware image.** Payload correctness does not depend on
   transport.
3. **The WebUI is not tested against the new payloads** — it polls and has never
   subscribed. That is an argument for the spec's C-vs-J comparison, not a gap.
4. **`tests/mocks/MockEventBus.h:40`** has its own `emit` template no guard would
   reach. Verified unreferenced: nothing includes it. The remedy is deletion, not
   a test — a mock nobody uses that models the defect is a trap.
5. **The `pendingByTopic` leak is not fixed here**; A6 characterises it and A4/A15
   route around it. Ordering is a spec decision.
6. **Downstream sketches are unreachable from here.** Mitigation is the CHANGELOG,
   not a test.
7. **Wildcard subscribers with heterogeneous payloads**, and `replayLast` silently
   ignored on wildcards. Pre-existing, unchanged by any option, want their own IDs.
8. **No mutation testing beyond §2.1's manual protocol.** Proportionate to what
   this repo has used before — and the one part whose quality depends on
   discipline rather than code, which is worth saying because it is the part most
   likely to be skipped.
