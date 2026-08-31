# ARCH-2 — LED "God Object": close by finding, not by fix

Status: v2 — amended after adversarial review, 2026-08-31. The review's
central catch: v1 claimed "no defect anywhere in the roadmap attributes a
bug to LED's structure" while **BUG-19** — `addLED()` after `begin()`
causes vector desync, exactly a shared-state defect in this class — sat
DONE thirty lines from ARCH-2's own entry. That claim is now the argument's
core instead of its blind spot. The review also demoted two claims resting
on the weakest evidence forms (a word-grep, an unexecuted test count) to
the stronger verified facts, and removed a refutable SRP assertion from the
center of the case.

Roadmap item: ARCH-2 [HIGH], ref LED-F1. The handoff's standing
instruction: ARCH-1 and ARCH-2 "do not measure — decide at spec time
whether to execute or re-argue severity like BUG-2." This document is that
decision, for ARCH-2. The decision is **neither** a refactor **nor** a
BUG-2-style downgrade: the entry's own prescribed remedy is already
delivered — half of it was false at filing, the other half landed on
2026-08-23 — so ARCH-2 closes as done-by-measurement, with no code change
in its lot. Branch `arch-2-close-by-finding`, one docs-only PR, no version
movement.

## What the entry claims, dated against the repository

The entry (filed 2026-03-10, first roadmap commit `ae5715e1`) reads:
"Combines config management, hardware pin control, PWM, effect calculations,
state management, WebUI in one class. Fix: Extract effect engine and WebUI
into separate classes."

**Claim "WebUI in one class" — false at filing.** `LEDWebUI.h` has been a
separate class since at least 2025-09-26 (`729a8fd0`, the current layout's
first commit — separation since that date is what the history proves, not
origin), five months before the entry was written. The evidence is stronger
than a word-grep, and stated as such because a grep is not an enumeration:
the LED.h of the filing commit (482 lines) **does not override
`getWebUIProvider()` at all** — the framework's own attachment mechanism —
and today's doesn't either; `LEDWebUI` is referenced by no production code
in the repository, only by its tests and the `LEDWithWebUI` example, which
pairs the two manually. LED.h's own class comment presents that as the
design: "can be paired with a WebUI provider to expose UI controls."
**Decision recorded, not filed**: System registers `LEDComponent` and never
`LEDWebUI`, so a LED UI exists only where a sketch pairs it — that is the
documented opt-in design, not a gap; noted here so the next sweep does not
rediscover it. Today LEDWebUI.h is 204 lines with a dedicated 23-test suite
(established by TEST-6, whose row was wrong about LED in the other
direction — the roadmap has now been wrong about this component twice, both
times claiming coupling or gaps a look at the files refutes).

**Claim "effect calculations [mixed in]" — true at filing, resolved
2026-08-23.** PR #17 (`07cb37a9`, the four-item LED lot: BUG-19, DC-5,
TEST-2, LO-11) extracted the effect engine as pure static functions —
`effectBrightness`, `rainbowColor`, `scaleToMax`, `pwmValue` — "pure
arithmetic, no hardware and no member state", in the code's own words,
split out precisely so the curves could be tested without a board (the HAL
swallows `analogWrite()` on the host). The extraction is real, not
duplicated math: `updateEffects()` (~33 lines) calls the statics and no
curve arithmetic remains inline. `test_led_effects` (29 tests) tests them
directly. That is the testability harm Constitution XIII names for god
objects, eliminated five months after the filing — the entry simply never
moved, the same staleness shape as BUG-26 ("found already fixed; the row
was stale").

The prescribed remedy — "extract effect engine and WebUI into separate
classes" — is therefore delivered in both halves. The engine is a set of
pure statics rather than a class; what XIII protects (testability,
diagnosability) does not distinguish the two, and `test_led_effects` is the
measurement.

## BUG-19: the structure's one recorded defect, and what closed it

The honest version of "does this structure hurt?" is not "no defect was
ever attributed to it" — one was. **BUG-19** (MEDIUM, DONE 2026-08-23,
PR #17): `addLED()` after `begin()` desynchronized `ledConfigs` from
`ledStates` — a shared-state coupling defect of exactly the kind a
god-object entry predicts. What closed it: a local fix and a 35-test
component suite with named regressions, in the same lot that extracted the
engine. No class split was needed, and none of the three classes the split
would produce ("config", "pins", "effects") would have prevented it — the
two vectors that desynchronized would still have to agree across whatever
boundary separated them. The one measured harm this structure produced was
closed by tests, which is XIII's stated rationale ("reduce testability")
addressed at the root.

## What remains in LEDComponent, measured against Constitution XIII

XIII's god-object row is qualitative — "classes with too many
responsibilities." Its **Code Smell Indicators** table is the only
measurement the constitution provides, and that reduction is made
explicitly here: the indicators are what can be measured; the rest is
judgment, argued below rather than derived. LED.h passes every indicator:

| Indicator | Threshold | LED.h |
|---|---|---|
| File size | > 800 lines | **544** |
| Function size | > 50 lines | largest is `updateEffects` at ~33 (measured by brace-depth scan over 4-space-indented signatures, largest hand-checked) |
| Inheritance depth | > 3 | 1 (`IComponent`) |
| Declared dependencies | > 5 | 0 |

The judgment: LEDComponent does still combine config storage, pin init, PWM
write, effect state and a timer loop — several responsibilities by SRP's
reason-to-change test, conceded. What decides against splitting is not SRP
but the constitution's other principles and the absence of measured harm:
KISS and YAGNI are Constitution-mandated with the same force as SOLID; each
"responsibility" is a 50–100 line section of a 544-line file; the split
would manufacture classes passing the `ledConfigs`/`ledStates` pair between
them — the exact coupling BUG-19 lives in, relocated rather than removed;
and the component carries **103 native tests, measured green from clean at
this closure** (`rm -rf .pio`, 103/103: `test_led_component` 35,
`test_led_effects` 29, `test_led_webui` 23, `test_led_types` 16).

**LED-F1, the cited source, does not exist in the repository** — no document
in `docs/` or `specs/` carries it. Like BUG-2's, this severity was inherited
from a sweep whose reasoning cannot be read, and was never argued in the
entry itself. Unlike BUG-2, nothing needs re-arguing to MEDIUM: the remedy
the entry prescribes is measurably present.

## The fork, checked

`esp32-ethernet` touches nothing in `DomoticsCore-LED` but two example
`platformio.ini` files. No coordination constraint.

## What closing this is not

- **Not a precedent that god-object findings close on line counts, and not
  ARCH-1's template.** The contrast is concrete: System orchestrates ten
  components across three files behind 54 `__has_include` directives, with
  persistence, console commands and boot diagnostics attached — the
  indicators that all pass here are the ones System strains. This closure
  rests on ARCH-2's prescribed extractions existing and being tested, a
  fact ARCH-1 does not share.
- **Not a code change.** The lot is documentation: the DONE marker, this
  document, the accounting. If a reviewer wants the pure statics moved into
  a named `LEDEffectEngine` class for taste, that is a new item filed on
  its own merits — XIII gives no measurement that would demand it.

## Measurement (what closes it)

- The two extractions named by the remedy exist and are dated:
  `LEDWebUI.h` (since at least 2025-09-26, pre-filing), the pure effect
  engine (PR #17, 2026-08-23), both with dedicated suites (23 and 29
  tests), the engine's call-site verified free of inline curve math.
- The four XIII indicators, measured in-entry, method stated.
- BUG-19 cited in-entry as the structure's one recorded defect, closed by
  tests in PR #17.
- LED native suites: **103/103 green from clean, measured 2026-08-31.**
- Roadmap: ARCH-2 → DONE with this argument in-entry; HIGH 2 → 1 (ARCH-1
  alone); both reconciliation families; the three-criteria sweep **re-run
  at closure, not predicted**.
