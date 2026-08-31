# SIZE-2 — StreamingContextSerializer.h under 800 lines, without surprising the fork

Status: v2 — amended after adversarial review, 2026-08-31. The review
falsified two sentences the first draft's Measurement section depended on:
the chunk sweep cannot start at 1 (escape sequences are atomic within one
write — sizes below the longest escape in the content livelock; the second
review pass sharpened this further: the floor must match the content, so the
maximal context carries a control character to make its floor-6 honest and
exercise the six-byte early-out), and "no output change
anywhere" was false as written — the escaper classifies control characters
with a bare `char` comparison, so its behaviour depends on char signedness.
**Measured**: `__CHAR_UNSIGNED__` is defined on all three shipped toolchains
(xtensa-esp32, xtensa-lx106, riscv32-esp) and absent on host gcc — devices
are correct, the native host mangles every UTF-8 byte into `\u00XX`. That is
BUG-33, filed and fixed in this lot (below).
Roadmap item: SIZE-2 [HIGH], ref WEB-F3. File is 933 lines today (921 when
filed); Constitution VII caps at 800. The filed remedy — "unify duplicated
writeJsonString overloads, extract field serialization helpers" — is right
about the first half and needs reshaping on the second, because of a fact the
filing predates:

## The constraint that shapes everything: `esp32-ethernet` rewrites this file

`marianorenzi`'s 96-file branch modifies `StreamingContextSerializer.h`
(+49/−25) and its test file. His patch:

1. **Replaces the multiselect `ValueValue` hack** — the `JsonDocument` +
   temporary-`String` rebuild-per-resume — with real streaming states
   (`ValuesArrayOpen/Value/Comma/Close`), reading his renamed
   `field.values` via his new `field.isMultiValue()`.
2. Renames `optionIndex` → `arrayIndex` throughout `writeField`.

That hack is also a fragility this lot probed: `writeLiteral` resumes on
**pointer identity**, and the temporary's address is stable only because the
allocation sequence is deterministic per resume. A probe test
(`test_multiselect_across_chunk_boundaries`, chunk sizes 2–32, byte-identical
against an unsplit reference) was written first and **passed on current code**.
**Correction found during the roadmap pass: this was already filed — it is
BUG-28 (2026-08-26), which describes the destroyed-String resume and the
comparison against an indeterminate pointer, and ends "the fix needs [a test
that forces a chunk boundary inside a multiselect value]".** The v1 draft of
this spec, written before re-reading the Code Safety section, said "not filed
as a bug per the SEC-9 discipline" — wrong in the other direction: the finding
existed, this lot closes it, and the probe (plus the maximal sweep) is the
test its entry asked for. The green probe stands as measurement that the UB's
usual outcome is the accidental-correctness the entry described, not the
malformed JSON, on the native allocator. BUG-26, the same file's other
pause/resume defect, turned out to be **already fixed** — `dc8886f1`
(marianorenzi, 2026-07-16) split the option-label states and shipped
`test_option_labels_across_chunk_boundaries`; the row was stale at filing.
Both close with this lot.

**Consequence**: any refactor that renames the helper call sites or moves the
`writeField` switch to another file turns his every hunk into a conflict. The
design below keeps both state machines in place, byte-for-byte, and adopts his
multiselect design (credited) so his rebase of this file approaches a no-op.

## BUG-33 (filed and fixed in-lot, before the refactor)

`writeJsonString`'s control-character check is `char c = …; if (c < 0x20)`
(both overloads). With signed `char` every byte ≥ 0x80 sign-extends negative
and is emitted as `\u00XX` of its low byte: `"é"` (0xC3 0xA9) becomes
`Ã©`, which parses back as `"Ã©"`. **On every shipped target char
is unsigned** (measured: `__CHAR_UNSIGNED__` on xtensa-esp32, xtensa-lx106,
riscv32-esp), so no device ever produced the mangled form — but the native
host does, which means a native UTF-8 test on today's code pins behaviour no
board has. LOW severity (host-only divergence, no production impact), fixed
here because the refactor routes multiselect values — previously serialized
by ArduinoJson, which is byte-transparent — through this escaper, and the
one-line fix (`static_cast<unsigned char>(c) < 0x20`, both overloads, then
unified) is what makes that routing behaviour-preserving on the host too.
Pinned by a UTF-8 test (title + unit + multiselect value round-trip through
a parse) that is **red natively before the fix** — the removal check — and
matches device behaviour after.

## Design

### New: `WebUI/JsonStreamWriter.h` (~230 lines)

The resumable streaming primitives, extracted as a **privately-inherited base**
so that not one call site in either state machine changes name or shape:

```cpp
class JsonStreamWriter {
protected:
    const char* currentLiteral = nullptr;
    size_t literalOffset = 0;
    size_t stringOffset = 0;
    char numBuf[16];

    void resetWriter();                      // the three offset/pointer members
                                             // (numBuf needs no reset: every
                                             // reader snprintf-fills it first)
    size_t writeLiteral(uint8_t*, size_t, const char*);
    bool isLiteralComplete() const;
    size_t writeJsonString(uint8_t*, size_t, const char*);   // adapter
    size_t writeJsonString(uint8_t*, size_t, const String&); // adapter
private:
    size_t writeJsonStringCore(uint8_t*, size_t, const char* data, size_t len);
};
```

Adapters, exactly (the review caught a null deref in the first draft's
pseudocode): the `const char*` adapter is
`const char* p = s ? s : ""; return writeJsonStringCore(buf, maxLen, p, strlen(p));`
and the `String&` adapter passes `(str.c_str(), str.length())`. Includes for
the new header: `#pragma once`, a Doxygen `@file` block,
`<DomoticsCore/Platform_HAL.h>` (the String type), `<cstring>`, `<cstdint>`,
`<cstddef>`, the three-level namespace. The two function-local
`static const char hex[]` tables merge into one in the core — no behavioural
change, stated so a reviewer does not hunt for it. There is no ArduinoJson
include to drop from the serializer header — it never had one; `JsonDocument`
arrived transitively via `IWebUIProvider.h`, which stays.

- **The two 70-line `writeJsonString` overloads become one core over
  `(data, len)`.** The `String&` adapter passes `(c_str(), length())`, the
  `const char*` adapter passes `(s ? s : "", strlen(s))` — each path's exact
  current behaviour, including the null normalization and the (theoretical)
  embedded-NUL difference between them, is preserved by construction.
- `numBuf` moves with the writer: both machines share one, exactly as both
  machines share the one member today; only one number streams at a time.
- One shared `stringOffset`/`currentLiteral` — cross-boundary resumability
  between the two machines is state the split must not duplicate.

### `StreamingContextSerializer.h` (~740 lines)

- `class StreamingContextSerializer : private JsonStreamWriter` — the switch
  bodies keep calling `writeLiteral(...)` / `writeJsonString(...)` /
  `isLiteralComplete()` unqualified. **Diff inside the two switches is
  exactly: (a) the mechanical `optionIndex` → `arrayIndex` rename (touches
  the declaration, `begin()`, `State::FieldComma`, and ~10 lines of the
  Options/OptionLabels states — the fork's own rename, adopted so his hunks
  land on identical lines), and (b) the multiselect hunk.** Nothing else.
- `begin()` replaces its writer-member resets with `resetWriter()`.
  **Production reuses one serializer across contexts** (`ProviderRegistry`,
  `WebUI.h`) while every existing test constructs fresh — so a
  `resetWriter()` that forgets a member survives the whole suite. A reuse
  test is therefore mandatory (below).
- **Dead member removed**: `currentString` is declared and reset and never
  read — it goes, with a line in the roadmap entry.
- **Multiselect streaming, adopted from the fork** (adapted to today's API:
  `field.type == WebUIFieldType::Multiselect` for his `isMultiValue()`,
  `field.selectedValues` for his `values`): `Value` branches to
  `ValuesArrayOpen → ValuesArrayValue ⇄ ValuesArrayComma →
  ValuesArrayClose → ValueComma`. **`ValuesArrayOpen` writes `[`, resets
  `arrayIndex`, and does the empty check inline, exactly as
  `FieldsArrayOpen` does for empty `fields`** — jumping straight to
  `ValuesArrayClose` when `selectedValues` is empty, so `ValuesArrayValue`
  is never entered with nothing to write and **no new non-writing state
  joins the `n == 0` guard lists** (which stay exactly `OptionsCheck` /
  `OptionLabelsCheck` in `writeField`, and the three `*Check` states in
  `write()`). The `Value` state's branch happens at literal completion, a
  writing state, so it adds no non-writing transition either. The
  `JsonDocument` block and the per-resume temporary disappear.
- Public API unchanged: `begin/write/isComplete/getTotalBytesWritten/
  getChunkCount` — the metrics members stay in the derived class, semantics
  untouched.

### What this is not

- **Not a WebUI.h change.** `serializeContext` at `WebUI.h:894` has the same
  JsonDocument multiselect pattern; it is not resumable, so it has no
  fragility, and that function is SIZE-1's territory.
- **On-device output is unchanged everywhere.** The multiselect array bytes
  are produced by our escaper instead of ArduinoJson's; with BUG-33 fixed
  first, bytes ≥ 0x80 pass through on host and target alike, so for ASCII
  and UTF-8 content the bytes are identical to the old path. Residual
  divergence from ArduinoJson: `\b`/`\f` (ArduinoJson escapes them short-form,
  our escaper as the six-char `\\u0008`/`\\u000c` forms) — parse-equivalent, pinned by the UTF-8
  parse test, and unreachable from any provider in the repository.

## Measurement (what closes a SIZE item)

- `StreamingContextSerializer.h` ≤ 800 lines (projected ~740) and
  `JsonStreamWriter.h` ≤ 800 (projected ~230): `wc -l` in the roadmap entry.
- Existing suite — 11 tests including the two chunk-boundary sweeps and the
  multiselect probe — runs **unchanged** before and after: necessary but not
  sufficient (three mutants of the new code survive it — see the review), so
  the tests below and the named mutations complete it. `rm -rf .pio` before
  any run that counts.
- **Pre-refactor tests, written and green on the old code first** (except
  BUG-33's, red by design until its fix):
  1. *UTF-8 parse round-trip* (BUG-33's pin): title, unit and a multiselect
     value carrying `"é"`/`"°C"` parse back intact. **Red natively before the
     one-line fix, green after** — and green describes what every board
     already does.
  2. *Maximal-context byte sweep*: customHtml/Css/Js on, options,
     optionLabels, a multiselect field, an escapable character in a value —
     chunk sizes **6–64** (escape sequences are atomic within one `write`;
     sizes below the longest escape in the content livelock — the content
     includes a control character precisely so that floor is 6 and the
     six-byte `\u00XX` early-out is exercised), each run byte-identical to
     a 1024-chunk run.
  3. *Empty-multiselect sweep*: `selectedValues` empty must emit `[]` at
     every chunk size — the old ArduinoJson path's behaviour, and the new
     `ValuesArrayOpen` inline check's discriminator.
  4. *Serializer reuse*: `begin()` a large context, write two small chunks,
     abandon, `begin()` a second context, stream to completion — byte-equal
     to a fresh serializer's output. This is what makes a forgetful
     `resetWriter()` fail; production reuses serializers, the current suite
     never does.
- **Named mutations on the NEW code** (the TEST-4 standard), each expected
  red, `rm -rf .pio` between: (1) drop `writeLiteral`'s pointer-identity
  resume (always restart) → sweeps red; (2) drop `stringOffset` restore in
  the core's early-out → sweeps red; (3) `resetWriter()` forgets
  `stringOffset` → reuse test red; (4) `String&` adapter passes `strlen`
  instead of `length()` → (expected: nothing reds — records the embedded-NUL
  asymmetry as unpinned, stated in the entry rather than silently assumed).
- Board: `test_schema_memory` on the nodemcuv2, **with a multiselect field
  added to one of its contexts** — without it the suite never enters the one
  path whose allocation behaviour changes, and its pass would be a
  compile-and-no-crash check sold as proof. With it, the removal of the
  per-resume `JsonDocument` becomes measurable where it matters.

## Fork coordination, stated

The multiselect hunk is his design, credited in the commit message and the
roadmap entry. After this lands, his rebase of `StreamingContextSerializer.h`
reduces to: drop his multiselect hunk (already present, modulo his
`IWebUIProvider` renames), keep his `platformio.ini`/test additions. This is
the BUG-30 pattern — the fork had already written the mechanism — applied
before the conflict instead of after it.

## Order

1. BUG-33: UTF-8 test (red natively) → one-line fix in both overloads →
   green. Run suite.
2. Pre-refactor tests 2–4; run suite (15 green on old-plus-fix code).
3. `JsonStreamWriter.h` + private inheritance + `resetWriter()` +
   `currentString` removal + `arrayIndex` rename. Run suite.
4. Multiselect streaming states. Run suite.
5. Mutations 1–4; `wc -l` both files; native WebUI suites; board
   `test_schema_memory` with its new multiselect field.
6. Roadmap: SIZE-2 → DONE (measurements in-entry), BUG-33 filed and fixed
   in-lot, HIGH 4 → 3, both reconciliation families, accounting audit in
   parallel with code review.
