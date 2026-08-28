# BUG-30 v3 — the EventBus payload contract

**Status**: design proposal. Replaces `spec-bug-30-v2-eventbus-payload-contract.md`,
which is superseded — not refuted this time, **overtaken**.
**Date**: 2026-08-28

> **What overtook it.** v2's recommendation was contingent on a question nobody had
> put to `marianorenzi`: does his transport-neutral rewrite need the bus to carry
> non-POD payloads? The maintainer's answer on 2026-08-28 was that he could not
> answer it and that Mariano would adapt — and then said to look at the fork first.
>
> **The fork answers it, and the answer changes the shape of the fix.** He does not
> need non-POD payloads; he needs *variable-length* ones, which is a different
> thing, and he already wrote the mechanism. It is already merged here. Seven of
> the sites v2 planned to convert do not exist in his branch. The correct lot is a
> fraction of what v2 proposed, with no public contract break and no major version.

---

## 1. The defect, unchanged

`EventBus::publish(const String& topic, const PayloadT& payload)` byte-copies its
argument into a queue that dispatches after the publisher's local has gone. BUG-1
added `static_assert(std::is_trivially_copyable<PayloadT>::value, …)` to the
sibling `publish(EventType, …)` overload at `EventBus.h:104` and **not** to the
topic overload twenty lines below. So `emit<String>` compiles, and a `String`
reaches subscribers as a pointer into freed heap.

**It is latent.** Nothing in the repository subscribes with `on<String>`; the only
two matches for that spelling are comments warning about it. A subscriber taking
`const void*` and ignoring the payload — which is what every test does — never
dereferences the corpse.

---

## 2. What the fork settles

### 2.1 The mechanism exists, he wrote it, and it is already ours

`marianorenzi/DomoticsCore@b6660b78`, *"feat(eventbus): publish overloads for
variable-length payloads"*, **is in this repository's history**. `EventBus.h:133`:

```cpp
// Topic-based publish with a variable-length payload copy.
// The caller retains ownership; the queued event owns its byte copy.
void publish(const String& topic, const void* payload, size_t payloadSize) {
```

with `publishSticky` alongside it and `emit(topic, ptr, size, sticky)` wrappers on
both `Core` (`Core.h:196`) and `IComponent` (`IComponent.h:230`).

That is *"publish bytes rather than the object"* — the repair `CLAUDE.md` names for
BUG-30 — already implemented, already merged, and written by the contributor whose
plans v2 was waiting on.

### 2.2 His rewrite deletes seven of the nine sites

On `esp32-ethernet`, `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h` publishes **no
String payload at all**. `EVENT_NETWORK_READY` is gone, replaced by
`NetworkEvents::EVENT_PROVIDER_REGISTERED` / `_STATE_CHANGED` / `_ADDRESS_CHANGED`
carrying structs. The surviving `getAPIP()` / `getLocalIP()` calls are inside
`DLOG_I` format strings. `MQTT_impl.h` publishes no String payload either.

### 2.3 So the answer to v2 §6.2 is "no"

He needs variable-length **byte** payloads. He does not put objects on the bus.
Nothing in his branch carries a `String` through `publish`. Option F — new
machinery for non-POD payloads — is answered and dead.

---

## 3. The recount

v2 said thirteen sites in four components. Counted on `main` at `3e33f94b`,
excluding the `emit` definitions themselves and log statements, the live sites are
**nine**:

| Site | Payload | Lifetime | In his rewrite |
|---|---|---|---|
| `Wifi.h:141,221,234,268,740,799,834` (7) | `getAPIP()` / `getLocalIP()` | **temporary** — destroyed at end of full-expression | **deleted** |
| `OTA.cpp:791` | serialised JSON local, 129–199 B | local, always heap | ours, untouched |
| `OTA.cpp:780` | serialised JSON local | dead code (`broadcastProgress`, DEAD-1) | ours, untouched |

The seven `Wifi.h` sites are the severe ones — a temporary is already destroyed
when `publish` returns, so the copy is of freed memory on any string that does not
fit the small-string optimisation. On ESP8266 that is any IP of eleven characters
or more, which is most of them, and `192.168.4.1` exactly.

---

## 4. What the four blocking review findings become

From `review-bug-30-v2-adversarial.md`:

| # | Finding | Status under v3 |
|---|---|---|
| 1 | §4's measurement was taken on option A/B, not the recommended J | **Moot** — J is not the recommendation. A byte-copy of the same bytes costs what it costs; there is no new representation to measure |
| 2 | E and J cannot coexist through one `void(const void*)` handler; E is in no PR | **Moot** — E and J are both dropped |
| 3 | The E precedent claimed at `ComponentRegistry.h:167-168` does not exist | **Moot** — the claim goes with E |
| 4 | A root `v3.0.0` protects nobody while every inter-component dependency is `>=` | **Moot** — there is no break to signal. Nothing public changes shape |

All four were consequences of a plan that no longer applies. **This is not four
problems solved; it is four problems that were downstream of the wrong shape.**

---

## 5. Proposed lot

Three changes, in dependency order, and the third is the one that matters.

1. **`OTA.cpp:791` publishes bytes.** `publishStatusEvent()` serialises to a local
   `String`; publish `payload.c_str(), payload.length() + 1` through the existing
   sized overload. The NUL is included so a subscriber can treat it as a C string.
   `OTA.cpp:780` is dead (`broadcastProgress`) and is left for DEAD-1 to remove
   rather than converted — converting dead code would be the third time this
   series touched it without deleting it.

2. **The seven `Wifi.h` sites publish bytes**, the same way, from a **named local**
   rather than a temporary. This duplicates work his rewrite deletes, and is
   proposed anyway: his branch is not merged, `main` ships today, and the sites are
   the severe ones. Seven mechanical edits, no signature change, no behaviour change
   for the zero subscribers that read the payload.

3. **The guard goes on the topic overload.** `static_assert(std::is_trivially_copyable<PayloadT>::value, …)`
   on `publish(const String&, const PayloadT&)`, matching `EventBus.h:104`.

   **This is what v2 could not do and v3 can.** The guard was unlandable while the
   only way to publish a variable-length payload was to hand the bus an object,
   because it would have refused legitimate callers with no alternative to offer
   them. `b6660b78` gave them the alternative. After steps 1 and 2 there are zero
   non-trivially-copyable publishers left, so the assert is additive: it breaks
   nothing that exists and stops the next `emit<String>` from being written.

No new concept, no `= delete`d overloads, no POD event structs, no version bump,
no public contract change.

---

## 6. Verification

- **Native, from a cleaned `.pio`**: the Core, Wifi and OTA suites. The assert in
  step 3 is compile-time, so *the suites compiling is the assertion*.
- **The removal check that matters**: with step 3 in place, add a throwaway
  `emit<String>("x", s)` and confirm it fails to compile with the authored message.
  Then remove it. Without that, step 3 is a line nobody has proven does anything.
- **Lifetime, on hardware**: publish from `Wifi.h`, drain the queue, and read the
  payload back as bytes on a `nodemcuv2`. The v2 verification plan's load-bearing
  idea survives intact and is reused — *a lifetime test that does not reuse the
  freed storage passes for the allocator's reasons rather than ours*. See
  `bug30-verification-plan.md` §8.
- **What cannot be verified**: that a `String` subscriber sees corruption, because
  no such subscriber exists and writing one to prove the bug would be writing the
  bug.

---

## 7. Decisions the maintainer still owns

1. **Step 2 — do the seven `Wifi.h` sites, or leave them to his rewrite?** Doing
   them makes `main` correct now and is thrown away when he lands. Leaving them
   means shipping a known use-after-free on the AP path for as long as his branch
   takes. This spec proposes doing them; it is a judgement call about which cost is
   worse, not a technical one.
2. **Does he get told?** `CLAUDE.md` says coordinate rather than surprise him, and
   BUG-29 already changed `MQTT_impl.h` under him. Step 3 changes a signature
   constraint on a file he builds against, and step 2 touches seven lines of a file
   he is rewriting. The maintainer's position is that Mariano will adapt. Recorded,
   not contested — but this lot is the one where the notification would have cost
   least and been worth most, because *he already solved the hard part*.
