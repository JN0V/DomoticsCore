# BMad review — BUG-30 design proposal

**Reviewed**: `spec-bug-30-eventbus-string-payload.md`, 2026-08-27, before any code
was written, at the maintainer's instruction.
**Lenses**: adversarial, edge-case hunter, verification gap — run in parallel and
blind to each other.
**Content class**: docs with a behavioural surface.

**Verdict: the spec is not sound enough to act on.** Its load-bearing premise is
false, its measurement table is single-platform, its account of the failure
mechanism is wrong, its recommended option is not established, and its option
space is incomplete. Findings below; the three verified against source by the
review author afterwards are marked ✔.

---

## The four that change the decision

### 1. ✔ "Nothing subscribes to any of these topics" is false

Two running native tests subscribe to `storage/ready` and dereference the payload
as a `String`:

- `DomoticsCore-Storage/test/test_storage_events/test_storage_events.cpp:64-69`
  — `storageReadyNamespace = *static_cast<const String*>(payload);`, asserted at
  `:86` with `TEST_ASSERT_EQUAL_STRING("test_events", ...)`
- the same shape at `:91-96`, asserted at `:108`

Both run in the native environment — `DomoticsCore-Storage/platformio.ini` ignores
only `test_heap_esp8266` — so both are in CI's 729 cases today.

This premise carried the spec's entire risk argument (section 5 point 3) and its
scoping of section 6. **Under options A or B those tests reinterpret ~12 bytes of
ASCII as a `std::string`-backed object, taking `_M_p` out of the characters
`test_eve` — a segfault in the native suite, not a clean failure.** Under option C
they fail on an empty namespace. Under option D, which defines a POD for OTA only,
the site is not addressed at all.

The spec costed no option against a consumer that exists.

### 2. ✔ The failure mechanism in section 2 is wrong, and inverted per platform

The spec says the two non-OTA sites are safe because their strings fit the SSO
buffer. That is the wrong reason on both platforms, in opposite directions.

- **Native** (`Platform_Stub.h:27`, wrapping `std::string`): libstdc++ stores
  `_M_p` pointing at the object's own `_M_local_buf`. A byte copy relocates the
  object but not the pointer, so **an SSO copy points into the destroyed
  original** — the empty `String("")` at `ComponentRegistry.h:128` dangles here,
  the platform the 13 suites actually run on.
- **ESP8266** (`WString.h:306`): `sso.buff` is an array inside the object and
  `buffer()` derives from `this`, so a relocated byte copy is self-consistent —
  **the SSO case is genuinely safe on target.**
- `storage/ready` passes today for a third reason entirely: it publishes
  `storageConfig.namespace_name`, a **live member** of a component still alive
  when `poll()` runs. Lifetime, not SSO.

So the spec's "armed by an 11-character namespace" reproduction would not
reproduce anything, and its one piece of evidence — `length()==114`, mojibake —
was taken on a String implementation that ships on no target.

### 3. ✔ The measurement table is ESP8266-only, presented as the analysis

`sizeof(String)` is 12 on ESP8266 (`_ptr` = `{char*, uint16_t, uint16_t}` = 8,
`SSOSIZE` = 11) but **16 on ESP32** (`_ptr` = `{char*, uint32_t, uint32_t}` = 12,
`SSOSIZE` = 15, so 14 characters). `sizeof(QueuedEvent)` follows: 28 vs 32. CI
cross-compiles `esp32dev` and `esp32c3`. Every delta in section 4 is wrong on two
of the three shipped targets.

### 4. Option A moves the trap rather than closing it

`Core::on<PayloadT>` (`Core.h:148-155`) and `IComponent::on<T>`
(`IComponent.h:212-219`) both do an unguarded `static_cast<const T*>(p)` with no
`static_assert`. After A, `core.on<String>("ota/start", …)` still compiles and now
reinterprets **raw JSON text** as a `String` object — the first bytes of
`{"success"…` become the buffer pointer. That is worse than today, which at least
reinterprets a real (dead) `String`. A is sold as "the trap cannot be re-entered";
it relocates the trap to a call form with no guard and a wilder failure mode.

---

## Memory analysis — section 3 does not hold

- **The queue is a `std::deque`**, not a list of independent allocations. Growth
  is a step function at fixed node boundaries, not a per-event slope, and popping
  does not return the last node. Both the "~122 B baseline" and the "10 KB worst
  case" are structurally wrong; the deque node term is invisible in the model and
  will show up in any hardware measurement as unexplained steps — attributed, as
  in STOR-ESP-1, to whatever was under test.
- **`lastByTopic` is uncosted.** `publishSticky` stores a second copy in a map
  that only `reset()` clears. `OTA.cpp:709` publishes `ota/completed` sticky.
  Today that is 12 bytes forever; under A or B it is ~199 bytes forever, plus a
  map node. Every option was scored as a transient in-flight delta. The one class
  of cost Constitution XIV treats as absolute is the one the analysis skipped.
- **Free heap is the wrong instrument.** On ESP8266 the binding constraint is the
  largest contiguous block. Churning 129–199 byte allocations fragments the heap
  in the exact window OTA needs a large contiguous buffer. An option neutral on
  free heap and destructive to max-free-block scores as neutral, then fails as an
  OTA that aborts with kilobytes free.
- **"Realistic in-flight depth is 1–2" is asserted, and contradicted in the file
  being modified.** `poll()` drains at most 8 per `Core::loop()` across all topics
  (`EventBus.h:174`, `ComponentRegistry.h:156`); `beginUpload()` publishes
  `EVENT_START` and `EVENT_INFO` back to back with no loop between
  (`OTA.cpp:230-239`); and `OTA.cpp:331` already carries the comment "Throttle
  progress broadcasts to avoid EventBus queue overflow" — someone previously found
  overflow to be real on this exact path.

---

## The option space is incomplete

Two options that dominate on the stated priority are absent and unrejected:

- **Synchronous dispatch** — a `publishNow(topic, payload)` invoking matching
  subscribers inline while the caller's object is alive. Zero queue entry, zero
  vector, zero copy, no lifetime bug by construction. OTA status is
  fire-and-forget UI notification, the shape that tolerates it. Costs: reentrancy
  (`dispatching_` forbids subscribe during dispatch) and loss of ordering against
  queued events. Neither is discussed.
- **Type-erased value semantics** — store a copy-construct/destroy pair beside the
  bytes so `QueuedEvent` owns a real object. Fixes the class rather than three
  sites. Costs: per-entry function pointers and code size on ESP8266.

The spec presents A–D as the space. A reader — the maintainer — approves believing
it was searched.

## Option D specifically

- `enum class State` has **no fixed underlying type**, so it is 4 bytes, not 1.
  With `float` alignment the struct is **20 bytes with 3 bytes of padding**, not
  "~16". The headline "+4 bytes" is wrong by a factor of two before any
  measurement, and the uninitialised padding is byte-copied — a source of
  intermittent red that reads like a cascade.
- **The WebUI does not subscribe.** `OTAWebUI.h:235-290` registers REST endpoints
  and the card is `withRealTime(2000)` — it polls. So `publishStatusEvent()`
  serializes JSON that nothing has ever read. D is justified by a consumer that
  already gets its data another way, and C should be re-argued against D on that
  fact.
- **`ota/info` and `ota/error` carry only text** (`message`, `error`). Under D as
  drafted they lose their only payload — D silently becomes C for two of the six
  topics, which the "against it" bullet does not say.
- `OTAStatusEvent` nests `OTAComponent::State`, forcing subscribers to include the
  whole component header, or to define the type twice and reproduce BUG-12's ODR
  violation in a second component.

---

## Verification plan — section 6 would not distinguish a working fix

- **The OTA tests are payload-blind by construction.** `TopicLog::watch`
  (`test_ota_component.cpp:425`) takes `const void*` unnamed and discards it;
  every assertion is `sawTopic`/`indexOf`. All four options are indistinguishable
  to the entire OTA suite. A fix that publishes the right topics with an empty or
  wrong payload is green.
- **Bullet 1 tests the queue path, never the sticky path.** Sticky replay is
  delivered inline from `subscribe()`, never through `poll()`
  (`test_eventbus.cpp:245` says so outright). No existing test reads back a sticky
  payload of any non-POD type. A fix that adds A's `publish` overload and omits the
  `publishSticky` twin passes everything in the tree while `ota/completed` keeps a
  dangling pointer handed to every late subscriber.
- **Bullet 2's test already exists and already passes.**
  `test_storage_events.cpp:73` uses `"test_events"` — 11 characters, the very case
  the spec calls "armed and never exercised". It is green against unmodified code,
  and no legal namespace (≤15, `Storage.h:131`) can exceed native SSO. Written as
  specified it would be recorded as proof and prove nothing.
- **The heap measurement repeats STOR-ESP-1's shape.** No occupancy floor; too few
  events to pass the 32-entry cap, so bounded growth is again indistinguishable
  from unbounded; a shared-baseline three-checkpoint layout that `HeapTracker`
  corrupts by charging its own node to the following window
  (`HeapTracker.h:96-101`); and no harness on that board that publishes OTA events
  at all — `test_ota_esp8266.cpp` constructs a bare `OTAComponent` with no `Core`,
  so `emit()` is a no-op. `test_heap_esp8266.cpp` already encodes every control
  this omits: floors at `:343`/`:351`, `HALF = 40` because "40 > 32", the
  two-cycle differential at `:385-398`.
- **No negative-compile test** proves the `static_assert` fires. And the live call
  sites use the explicit form `emit<String>(...)`, which forces
  `IComponent::emit<String>` and relies on overload resolution one level down
  inside that template — untested.
- **✔ The inventory method is structurally incomplete.**
  `std::is_trivially_copyable` accepts raw pointers, so the assert cannot see
  `ComponentRegistry.h:121`, which publishes `const char* namePtr =
  component->metadata.name` and is read back and dereferenced at
  `test_lifecycle_events.cpp:142-151`. A template is also only instantiated when
  ODR-used, so dead code, untaken `#if` branches, and examples other than
  FullStack are invisible — and `tests/mocks/MockEventBus.h:40` has its own `emit`
  template that would never receive the guard, so mock-based tests prove nothing
  about the fix.

---

## Process

- **No landing plan.** Every option spans Core, OTA, Storage, ComponentRegistry
  and the OTA reference. CLAUDE.md requires one lot per PR grouped by component,
  roadmap rows updated, the tracking summary re-checked, no version bumps inside
  the lot.
- **No coordination step.** `marianorenzi` is rewriting the network layer over
  fourteen overlapping files that all publish through the EventBus, and BUG-29
  already changed `MQTT_impl.h` under him without notice. A payload-contract
  change to the class every component publishes through must be raised before it
  merges, not after.
- **No compatibility position.** Payload shapes are public API on a registry
  library installed by version. A changes semantics silently with no compile
  error; B hard-breaks any downstream sketch publishing a `String`; D changes all
  six OTA topics. The spec says nothing about SemVer, CHANGELOG, deprecation, or
  a migration note.

---

## Also raised, not decision-critical

- `publish(topic, ptr, size)` returns silently on `payloadSize == 0`, so an empty
  payload suppresses the whole event.
- Under C, `payloadPtr` is `nullptr` and both typed helpers guard with
  `if (payload)` — typed subscribers go **silent**, not empty-handed. C was
  rejected on the wrong grounds.
- A `const char*` payload delivered by A/B is valid only for the callback's
  duration; a handler that stores it reintroduces the defect. Needs documenting.
- NUL-terminated delivery truncates a payload containing an embedded NUL; deliver
  the length instead.
- Wildcard subscribers (`storage/*`) receive payloads of different types with no
  size or type tag.
- `replayLast` is silently ignored on wildcard subscriptions.
- The `pendingByTopic` overflow leak already recorded in `deferred-work.md` means
  replay is permanently suppressed for any topic that has overflowed — which both
  lowers the severity of the stored dangling pointer and means A's ~199 bytes
  would be retained permanently while unreachable. Decide the ordering of the two
  fixes.
- Dead `broadcastProgress()` is a fourth instantiation; it must be deleted or the
  guard breaks the build.
- `-fno-exceptions` on ESP builds: a failed 129–199 byte allocation during OTA
  aborts rather than throwing.
- D's `source` field has no `Unknown` value, so transition-driven errors that set
  no source would encode as `download`.
- D has no version field, so a consumer compiled against a different library
  version reads past the buffer.

---

## What the review says to do next

Not "pick an option". The spec has to be rebuilt on:

1. the real consumer list, starting with the two Storage tests;
2. per-platform measurements (esp8266 / esp32 / native), with max-free-block
   alongside free heap, and the deque node cost modelled;
3. the correct failure mechanism — lifetime, not SSO — and a target-side
   observation of the bug, since the only evidence so far is from a stub;
4. six options, not four;
5. a verification plan whose tests can fail, checked against
   `test_heap_esp8266.cpp`'s existing controls.
