# BUG-30 v2 — the EventBus payload contract

**Status**: **SUPERSEDED on 2026-08-28 by `spec-bug-30-v3-eventbus-payload-contract.md`.**
Not refuted — overtaken. Its recommendation was contingent on a question nobody had
put to `marianorenzi`; reading his fork answered it. He does not need non-POD
payloads, he needs variable-length ones, and the overloads for that are already
merged here — his commit `b6660b78`. Seven of the sites below do not exist in his
rewrite. Nothing in this document should be implemented; it is kept for the option
space and the hardware measurements, which v3 cites.

*Original status: design proposal, reviewed and not yet ready to implement.
Replaced `spec-bug-30-eventbus-string-payload.md`, which the review of 2026-08-27
refuted.*

> **Adversarial review, 2026-08-27** — `review-bug-30-v2-adversarial.md`, 17
> findings. Four block implementation: §4's measurement was taken on option A/B,
> not on the recommended J; E and J cannot coexist through a single
> `void(const void*)` handler and E appears in no PR; the E precedent claimed at
> `ComponentRegistry.h:167-168` does not exist; and a root v3.0.0 protects nothing
> while every inter-component dependency is `>=`. **And this document's
> recommendation is contingent on §6.2, a question nobody has yet put to
> `marianorenzi` — which makes it a deferral rather than a decision.** Read the
> review before acting on anything below.

**Built by**: the architect (option space, mechanism, compatibility, landing),
the test architect (verification), and hardware measurement on a `nodemcuv2`.
Companions: `review-bug-30-eventbus-string-payload.md` (what killed v1),
`bug30-verification-plan.md` (section 8 in full).

**What changed from v1, in one line each.** The premise "nothing subscribes" was
false. The failure mechanism was backwards and inverts between native and target.
The inventory was a third of the real size and missed an entire component. The
measurements were modelled, single-platform, and wrong in both directions. The
recommendation's headline memory claim was false. The option space was less than
half of what exists.

---

## 1. The defect

`EventBus::publish(const String& topic, const PayloadT&)` (`EventBus.h:121-129`)
copies the payload's **object representation**:

```cpp
const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
qe.data.assign(p, p + sizeof(PayloadT));
```

BUG-1 added `static_assert(std::is_trivially_copyable<PayloadT>::value)` to the
sibling `publish(EventType, const PayloadT&)` twenty lines above. The topic
overload never got it, and neither did `publishSticky` (`EventBus.h:151-157`),
which additionally stores the copy in `lastByTopic` — replayed to every
`replayLast` subscriber, cleared only by `reset()`.

Three consequences, all load-bearing:

1. **No copy constructor runs and no destructor runs.** The queue holds a
   *carcass*: an object whose invariants were true at one instant, in another
   location. There is no double free, and there is no owned buffer either.
2. **The carcass is relocated** to `qe.data.data()`.
3. **`poll()` hands out `qe.data.data()` and destroys `qe` at the end of that
   loop iteration.** Any pointer a handler receives is valid only inside the
   handler. That is true today and under every option below.

Whether the carcass is usable depends on whether its internal pointers survive
both the relocation and the original's death.

---

## 2. The mechanism, per platform — v1 had this backwards

| | ESP8266 / ESP32 (`WString`) | native (`Platform_Stub.h:27` → `std::string`) |
|---|---|---|
| **Short string** (≤10 / ≤14 / ≤15) | `sso.buff` is an array **inside** the object; `buffer()` derives from `this`. Relocation harmless, death irrelevant. **Safe.** | libstdc++ stores `_M_p` pointing at the object's **own** `_M_local_buf`. The copy's pointer still refers to the *original's* storage. **Unsafe.** |
| **Long string** | `ptr.buff` is heap, freed by the original's destructor. **Unsafe.** | `_M_p` is heap, freed. **Unsafe.** |

v1 treated SSO as the safety condition. SSO **is** the safety condition on
target and the **unsafety** condition on the host that runs the 13 CI suites.

### The four reasons the sites behave as they do today

**Reason 1 — short enough for the target's SSO.** `system/ready`,
`shutdown/start` (`String("")`). Safe on ESP8266 and ESP32. Formally UB on
native, unobserved because both consumers take `const void*` unnamed.

**Reason 2 — the publisher's object outlives the poll.** `storage/ready`
publishes `storageConfig.namespace_name`, a **live member**. The two CI tests
that dereference it read live memory. **Lifetime, not SSO** — and it would still
pass with a 30-character namespace. v1's "an 11-character namespace arms it" was
wrong, and a test written from it would have passed green while proving nothing.

**Reason 3 — a pointer whose pointee outlives dispatch.** `component/ready`
publishes a `const char*`. Trivially copyable, so **no `static_assert` on any
overload can ever flag it.** It works because `metadata.name` is usually a
literal — but `Storage.h:110` assigns `nameStr_.c_str()`, a pointer into a
`String` member's buffer. This is why a guard can never be the whole answer.

**Reason 4 — the object is dead before `poll()` runs.** These are broken today.

### What is actually broken, on which target

| Site | ESP8266 | ESP32 | native |
|---|---|---|---|
| `component/ready` (pointer) | safe (literal) / fragile (`Storage.h:110`) | same | same |
| `system/ready`, `shutdown/start` | safe (SSO) | safe (SSO) | **UB, unobserved** |
| `storage/ready` | safe (live member) | safe | safe — **and read by two CI tests** |
| `network/ready` ×7 | **broken for IPs ≥11 chars** | broken for IPs ≥15 | **broken always** |
| `ota/*` ×6 (one sticky) | **broken always** | **broken always** | **broken always** |

---

## 3. The inventory — twelve String sites in four components

v1 said three sites in three components, found by adding the `static_assert` and
compiling. That method is structurally incapable of producing an inventory:

- **It reports one site per type per translation unit.** `emit<String>` is
  instantiated once for `T = String`; later sites reuse it silently. `Wifi.h` has
  seven and the compiler named one. `ComponentRegistry.h` has two and it named
  one. `OTA.cpp` has two and it named **only the dead one**, hiding the live site
  behind it.
- **It cannot see pointer payloads at all**, because they are trivially copyable.

**Grep is the inventory; the assert is the ratchet.** Corrected:

| # | Site | Topic | Payload | Class |
|---|---|---|---|---|
| 1 | `ComponentRegistry.h:121` | `component/ready` | `const char*` | pointer — assert-invisible |
| 2 | `ComponentRegistry.h:128` | `system/ready` | `String("")` | empty |
| 3 | `ComponentRegistry.h:167` | `shutdown/start` | `String("")` | empty |
| 4 | `Storage.h:149` | `storage/ready` | `namespace_name` | live member |
| 5–11 | `Wifi.h:141,221,234,268,740,799,834` | `network/ready` | `getAPIP()` / `getLocalIP()` | **temporary** |
| 12 | `OTA.cpp:733` | six `ota/*`, one sticky | JSON local, 129–199 B | **local, always heap** |
| 13 | `OTA.cpp:722` | `ota/progress` | JSON local | dead, still instantiated |

### `network/ready` is a live use-after-free, and it wants its own ID

`Wifi_HAL.h:84-85` returns `String` **by value**; `Wifi_ESP8266.h:64` is
`WiFi.softAPIP().toString()`. The temporary dies at the semicolon and `poll()`
runs later. The ESP8266 SoftAP default is **`192.168.4.1` — eleven characters**,
one past the 10-character SSO buffer, so the most ordinary AP address this
library produces is heap-backed and freed before dispatch. Most DHCP addresses
are 11–15 characters and behave the same.

It is documented as a public event in five places, and
`docs/components/wifi/project-context.md:123` states that MQTT *subscribes* to
it — no such subscriber exists in the tree, which is a second, separate doc
defect. **Propose BUG-32 [HIGH]**, filed and fixed in this lot.

`Wifi.h` is one of the fourteen files `marianorenzi` is rewriting. See §7.

---

## 4. Measurements

### ABI, from the compiler

| | ESP8266 | ESP32 | native (64-bit) |
|---|---|---|---|
| `sizeof(String)` | 12 | 16 | 40 |
| `sizeof(QueuedEvent)` | 28 | 32 | 72 |
| `sizeof(std::deque<QueuedEvent>)` | 40 | 40 | 80 |
| SSO capacity | 10 chars | 14 chars | 15 chars |

v1 published the ESP8266 column as if it were the analysis. CI cross-compiles
both ESP32 targets.

### Hardware, `nodemcuv2`, 2026-08-27

40 publishes (> the 32-entry cap) of a 154-byte payload on one topic, two cycles
per arm with cycle 1 unmeasured, one subscriber attached, occupancy floor
asserted, mid-window read through `ESP.getFreeHeap()` / `getMaxFreeBlockSize()`
rather than a third `HeapTracker` checkpoint. **Both arms ran against unmodified
`main`** — `publish(topic, const void*, size_t)` already exists, so the candidate
needed no code to measure.

| arm | held at cap | returned on drain | max-free-block cost |
|---|---|---|---|
| today (byte-copy of the object) | 2 304 B | 2 304 B | 16 B |
| candidate (copy the characters) | 6 400 B | 6 400 B | **1 104 B** |

`dispatched = 128` over four cycles = 32 per cycle: the cap was genuinely hit.

**Delta at the cap: 4.1 KB.** v1 modelled ~10 KB; the rebuilt verification plan
modelled 6.0–6.8 KB. Both were wrong and the measurement is neither. Derived and
labelled as derived, because a deque grows in node steps: 72 B/event today,
200 B/event under the candidate.

**Neither arm leaks** — both return everything on drain.

**Fragmentation is the figure v1 could not see**, having measured only free heap:
1 104 B of largest contiguous block against 16 B. That is the number that binds
during an OTA, which needs a large contiguous buffer.

### Cost model, ESP8266, per in-flight event

```
per event = QueuedEvent slot 28 B (inside a deque node)
          + topic String heap  (0 if ≤10 chars, else round8(len+1) + 8)
          + data vector heap   (0 if empty, else round8(n) + 8)

one-off   = first deque node 512 B + map array ~32 B ≈ 544 B, resident once the
            bus has ever queued anything, unchanged by every option — model it
            explicitly so nobody charges it to the fix (STOR-ESP-1's lesson)

lastByTopic, per sticky topic, permanent
          = map node ~48 B + key String heap + value vector heap
```

---

## 5. The option space — nine, not four

| | Subscriber gets | per event | `lastByTopic` perm. | Storage tests | flash Δ | `on<T>` hole |
|---|---|---|---|---|---|---|
| **today** | dead `String` | ~76 B | ~96 B | pass, by lifetime | — | open |
| **A** `String` overloads | `const char*` | ~260 B | ~280 B | **segfault** | +~50 B | **worse** |
| **B/I** assert / `= delete` | `const char*` | ~260 B | ~280 B | **segfault** | ~0 | open |
| **C/K** no payload | `nullptr` → **silent** | ~52 B | ~72 B | fail | smallest | closed (nothing to read) |
| **D/J** fixed POD | POD ref | ~124 B | ~144 B | rewrite, then real | likely −ve | **closed by construction** |
| **E** synchronous | live `String&` | **0** | n/a | pass | ~0 | closed |
| **F** type erasure | live object | ~276 B | ~280 B | pass unchanged | **+~1 KB** | **truly closed** |
| **G** size tag | + size check | +4–8 B | +4–8 B | — | +~100 B | runtime-caught |
| **H** `emitText`/`onText` | `(char*, len)` | ~260 B | ~280 B | rewrite | +~80 B | closed by separate verb |

Notes that decide between them:

- **A is dead.** `Core::on<T>` and `IComponent::on<T>` do an unguarded
  `static_cast<const T*>(p)`. After A, `on<String>("ota/start", …)` compiles and
  reinterprets **raw JSON** as a String object — `{"succ` becomes the buffer
  pointer. Worse than today, which at least reinterprets a real dead String.
- **C/K is not "empty payload".** `payloadPtr` is `nullptr` and both typed
  helpers guard with `if (payload)`, so typed subscribers **go silent**. v1
  rejected C on the wrong grounds; this is the real cost.
- **E is free on memory and wrong for OTA.** `publishStatusEvent()` runs inside
  the download loop and the HTTP upload callback — on ESP8266 that is the `cont`
  stack with ~4 KB headroom, inside an active `WiFiClient` read and an
  `Update.write()`. `OTA.cpp:331` already throttles "to avoid EventBus queue
  overflow": the queue is load-bearing there. E **is** right for the five
  main-loop lifecycle topics, and `ComponentRegistry.h:167-168` already does
  publish-then-poll, which is synchronous dispatch written the long way.
- **J costs more queue memory than the broken status quo** — a 64-byte struct
  against a 12-byte carcass whose 199-byte buffer the bus never owned. v1's "the
  only option that improves memory" was false and must not be repeated. What J
  removes is a `JsonDocument`, a `serializeJson()` and a `String` per publish —
  transient churn and flash, which is what MEM-2 actually complains about.
- **F is the only option that fixes the class** rather than thirteen sites, at
  ~1 KB of flash and a destructor that must run on four paths (dispatch, the
  overflow drop, `reset()`, sticky overwrite). Missing one is a leak no native
  test would see.

---

## 6. Recommendation

**J + I + E, with G as a runtime backstop** — and one question that can overturn
it, in §6.2.

1. **Fixed-size POD payloads at all thirteen sites** (J), in the events headers,
   not the component headers: `OTAState : uint8_t`, `OTASource : uint8_t` with an
   explicit `Unknown = 0`, explicit padding, `static_assert(sizeof(...) == 64)`,
   value-initialised at every publish so the copied bytes are deterministic.
   `char message[48]` carries `error`/`message`/`lastResult`.
2. **`= delete`d `String` and `std::string` overloads on `publish` and
   `publishSticky`**, plus the `static_assert` on both topic templates. A deleted
   overload beats the template and puts the diagnostic at the call site.
3. **The same guard on all four typed helpers** — `Core::on`, `Core::emit`,
   `IComponent::on`, `IComponent::emit`. Required, not optional: `emit<T>`
   instantiates **both** `publish` and `publishSticky` regardless of the `sticky`
   argument, so a guard only on EventBus points the error inside a header rather
   than at the offending line.
4. **Synchronous delivery for the five main-loop lifecycle topics** (E), where it
   is free and correct. First thing to drop under time pressure.
5. **A payload-size field checked by `on<T>`** (G) — the only mechanism that
   catches a wrong cast made through the raw `subscribe` API.

**How this answers the objection that killed A**: after J there is **no `String`
on the bus at all**, so `on<OTAStatusEvent>` is not tolerated, it is correct. The
typed helper stops being a trap because there is no non-trivially-copyable
payload left for it to misread. The guard still goes in, as a ratchet rather than
as the load-bearing safety property.

**The honest limit, to be documented rather than papered over**:
`subscribe(topic, [](const void* p){ *(const String*)p; })` compiles under every
option here and is wrong under every one of them. No `static_assert` reaches it.
G catches a size mismatch, not a same-size wrong type.

### 6.1 Attacks on this recommendation

1. J is **more** queue memory than today. Any commit message claiming otherwise
   repeats v1's error.
2. J does not fix the class. Only F does.
3. J truncates OTA error text at 47 characters, **silently**. There is no
   non-silent way to truncate inside a POD.
4. `char ip[16]` is IPv4-only. IPv6 needs 46. On an ethernet-capable rewrite that
   is not academic.
5. Five or six PRs across four components, for one HIGH row.
6. Nothing in this repository observes most of the fix. `network/ready` and all
   six `ota/*` have no consumer, and `TopicLog` is payload-blind.

### 6.2 The one question that changes the answer

**Does `marianorenzi`'s transport-neutral rewrite need the bus to carry non-POD
payloads?** If yes, J is thirteen local patches his rewrite will reopen, and F is
the change that makes his design possible — ~1 KB of flash to make the bus
type-safe for the contributor actively building on it. He is better placed to
answer than we are, and `network/ready` is the exact topic his rewrite is built
around.

Two lesser flips: **H wins** (not A — H has the separate verb and delivers the
length) if any downstream user consumes the OTA JSON off the bus. **K wins** if a
max-free-block measurement during a real OTA download shows J's ~124 B/event is
material — the payloads should then go away entirely rather than shrink.

---

## 7. Compatibility and landing

**No version bump inside the lot.** The series that contains it releases as
**v3.0.0**: J changes the payload of eight documented public topics and makes
`on<String>` a compile error. The "it was UB, so there is no correct downstream
code to break" argument holds for `ota/*` and for `network/ready` on ESP8266 —
and **fails for `storage/ready`**, where a downstream `on<String>` works
correctly today on all three platforms. That one topic makes it a major.

**No deprecated path, deliberately.** A one-release `[[deprecated]]` grace would
ship a release where `on<String>` still compiles and still misreads the payload —
known UB with the warning attached to the wrong person. A compile error is the
only signal that reaches someone who installs by version.

**The CHANGELOG must name** every affected topic with old and new payload; that
`on<String>` now fails to compile, with the replacement type per topic; that a
manual `static_cast<const String*>` inside a raw `subscribe` **compiles, runs and
is wrong**, with the grep to run; the 47-character truncation; and that
`network/ready` was a use-after-free on ESP8266 — a fix a user may want to pull
for its own sake.

**Landing**, one lot, one PR per component, each independently green. The order
is forced: the guard cannot land before the conversions, because it breaks four
components by design.

| PR | Component | Content |
|---|---|---|
| 1 | Core | Fix `pendingByTopic` not decremented on the overflow drop. **Prerequisite** — without it no sticky-replay test can pass and every later heap number is polluted. File as BUG-31. |
| 2 | Wifi | `NetworkReadyEvent`; seven sites. **Shape agreed with `marianorenzi` first.** File BUG-32 [HIGH]. |
| 3 | Storage | `StorageReadyEvent`; rewrite the two consumer tests. |
| 4 | Core | `component/ready` → `char name[32]`; `system/ready` and `shutdown/start` payload-less. File BUG-33 [MEDIUM]. |
| 5 | OTA | The POD; delete `broadcastProgress()`; rewrite the payload table and remove the BUG-30 warning; make `TopicLog` observe payloads. |
| 6 | Core | The ratchet: asserts, `= delete`, typed-helper guards, size tag, negative-compile test, `MockEventBus` guard. **BUG-30 → DONE.** |

**Roadmap**: 111 items, `0C, 8H, 28M, 33L`, 54 resolved. BUG-30 closes (8H → 7H);
BUG-32 opens and closes in-lot; BUG-31 and BUG-33 open as MEDIUM. Re-sum the rows
against the total after every edit. Leave the item-count reconciliation as found.

**Coordination owed before PR 2, not after.** The standing rule is that
`marianorenzi` is notified once every CRITICAL and HIGH is closed. **That rule
does not cover this case**: BUG-30 is one of the eight, BUG-32 lands in his file,
and BUG-29 already changed `MQTT_impl.h` under him without notice. Send him the
mechanism, the inventory, the §6.2 question, `NetworkReadyEvent`'s field list as
**his** to specify, the owed BUG-29 note, and the sequencing question — if his
branch is close, BUG-32 may be better fixed inside his rewrite and this lot skips
Wifi entirely.

---

## 8. Verification

Full plan in `bug30-verification-plan.md`. Its load-bearing decisions:

- **Two claims, two families.** *Shape* (the bytes are the characters, not the
  object) is testable everywhere. *Lifetime* (they survive the publisher's death
  **and its storage being reused**) is testable only where a temporary or local
  is published. `storage/ready` has a shape bug and will never have a lifetime
  bug; a test there must say so or it will be miscited as lifetime evidence.
- **Four-byte shape probe before the full comparison.** Under unfixed code the
  first bytes are a buffer pointer, 8-byte aligned, so any payload whose first
  character is not a multiple of 8 (`{` = 0x7B) discriminates in one byte. Four
  bytes is always in bounds — the unfixed vector is 12/16/40 bytes. And because
  Unity longjmps on the first failure, the probe aborts **before** the full
  dereference, so the red phase cannot become a segfault that takes the following
  tests with it.
- **Lifetime tests must scribble.** Publishing from a dead scope is not enough:
  freed storage often still holds the old bytes and the test then passes for the
  allocator's reasons. Reuse the storage in the same size class between publish
  and poll, or the test is not in the plan.
- **Occupancy floors everywhere**, and more than 32 events, because 20 never
  reaches the cap and bounded growth then reads as unbounded — STOR-ESP-1 exactly.
- **Instrument bias quantified first.** `HeapTracker::checkpoint()` samples before
  inserting its own node, and the thresholds are 64 B.
- **Negative-compile checks** with three controls: grep the diagnostic text (or a
  typo passes as success), positive controls (or a broken `-I` makes everything
  pass), and refuse an empty glob.
- **Red protocol**: land the tests first with the fix absent, run each one
  **individually**, record the actual failure message. A cascade red is not
  evidence — this project has already published one as though it were.

Deliberately not covered: ESP32 hardware (cross-compilation and the compile-time
size budget only); downstream sketches, which no test here can reach; the
wildcard and `replayLast` defects, which are pre-existing and want their own IDs.

---

## 9. What this document still does not know

1. **The §6.2 question**, which is a person's answer and not a measurement.
2. **Max-free-block during a real OTA download**, with WiFi and TLS active. §4's
   1 104 B is from a bare harness; the binding figure is on a device doing the
   real thing, and it is the number that would flip J to K.
3. **Flash deltas.** Every figure in §5's flash column is an estimate. `pio run
   -t size` on `esp8266dev` settles it and has not been run.
4. **Whether any downstream user consumes these payloads.** Unknowable from here;
   absence of complaints is not evidence on a registry library.
