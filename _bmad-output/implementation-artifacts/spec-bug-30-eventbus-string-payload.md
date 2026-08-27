# BUG-30 — `EventBus` topic payloads that are not trivially copyable

> **SUPERSEDED — do not implement from this document.** The BMad review of
> 2026-08-27 found the load-bearing premise false (two running native tests do
> subscribe to `storage/ready` and dereference the payload), the failure mechanism
> in section 2 wrong and inverted between native and ESP8266, the section 3
> measurements single-platform, the recommendation in section 5 unestablished, and
> the option space in section 4 incomplete. See
> `review-bug-30-eventbus-string-payload.md`. Sections below are kept as the
> record of what was proposed and refuted, not as guidance.

**Status**: design proposal, nothing implemented. Opened 2026-08-27 by the BUG-21
tests, which were the first code in the repository to subscribe to a topic OTA
publishes on.

**This document exists to be attacked before any code is written.** The
maintainer's stated concern is memory: String handling on the ESP8266 has cost
this project before, and Constitution XIV treats it as an absolute priority. The
option the author first recommended makes EventBus memory *worse*, and that was
not costed until it was challenged. Assume the rest of this needs the same
scrutiny.

---

## 1. The defect

`EventBus::publish(const String& topic, const PayloadT&)`
(`EventBus.h:121-129`) byte-copies the payload object:

```cpp
const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
qe.data.assign(p, p + sizeof(PayloadT));
```

BUG-1 added `static_assert(std::is_trivially_copyable<PayloadT>::value)` to the
sibling `publish(EventType, const PayloadT&)` twenty lines above. **The topic
overload never got it.** Neither did `publishSticky(const String&, const
PayloadT&)` (`EventBus.h:151-157`), which is worse: it stores the same byte copy
in `lastByTopic`, and `subscribe(..., replayLast=true)` hands that stored copy to
every late subscriber, indefinitely.

Given a `String` payload, the queue receives the *object's* bytes — on ESP8266 a
`char*`, a `uint16_t` capacity and a `uint16_t` length. The publisher's local
`String` is destroyed on return and its buffer freed. The pointer in the queue
then refers to freed heap.

### Measured, on the host

A subscriber registered with `core.on<String>("ota/start", …)` receives a
`String` reporting `length() == 114` whose characters are
`r\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd`. The length is correct — it was copied
out of the dead object — and the bytes are freed heap.

---

## 2. Call sites — the complete inventory

Found by adding the `static_assert` and compiling all 13 native projects plus
the three cross-compilation targets. Three sites, all publishing `String`:

| Site | Topic | Payload | Reachable? |
|---|---|---|---|
| `ComponentRegistry.h:128` | `system/ready` | `String("")` | Empty — always SSO. Never bites. |
| `Storage.h:149` | `storage/ready` | `storageConfig.namespace_name` | **Yes, by configuration.** See below. |
| `OTA.cpp` `publishStatusEvent()` | all six `ota/*` | serialized JSON, 129–199 bytes | **Yes, always.** |

`OTAComponent::broadcastProgress()` is a fourth instantiation but is dead code
(never called; recorded as DEAD-1 in the OTA technical reference).

### `storage/ready` is armed, not latent

The ESP8266 `String` SSO buffer holds **10 characters**
(`WString.h:316`: `SSOSIZE = sizeof(struct _ptr) + 4 - 1` = 11 bytes, "up to 11
(10 + \0)"). `StorageConfig::namespace_name` defaults to `"domotics"` — 8
characters, so it lands in SSO and the byte copy carries the characters with it.
**The field is validated at 15 characters** (`Storage.h:131`). Any namespace of
11 to 15 characters is heap-allocated, and `storage/ready` then publishes a
dangling pointer.

Nothing in this repository subscribes to `storage/ready`, so it has never fired.
That is luck plus absence of consumers, not a design that holds.

---

## 3. Measurements

ESP8266 (`nodemcuv2`, xtensa 32-bit), obtained from the compiler:

| | bytes |
|---|---|
| `sizeof(String)` | 12 |
| `sizeof(std::vector<uint8_t>)` | 12 |
| `sizeof(EventBus::QueuedEvent)` | 28 |
| String SSO threshold | 10 characters |

Actual OTA payload lengths, measured on the host by subscribing to each topic:

| Topic | payload |
|---|---|
| `ota/end` | 129 bytes |
| `ota/start` | 154 bytes |
| `ota/completed` | 199 bytes |

Queue cap: **32 entries** (`EventBus.h:238`), oldest dropped on overflow.

### The prior art that matters

STOR-ESP-1 measured **~122 bytes of heap per undrained event** on real hardware
(20 iterations, 2,448 bytes, reproducible to the byte). That figure is for
`storage/changed` and is dominated by per-entry overhead — the `QueuedEvent`
itself, the topic `String`, the `data` vector's allocation, and umm_malloc's
per-block cost — not by the 12-byte payload copy.

**So the baseline is already ~122 B per in-flight event**, and the payload is a
small part of it. Any option below should be judged as a delta on 122, not on 12.

---

## 4. Options

### A — `String` overloads on `publish`/`publishSticky`, plus the assert

```cpp
void publish(const String& topic, const String& payload) {
    publish(topic, payload.c_str(), payload.length() + 1);
}
void publishSticky(const String& topic, const String& payload) {
    publishSticky(topic, payload.c_str(), payload.length() + 1);
}
```

A non-template overload beats the template on an exact match, so every existing
`emit<String>` call site compiles unchanged and silently does the right thing.
The `static_assert` on the templates rejects every other non-trivially-copyable
type at compile time. Subscribers receive a null-terminated `const char*`.

- **Memory**: +129 to +199 bytes per in-flight OTA event, on a ~122 B baseline —
  roughly 2.1× to 2.6× per event. Worst case with a full queue of `ota/completed`:
  32 × ~320 B ≈ **10 KB**, against ~3.9 KB today.
- **Against it**: the call site says `String` and the subscriber gets `char*`.
  That asymmetry is invisible at the call site and must be documented.
- **For it**: the trap cannot be re-entered. Nobody has to know an idiom.

### B — assert only, explicit call sites

No overload. The three sites convert explicitly:

```cpp
emit(topic, payload.c_str(), payload.length() + 1, sticky);
```

- **Memory**: identical to A.
- **Against it**: the next person to write `emit<String>` gets
  "payload must be trivially copyable", which is true but does not say what to do
  instead.
- **For it**: no hidden conversion; the change is visible where it happens.

### C — drop the payloads, keep the topics

Publish all three without payload.

- **Memory**: strictly better than today — no `data` vector at all.
- **Against it**: throws away the OTA JSON, which is the only thing that would
  make these events useful to a UI. `ota/completed` carrying nothing is a
  notification, not an event.

### D — publish a POD struct, serialize at the point of use

Stop putting JSON on the bus. Define a trivially copyable status struct, the way
`MQTTMessageEvent` already does for `mqtt/message`:

```cpp
struct OTAStatusEvent {
    OTAComponent::State state;     // 1
    uint8_t  source;               // 1  enum: download | upload
    float    progress;             // 4
    uint32_t bytes;                // 4
    uint32_t total;                // 4
};                                 // ~16 bytes
```

Whoever needs JSON — the WebUI — builds it when it renders, not on every publish.

- **Memory**: ~16 bytes per event against 12 today, i.e. **+4 bytes** on a ~122 B
  baseline. Also removes a `serializeJson()` and a `String` construction from
  every publish, which is Constitution XIV's actual complaint.
- **Against it**: `lastError` and `lastResult` are text and do not fit a POD
  without fixed `char[]` fields, which is a size decision of its own. It is the
  largest change of the four, and it changes the payload of every OTA event.
- **For it**: it is what the constitution asks for, and it is the only option
  that does not make the memory picture worse.

---

## 5. What the author currently believes, and where it is weak

**Recommendation: D, with A's `static_assert` as the guard rail.** D is the only
option that improves memory rather than degrading it, and the JSON-per-publish it
removes is exactly the pattern MEM-2 exists to eliminate.

Weak points, offered up deliberately:

1. **The worst case in A may be a fiction.** 32 queued OTA events requires
   firmware that does not call `Core::loop()` — which is precisely the mistake
   that produced and then withdrew STOR-ESP-1. Realistic in-flight depth is 1–2.
   If that is right, A costs a few hundred bytes transiently and the 10 KB figure
   is scaremongering.
2. **D's text fields are unresolved.** `lastError`/`lastResult` are `String`
   today. Fixed `char[32]` truncates messages; leaving them out changes what
   subscribers can see. No position taken here.
3. **Nothing subscribes to any of these topics.** Every option is currently
   unobservable from outside. That argues for the cheapest correct fix, and
   against a large refactor justified by hypothetical consumers.
4. **The OTA reference documents these payload fields**, and BUG-21 just added
   two more rows to that table. Whatever is chosen has to be reflected there, and
   D changes what the table can honestly promise.
5. **`publishSticky` + `replayLast` may deserve separate treatment.** A stored
   dangling pointer replayed to every late subscriber is a different severity
   from a queued one that lives for one `poll()`. No option above treats them
   differently. Perhaps one should.

---

## 6. Verification the fix will need

- A native test that subscribes and reads the payload back after `poll()` and
  asserts the bytes are intact. Today's equivalent returns freed heap; it must
  fail against unmodified code.
- A `storage/ready` test with an 11-character namespace — the case that is armed
  and has never been exercised.
- Heap measurement on the `nodemcuv2` across a full OTA event sequence, with
  `Core::loop()` called as firmware calls it, and a second measurement with it
  deliberately withheld to bound the undrained case. Both against the same
  baseline, because STOR-ESP-1 is the standing proof that a hardware number can
  be reproducible to the byte and still measure the wrong thing.
- Whatever is chosen, ask what would still pass if the change were removed, and
  what a failure actually reached. Both halves of that lesson were learned the
  expensive way on 2026-08-26 and 2026-08-27.
