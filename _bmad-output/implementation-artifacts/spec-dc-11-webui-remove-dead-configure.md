---
title: 'DC-11 — remove the WebUI configure() extension channel that nothing reads'
type: 'refactor'
created: '2026-08-26'
status: 'implemented-pending-hardware'
review_loop_iteration: 0
baseline_commit: '003f8626c3db47769d15c71ce53b1b47bdc55714'
context:
  - '{project-root}/.specify/memory/constitution.md'
  - '{project-root}/docs/components/webui/project-context.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `WebUIField::configure()` and `WebUIContext::configure()` lazily allocate a `JsonDocument` and write into it. No caller exists anywhere in the repository — production, examples, tests or docs code. No serializer reads it. The only other code touching the members deep-copies them, so every field or context copy can duplicate a document that will never be sent. On ESP8266, with 80 KB of RAM against the ESP32's 320 KB, that is an unbounded allocation per field for nothing. Constitution IV (YAGNI) and XIV (memory, ABSOLUTE PRIORITY) both apply.

**Approach:** Delete the channel and its storage outright, including the deep-copy branches, and correct the two documentation references that advertise it. The maintainer has decided that structured field data will travel through explicit schema keys instead of a generic JSON blob; this lot removes the generic half. It does not design the explicit half.

## Boundaries & Constraints

**Always:**
- Removal only. No new schema key, no new field type, no serializer state added in this lot.
- The emitted JSON shape is unchanged. Nothing serialized `config` or `contextConfig`, so nothing on the wire moves.
- No version bump, no CHANGELOG entry.
- English files and commit messages; conventional commits.

**Ask First:**
- **Any change to `webui_src/app.js`.** marianorenzi is fixing a defect in that exact file and function region. Nothing in this lot needs the frontend; if something appears to, stop.
- **Any change to the wire format.** If removal turns out to require touching what the serializers emit, that is no longer this lot.
- **Designing how table rows travel.** `WebUIField::value` is a `String`, so rows would arrive as JSON inside a string. That is the open decision behind DC-12 and it is deliberately not settled here.

**Never:**
- Do not fix the Multiselect dangling-literal defect found during investigation (see Design Notes). Record it; do not touch it. It sits in the same file and would enlarge this diff into the serializer, which is precisely where marianorenzi's work will land.
- Do not close DC-12. It stays open, and its entry records the decision plus why the fix is frontend.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Schema request after removal | any registered provider | Byte-identical JSON to before | N/A |
| Copying a `WebUIField` | field with options, labels, endpoint | All members copied; no document to copy | N/A |
| Moving a `WebUIField` | defaulted move | Still compiles and moves every remaining member | N/A |
| Existing caller of `configure()` | none exist | Build succeeds; if any is found, HALT | Report before deleting |

</frozen-after-approval>

## Code Map

- `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h` — the whole change. `WebUIField::config` member :168; copy ctor deep-copy :191-193; copy assign :216-220; `WebUIField::configure` :249-255. `WebUIContext::contextConfig` member :297; copy ctor :323-325; copy assign :352-356; `WebUIContext::configure` :399-402. Note the members have **different names** — `config` on the field, `contextConfig` on the context. Move ctor/assign are `= default` (:226-227, :362-363) and need no edit.
- `docs/components/webui/technical-reference.md:174` and `:252` — signature listings that advertise the method. Both must go.
- `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h` — read-only confirmation that neither member is referenced.
- `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h:820-861` — the DOM emitter; same confirmation.
- `docs/CODE-ROADMAP.md` — DC-11 and DC-12 entries are uncommitted in the working tree and are part of this lot.

Investigation result carried forward so it is not re-derived: a repo-wide sweep for the bare token `configure` found **zero call sites**. Every other hit is prose, a log string, or an unrelated identifier (`configureTime`, `getConfiguredSSID`). No other type in the tree has a member named `configure`.

## Tasks & Acceptance

**Execution:**
- [x] `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h` — delete both `configure()` methods, both members, and the four copy-path branches that reference them. Re-run the sweep first and report what it found before deleting anything.
- [x] `docs/components/webui/technical-reference.md` — remove the two signature listings, so the documentation stops advertising a method that no longer exists.
- [x] `docs/CODE-ROADMAP.md` — mark DC-11 done. Rewrite DC-12 to record the decision (explicit schema keys; `configure()` removed) and state that its own fix is frontend work sequenced behind marianorenzi's change. Add the serializer finding from Design Notes as a new entry. Keep the tracking summary's rows summing to its total — it currently reads 106 items, `0C, 8H, 27M, 33L`, 48 resolved.

**Acceptance Criteria:**
- Given a clean `.pio`, when the native WebUI suites run, then all pass — including `test_streaming_serializer` and `test_webui_component`.
- Given the three declared targets, when the FullStack example is cross-compiled, then `esp32dev`, `esp8266dev` and `esp32c3` all build.
- Given a board on `/dev/ttyUSB0`, when the two WebUI on-device suites run, then both pass with heap thresholds unchanged — **met, 2026-08-26: `DomoticsCore-WebUI` (`test_filter = test_heap_esp8266`) 6/6 in 31.66 s, and `test_schema_memory` 3/3 in 39.55 s. The "nine" quoted in CLAUDE.md is the sum of the two suites, not one of them; this criterion originally misread it as 9/9 for the first.**
- Given the schema endpoint before and after, when both outputs are compared, then they are byte-identical.
- Given `git grep configure` over the repository, when the change is complete, then no hit refers to the removed methods.

## Spec Change Log

- **Trigger:** the maintainer asked whether downstream consumers of the published library call `configure()`. The Intent justified removal on "no caller exists anywhere in the repository", which is true and insufficient: this is a public method on a public struct in a library published on the PlatformIO Registry and installed by version. The sweep proved the repository was clean, not that the API was unused.
- **Verified after the fact, 2026-08-26:** the known downstream consumers were checked and none calls it, nor touches either member. That check cannot be exhaustive, and it is not what makes the removal safe to ship quietly.
- **Amended:** the roadmap entry now flags the change as **breaking, requiring a MAJOR bump** at release — independent of whether a caller can be enumerated. Nothing in the code changed. Consumer projects are deliberately not named: they are separate repositories and their dependency state is not this roadmap's business.
- **Known-bad state avoided:** shipping a public API removal recorded as ordinary dead-code cleanup, so the release that carries it is cut as a minor version.
- **KEEP:** the "what it actually saved, stated honestly" paragraph in the roadmap entry. Because `configure()` was never called, both pointers were always null, no `JsonDocument` was ever allocated, and there was no leak — the realised saving is 4 bytes per field and per context plus four dead branches. That refusal to overstate is the STOR-ESP-1 lesson applied, and it must survive any re-derivation.

## Design Notes

Removing this is not merely tidying. `WebUIField` is copied — the copy constructor exists precisely to preserve hybrid pointer state — and each copy carrying a `JsonDocument` deep-copy is a per-field heap cost on the platform the constitution protects first.

**Found during investigation, deliberately out of scope.** `writeLiteral` (`StreamingContextSerializer.h:431-449`) caches its `str` argument in the member `currentLiteral` so a literal can resume across `write()` calls. The Multiselect value path (`:700-708`) builds a `String serializedValues` local to the `case` block and passes `serializedValues.c_str()`. If the output buffer fills mid-literal, that `String` is destroyed before the next call: either the pointer compares equal to a reallocated buffer and resumes into freed-then-reused memory, or it compares unequal and the array is restarted from offset zero after part of it was already written, producing malformed JSON. No test forces a chunk boundary inside a multiselect value. This must be recorded as its own finding, and it is the reason any future dynamic schema key must hold its serialized text in a serializer member rather than a local.

## Verification

**Commands:**
- `cd DomoticsCore-WebUI && rm -rf .pio && pio test -e native` — expected: all suites green.
- `cd DomoticsCore-WebUI && rm -rf .pio && pio test -e esp8266dev` — expected: 9/9.
- `cd DomoticsCore-WebUI/test/test_schema_memory && rm -rf .pio && pio test -e esp8266dev` — expected: pass, thresholds untouched.
- `cd DomoticsCore-System/examples/FullStack && rm -rf .pio && pio run -e esp32dev -e esp8266dev -e esp32c3` — expected: three clean builds.
- `git diff --stat` — expected: `IWebUIProvider.h`, `technical-reference.md`, `CODE-ROADMAP.md`. Nothing else, and **not** `webui_src/app.js`.
