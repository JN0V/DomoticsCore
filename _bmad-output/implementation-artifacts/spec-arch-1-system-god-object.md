# ARCH-1 — System "God Object": re-argue HIGH → MEDIUM, defer the two fork-coupled extractions, decline the third on its own merits

Status: v2 — amended after adversarial review, 2026-08-31. The review's
three central catches, all folded in: the BUG-2 analogy was borrowed
authority (BUG-2's remedies were measured impossible; ARCH-1's are
feasible — this demotion rests on the weaker ground of harm-assessment and
timing, owned as such); the fork carries only **two** of the three
prescribed extractions, so `SystemConsoleCommands` is argued on its own
merits rather than hidden behind the deferral; and the open-ended trigger
is now dual, with the notification draft converting it into a coordination
question. Two half-false claims corrected (boot-diagnostics persistence
lives in System.h; PERSIST-1 is open in SystemPersistence.h).

Roadmap item: ARCH-1 [HIGH], ref SYS-F6. The standing instruction: "decide
at spec time whether to execute or re-argue severity like BUG-2." The
decision: **severity re-argued HIGH → MEDIUM; the item stays open** with a
dual re-evaluation trigger. This is the second consecutive lot in which
the last remaining HIGH leaves the column without code — named here, not
hidden: ARCH-2 closed because its remedy already existed; ARCH-1 demotes
because no measurement supports HIGH and the workable remedy's moment is
after the fork lands. **The milestone therefore reads "zero open HIGH —
the last re-argued MEDIUM and still open", never "all HIGH resolved."**
Branch `arch-1-re-argue-severity`, one docs-only PR, no version movement,
no code change.

## The entry, dated against the repository

Filed 2026-03-10 (`ae5715e1`): "Orchestrates 10 components, handles
registration, persistence, events, state, console commands, boot
diagnostics. Fix: Extract `SystemComponentRegistrar`,
`SystemEventOrchestrator`, `SystemConsoleCommands`."

**SYS-F6, the cited source, cannot be read** — no document in the
repository carries it (BUG-23's SYS-F4 shows the family existed in the
originating sweep; F6's reasoning did not survive). The severity was
inherited, never argued in the entry.

**The dating, stated precisely**: config persistence has been delegated to
`SystemPersistence.h` (340 lines) and the WebUI provider setup to
`SystemWebUISetup.h` (417 lines) since 2025-11-30 (`87d0a182`), three
months before filing — but **boot-diagnostics persistence remains in
System.h proper** (`initBootDiagnosticsPersistence`, its own titled
section), so "handles persistence" was and is partially true. The filing
overstates by one delegation, not two; this document does not repeat the
inverse error.

**"Orchestrates 10 components" is the component's definition, not its
defect.** `docs/project-context.md` names System "Meta-orchestrator
(assembles all components)". Registration — ten `registerXComponent()`
methods of 5–25 lines — is what the class exists for. And the assembler
**maximizes coupling by design**: the ">5 dependencies" indicator's letter
does not apply (System is not an `IComponent`), but its spirit — blast
radius, testability — applies here more than anywhere, which is exactly
why TEST-1 was HIGH and why its suite is the operative mitigation.

## What measurably remains

System.h is 643 lines (623 at filing). Indicators, measured by the same
brace-depth scan ARCH-2's closure used, largest hand-checked:
**`begin()` is 62 lines (> 50) — exceeded, and exceeded at filing too
(62 then, at L113)**; `getBootDiagnostics()` 50, `setupEventOrchestration()`
45. The judgment the entry gestures at is real: console wiring, boot
diagnostics and state/LED mapping (together roughly 220 of the 643 lines)
live in the orchestrator because each touches several components at once,
but a reasonable engineer could house them elsewhere. That is a
MEDIUM-shaped observation — structure worth improving when the moment is
right, harming nothing measured today.

**Defects, cited in full rather than rounded to zero**: BUG-23
(Early-Init in `registerLEDComponent()`, HIGH, DONE 2026-08-22) was a
registration bug worse than its title — the manual `begin()` silently
removed framework-pointer injection. What closed it: a two-line fix and,
later, TEST-1's suites, which gave it "a real check" where it had been
"verified by compilation only". The honest form of the extraction
counter-argument: a `SystemComponentRegistrar` **as prescribed** would
have relocated the same call site, bug included; only a narrower contract
(register, never begin) prevents the class of bug — and that contract
change is precisely what BUG-23's fix wrote, in place, where the
`// BUG-23: register only` comment now stands guard. **PERSIST-1** is
open (MEDIUM) in `SystemPersistence.h` — dead-code shaped, filed from
TEST-1's suite: evidence the delegated file gets audited, not structural
harm in the class, and named here so "the open defects" is a checked list
rather than a claimed zero.

## Why not BUG-2's authority — and what carries the demotion instead

BUG-2's demotion rested on both recorded remedies being **measured
impossible**. ARCH-1's three extractions are feasible; nothing here claims
otherwise. The demotion rests on: no readable argument for HIGH ever
existed (SYS-F6); one XIII indicator exceeded and stable since filing,
against a 643-line file otherwise inside every measurement; the one
historical structural defect closed by contract-fix-plus-tests, with the
suites (three, run at closure — figure written then, not grepped now)
standing as the mitigation XIII's rationale actually asks for; and the
fork evidence below for why executing the two big extractions now is the
wrong moment, not the wrong idea.

## The fork rewrites two of the three extraction zones — and the third is declined on its own merits

Measured (`git diff b17f0a1c fork/esp32-ethernet`, System.h +13/−21, plus
SystemConfig.h +4, SystemWebUISetup.h +25):

1. **Registration**: he inserts `registerNetworkComponent()` into the
   register-method list, adds member, getter, and threads `network` into
   `setupWebUIProviders`. His patch depends on the pattern a
   `SystemComponentRegistrar` would relocate.
2. **Event orchestration**: he deletes the WiFi→MQTT block from
   `setupEventOrchestration()` — `NetworkEvents` replace it — and amends
   the NTP block. The function a `SystemEventOrchestrator` would extract
   is one he is actively shrinking and re-founding on new event types.
3. **`SystemConsoleCommands` overlaps none of his hunks** — the fork
   cannot carry its deferral, so it is declined on its own ground: four
   one-line `registerCommand` lambdas over status getters, no measured
   harm, no defect ever attributed, and a new header plus indirection to
   save ~20 lines of wiring is YAGNI by the constitution's own list. If a
   future defect lands in that wiring, the item to file is that defect.

Executing extractions 1–2 now would conflict with the most structural
branch a contributor has run against this repository, against the standing
rule: coordinate rather than surprise him. After his branch lands, the
orchestrator is smaller and transport-neutral, and the right extraction
shape is visible rather than guessed.

**The trigger is dual, so the deferral cannot become a stall**: ARCH-1 is
re-evaluated — execute, restate, or close — when `esp32-ethernet` lands
**or** at the next release series' planning, whichever comes first; and
the notification draft asks marianorenzi directly where System.h sits in
his plans, converting the trigger into a coordination fact.

## The decision

- **HIGH → MEDIUM**, on the grounds above; the `[HIGH]` heading becomes
  `[MEDIUM]` and leaves the sweep as SEC-12's did.
- **Open**, with the dual trigger in-entry.
- **The notification to marianorenzi becomes due** (zero open HIGH) and is
  **drafted, not sent** — sending is the maintainer's call. The draft is
  constrained in advance: it states ARCH-2 closed by measurement and
  ARCH-1 re-argued MEDIUM and open pending his branch (never "every HIGH
  resolved"), carries the MQTT change he needs (`publish()` queues instead
  of returning false when rate-limited), lists the files his fork overlaps
  that moved (MQTT_impl.h, WebUI.h ×5 lots, StreamingContextSerializer.h,
  the stateful WiFi stub's tearDown obligation), credits his multiselect
  design and `dc8886f1`, and asks his System.h timing.

## Measurement (what lands)

- The roadmap entry rewritten in place: `[MEDIUM]`, the dating (both
  precise halves), BUG-23 with the honest counter-argument, PERSIST-1
  named, the fork measurement, the declined-third, the dual trigger.
- The three System suites run green from clean at closure; the measured
  case count written into the entry.
- Accounting: no new ID, nothing resolved — items 130, resolved 70
  unchanged; HIGH 1 → 0 open, MEDIUM 38 → 39; remaining 73 = 0 HIGH +
  39 MEDIUM + 34 LOW. Sweep re-run, not predicted — expected shape
  34 headings / 34 evidenced / 0 open, the heading-count drop explained
  SEC-12-style in the chain.
- `draft-marianorenzi-notification.md` written under the constraints
  above.
