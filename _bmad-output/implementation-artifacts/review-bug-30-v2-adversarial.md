# Adversarial review — BUG-30 spec v2

**Reviewed**: `spec-bug-30-v2-eventbus-payload-contract.md` and
`bug30-verification-plan.md`, 2026-08-27, before any code was written.
**Lens**: adversarial. 17 findings.

**Verdict: v2 is a far better document than v1 and is still not ready to
implement.** v1's premise, mechanism, inventory and measurements were wrong; v2
fixed all four. What v2 introduced is a recommendation that does not cohere with
its own landing plan, a headline measurement that belongs to a different option,
and a compatibility argument that rests on protection the repository does not
have.

The three verified independently after the review are marked ✔.

---

## The four that block implementation

### 1. ✔ The hardware measurement measured the option that was rejected

§4's two arms were `publish(topic, s)` and `publish(topic, s.c_str(), s.length()+1)`
— a 155-byte character copy. **That is option A/B**, which §5 marks dead. The
recommendation is J, a 64-byte POD.

So 4.1 KB and 1 104 B belong to an option nobody is proposing, and §9.2 calls the
1 104 B "the number that would flip J to K". J's real per-event delta is roughly a
third of that and its fragmentation profile is **unmeasured**.

The third arm needs no new code either — `publish(topic, buf, 64)` is the same
existing overload. Until it is run, §9's first unknown is "J has never been
measured".

### 2. ✔ E and J cannot coexist as specified

`EventBus.h:20` fixes `Handler = std::function<void(const void*)>`. Under J the
pointer is a POD copy in the queue; under E it would be a live `String&`
delivered inline. **Nothing tells a subscriber which it has.** And I (the
`= delete` + typed-helper guards) makes `on<String>` a compile error, so E's
payload would be reachable only through a raw `subscribe` with an unguarded
`static_cast` — the exact form §6 calls "wrong under every option".

Worse, E appears in the recommendation line and in **none of the six PRs**. PR 4
makes those topics payload-less, which is option K. A four-part recommendation
whose landing plan silently delivers three.

### 3. ✔ `ComponentRegistry.h:167-168` is not the precedent claimed

§5 says publish-then-poll there is "synchronous dispatch written the long way".
It is not: `publish(EVENT_SHUTDOWN_START, String(""))` destroys the temporary at
the semicolon, *before* `poll()` on the next line. It is queued dispatch with an
immediate drain, and `poll()` defaults to 8 per call so it is not even guaranteed
to reach the event.

E is therefore scored as low-risk on a precedent that does not exist. An
implementer converting a site to "the pattern already at 167-168" would ship the
same use-after-free under a new name.

### 4. ✔ v3.0.0 protects nothing at component level

Every inter-component dependency is open-ended — `DomoticsCore-Core >=1.4.0`
(HomeAssistant, OTA, RemoteConsole, Storage, SystemInfo), `>=1.0.0` (MQTT, NTP,
System). Only LED pins with a caret, and LO-29 currently files that as the
*defect*.

A user depending on `DomoticsCore-OTA` silently resolves any future Core,
including one carrying the new payload contract. The root manifest's major stops
nothing for them. `tools/check_versions.py` does not bound dependency ranges.

---

## Substantive, not blocking

- **§3's inventory asserts completeness with no reproducible method** — no grep
  pattern, no swept path set, no statement of which components were checked and
  found clean. v1 was rejected for exactly this. Swept independently by the
  reviewer, the result holds; the finding is the missing evidence, not a missing
  site. The count is also inconsistent — heading says twelve, table has thirteen
  rows, §6 says thirteen.
- **The plan's own gate was violated and not enforced.** §3.4 requires the
  arm-to-arm delta be checked against a budget written before the run, and says
  that landing outside means "the option analysis must be rebuilt before the
  number is used". It landed outside — 4.1 KB against 6.0–6.8 KB and ~10 KB — and
  §5's per-event column was not rebuilt. §5's `~76 B` is never reconciled with
  §4's measured 72 B/event.
- **The harness is unrepresentative in ways the cost model makes load-bearing.**
  §4 never states the topic string, yet the model makes topic-String heap a term
  and every real topic exceeds the ESP8266 10-char SSO (`network/ready` 13,
  `ota/completed` 13, `component/ready` 15). One topic, one subscriber, so the six
  distinct `ota/*` topics — six `lastByTopic` values, six `pendingByTopic`
  counters, all permanent — contributed nothing. Fragmentation is dominated by
  small-allocation count, so the harness understates 1 104 B in the direction that
  favours the recommendation.
- ✔ **A16, the plan's designated load-bearing on-silicon test, exercises a path
  that publishes nothing.** `OTA.cpp:178-181` sets `lastError`, logs, and returns
  `false` — no `publishStatusEvent` on that branch. With a floor it fails
  permanently and reads as a fix defect; without one it passes vacuously and is
  recorded as *the* on-silicon evidence. Precisely the failure §8 claims to
  encode against.
- **J drops a field, it does not truncate one.** `publishStatusEvent` always sets
  `lastResult` (`OTA.cpp:731`) *and* every error lambda sets `error`. Both
  co-occur on `ota/error`. `char message[48]` inside a 64-byte budget holds one.
  §6.1 attack 3 tells the reviewer the loss is bounded at 47 characters when it is
  a whole field.
- **Fixed-size POD event payloads are already the in-tree convention, and the
  spec cites none of them.** `MQTTMessageEvent` `char payload[700]`,
  `HACommandEvent` `char command[128]`, `StorageChangedEvent` `char key[64]` —
  `MQTT.h:64` even carries the comment "Uses fixed-size char buffer for safe copy
  via EventBus memcpy". 48 bytes is an order of magnitude below every sibling, and
  the same subsection would settle the `char ip[16]` vs IPv6 attack.
- **G is costed but not specified, and needs a second breaking change.** Handler
  takes no size, so delivering one changes `Handler` and the signature of every
  raw `subscribe` in the tree and downstream. §7 mentions neither.
- **K's real cost is applied and never charged.** PR 4 makes `system/ready` and
  `shutdown/start` payload-less. §5 documents that typed subscribers then go
  *silent* — and that cost appears in neither §6, §6.1's six attacks, nor the
  CHANGELOG list.
- **The no-deprecation argument does not survive §6's own concession.** §7 rules
  out a grace release because it would "ship known UB with the warning attached to
  the wrong person", concluding a compile error is "the only signal". §6 concedes
  two paragraphs earlier that a raw `subscribe` cast compiles and is wrong under
  every option, unreachable by any assert. The chosen design ships that too.
- **The compile-guard control is unimplementable as written.** "Grep the
  diagnostic text" never says what text, and the two mechanisms differ: the
  `static_assert` has an authored message, a `= delete`d overload produces
  compiler-worded, GCC/Clang-divergent text.
- **The roadmap bookkeeping has no target.** §7 gives starting figures and deltas
  but never the resulting figures, so the one arithmetic CLAUDE.md says has broken
  twice has nothing to check against. And "BUG-30 → DONE" leaves the row body
  containing two now-false statements: that `publishStatusEvent()` is the only
  `emit<String>` caller, and a repair paragraph recommending option A/B.

---

## The structural finding

**§6.2 makes the whole recommendation contingent on a question nobody has asked.**
If `marianorenzi`'s transport-neutral rewrite needs non-POD payloads, F replaces
J and thirteen conversions become work his rewrite reopens.

§7 gates only PR 2 on that coordination. PRs 3, 4, 5 and 6 are pure-J work that a
"yes" invalidates, and they are gated on nothing. Four cross-component payload
PRs could merge into a protected `main` before the answer arrives — and reverting
a merged public payload change costs a second major.

**A document whose recommendation flips on an unasked question is a deferral, not
a decision.** Either ask it and reissue, or say so in the Status line.
