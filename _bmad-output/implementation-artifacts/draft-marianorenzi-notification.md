# DRAFT — notification to marianorenzi (NOT SENT — the maintainer sends, edits, or discards)

Written 2026-08-31, when the open-HIGH count reached zero. The maintainer's
standing rule: he is notified once every CRITICAL and HIGH is closed. The
precise state — stated the same way in the roadmap — is: **zero open HIGH;
the last one (ARCH-1, the System god-object item) was re-argued to MEDIUM
and stays open**, partly because your branch is the reason not to touch
System.h right now. Nothing below claims "all HIGH resolved."

Suggested channel: a GitHub discussion or an issue comment where he is
already active. Written in English, like everything public here.

---

Hi Mariano,

The remediation campaign that started after v2.2.0 has reached the
milestone I wanted to reach before writing to you: there are no open
CRITICAL or HIGH items left on the roadmap (the last HIGH, the System
god-object item, is re-argued to MEDIUM and deliberately deferred — more
on that below, it concerns your branch directly). Several of the merged
lots touched files your `esp32-ethernet` branch rewrites, so here is the
complete list of what moved and why, oldest first.

**MQTT — the one behaviour change you should know about.**
`MQTTComponent::publish()` no longer returns `false` when rate-limited: it
queues and returns `true`, and the queue drains on every connected
`loop()` at the configured rate (BUG-29 — devices declaring >10 HA
entities were silently losing discovery messages). If your code branched
on that `false`, the branch is now dead. The limiter also no longer
applies while disconnected (`maxQueueSize` is the bound there).

**WebUI.h — five lots, and your two hunks were deliberately preserved.**
SEC-10 added a per-boot CSRF token: every state-changing route now
requires it, and `/api/ui/action` moved GET → POST (a corrective release
for third-party front-ends is pending). BUG-32 escaped `device_name` at a
new `WebUI/SystemHeader.h` sink. SIZE-1 then split the file
(1008 → 769): the schema chunk loop is now
`SchemaChunkState::writeChunk()` in `ProviderRegistry.h`, plus two new
headers (`SchemaMemProbe.h`, `UpdateBuilder.h`). Throughout, the two
regions your branch modifies — the `onComponentsReady` subscription block
and `serializeContext`'s multiselect block — are byte-identical to what
you branched from, as is the include block your hunks use as context.
Your rebase of WebUI.h should be near-clean.

**StreamingContextSerializer.h — your design, adopted and credited.**
SIZE-2 split it (933 → 756 + a 216-line `JsonStreamWriter.h`, a
privately-inherited base — no call site changed name or shape, so your
hunks land). Your streaming-multiselect states were adopted outright,
adapted to the current field API, your `optionIndex` → `arrayIndex`
rename included — that closed BUG-28, and we found BUG-26 had already
been fixed by your `dc8886f1` back in July. After a rebase, your
multiselect hunk should reduce to a no-op. Related: BUG-34 — the
`/api/ui/schema` route had never received the v1.5.0 chunked-truncation
fix; both routes now share one retry helper.

**EventBus — the `publish(topic, payload)` template overload now
`static_assert`s trivially-copyable** (BUG-30). Your
`publish(topic, const void*, size_t)` — which your `b6660b78` introduced
— is the intended escape hatch for variable-length payloads and is what
the framework itself now uses.

**Tests — one obligation your branch inherits.** The WiFi stub is now
stateful (TEST-4): defaults are byte-identical, but any suite that boots
an AP fixture must call `resetWifiStateForTest()` in `tearDown()` — three
of ours do, and yours will need the same if they touch AP mode.

**System.h — the question.** The remaining architecture item (ARCH-1)
prescribes extracting the component-registration and event-orchestration
code out of `System`. I measured your branch: it inserts
`registerNetworkComponent()` into the register-method list and rewrites
`setupEventOrchestration` around `NetworkEvents` — exactly the two zones
such an extraction would move. So it is deferred until your branch lands
rather than creating conflicts on top of you. Two questions, no urgency:
where does `System.h` sit in your plans for `esp32-ethernet` — and is
there anything on `main` right now that would make your rebase easier if
it changed?

Thanks for the fork — reading it has repeatedly been the fastest way to
answer design questions here (BUG-30 and the multiselect streaming both
came from it).

---

*Maintainer notes (delete before sending): BUG-27 / DC-11 / DC-12 — his
WebUI findings — are recorded as his to fix, not chased here; the message
deliberately doesn't press him on them. PERSIST-1 sits in `Wifi.h`
territory his branch overlaps; it is ours and still open.*
