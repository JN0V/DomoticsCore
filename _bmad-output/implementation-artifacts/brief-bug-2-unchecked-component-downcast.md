# BUG-2 — analysis brief

**Status**: input to a BMAD team analysis. No decision taken, no code written.
**Date**: 2026-08-28
**Item**: BUG-2, `docs/CODE-ROADMAP.md`, **HIGH**, open — one of the nine.

**The question is not how to fix it. It is whether it is a defect at all**, and if
so whether its severity is right. The maintainer's reading, which this brief was
written to test rather than to support: *"if it's not possible, it's not possible
— and I'm not sure this was a real bug."*

---

## 1. The finding, as filed

`DomoticsCore-Core/include/DomoticsCore/Core.h:102-105`

```cpp
template<typename T>
T* getComponent(const String& name) {
    auto* component = componentRegistry.getComponent(name);
    return component ? static_cast<T*>(component) : nullptr;
}
```

> **Problem**: `getComponent<T>()` uses `static_cast<T*>` with no runtime type
> check. Wrong type = undefined behavior.
> **Fix**: Add type-key verification before cast, or use `dynamic_cast` (if RTTI
> enabled).

Filed as `CORE-F2`. It carries no DONE marker, appears in no release table and in
no merged lot. It was **absent from both the severity rows and the total** until
2026-08-27, when enumerating every `[HIGH]` heading found it — the same method,
and the same class of miss, as BUG-21 one day earlier.

---

## 2. Both recorded fixes are dead. Measured, 2026-08-28.

### 2.1 `dynamic_cast` does not compile on either platform

Not "if RTTI enabled" — RTTI is **off on both**:

```
Core.h:104:28: error: 'dynamic_cast' not permitted with '-fno-rtti'
```

from `pio run -e esp8266dev` **and** `-e esp32dev` on
`DomoticsCore-System/examples/FullStack`, the only example that pulls all twelve
components.

**A first probe said the opposite and was worthless.** Building
`test_ota_esp8266`, `test_ota_esp32` and `native` with the `dynamic_cast` in place
succeeded on all three. The device suites construct `OTAComponent` directly and
never call `getComponent<T>()`, so the template was never instantiated and never
type-checked; the native environment does instantiate it, and the host toolchain
has RTTI on. Three green builds, none of them about the question. Recorded here
because the pattern — *a check that passes for reasons unrelated to what it
claims* — is the one this repository keeps paying for.

### 2.2 Type-key verification would check nothing

`getTypeKey()` is virtual with a default of `""` (`IComponent.h:129`), and it is
documented as a WebUI mechanism:

> *Optional: Stable type key to identify component kind (e.g., "system_info").
> Used by WebUI to attach composition-based UI wrappers automatically.*

**Two of twelve components override it** — `OTAComponent` (`"ota"`) and
`SystemInfoComponent` (`"system_info"`). For the other ten the comparison is
`"" == ""`, which matches everything. Storage, MQTT, WiFi, NTP, WebUI, LED,
RemoteConsole, HomeAssistant and System would all pass a check that catches
nothing, silently.

Making it work means giving every component a **static** identity `T` can name at
compile time — a new concept in the public component API of a library people
install by version, and one `marianorenzi`'s `esp32-ethernet` components would
also have to adopt. That is the addition the maintainer is questioning.

---

## 3. What the defect actually requires to bite

The registry is `std::map<String, IComponent*>` keyed by the name each component
sets on itself. So a mismatch needs a caller to pair a **valid** name with the
**wrong** type:

```cpp
core.getComponent<StorageComponent>("MQTT");   // UB on first use
```

- A **misspelled** name returns `nullptr`. Safe, and this is the common slip.
- All **70 in-repo call sites** pair name and type conventionally, in
  `SystemWebUISetup.h`, `SystemPersistence.h`, examples and tests.
- Nothing in the library calls it with a caller-supplied string.
- Component names are ordinary strings — `"Storage"`, `"MQTT"`, `"System Info"`,
  `"HomeAssistant"` — so an application registering its own component under a
  built-in's name, then fetching it with the built-in's type, would reach it.

So the trigger is an application-side programming error that the compiler cannot
catch and that produces a wrong-typed pointer rather than a null one.

**The case for it being real anyway**: the function's own documentation says
*"Pointer to component cast to T or nullptr if not found"*. It honours that for a
wrong name and silently breaks it for a wrong type. A public accessor that returns
`nullptr` on one kind of failure and undefined behaviour on the other is a footgun
regardless of how often it is fired.

**The case against HIGH**: no in-repo path reaches it, no reported occurrence
exists, the common failure mode is already safe, and it has sat open for the
entire life of the project without producing a symptom. Compare the other eight
HIGH — MEM-2, BUG-30, TEST-4, TEST-6, SIZE-1, SIZE-2, ARCH-1, ARCH-2 — and ask
whether this belongs beside them.

---

## 4. Options

**A — static identity, opt-in.** A macro in `IComponent.h`, one line per
component, adopted by the twelve here; `getComponent<T>()` checks when `T`
declares it and behaves as today when it does not (SFINAE). Non-breaking. Partial
adoption gives partial protection, silently — including for third-party
components.

**B — static identity, mandatory.** Same mechanism, no fallback: every component
declares it or fails to compile. Complete and verifiable; a breaking change for
every third-party component, `esp32-ethernet` included.

**C — rewrite the entry, do not fix.** Record that both recorded fixes are
impossible, state what the defect actually requires, and re-argue the severity.
No new concept, no API change.

**D — narrow the contract instead of the code.** Leave the cast, fix the
documentation so it stops promising a safety it does not provide, and say plainly
that the name/type pairing is the caller's responsibility.

---

## 5. Questions for the team

1. **Is BUG-2 a defect, or a documented responsibility of the caller?** The answer
   decides whether anything is written at all.
2. **If it is a defect, is HIGH right?** It was invisible to the tracking table
   until last week; it has never been re-argued on merit, only re-counted.
3. **Does a partial guard (A) make things better or worse?** A check that silently
   passes for components that did not opt in is the shape of defect this
   repository has recorded three times — SEC-2 inert, the `end(false)` HAL pin,
   the `dynamic_cast` probe above.
4. **Is any of this `marianorenzi`'s to weigh in on?** The component API is what
   his fork extends. BUG-30 is already parked on a question nobody has put to him.
5. **What would a test for this even look like?** A native test can register two
   components and fetch one with the other's type — but the failure it would
   assert on is UB, which is not a thing a test can observe reliably.

---

## 6. Constraints

- One lot, one PR, off `main`. No version bumps inside the lot.
- Whatever is decided, the entry is rewritten: it currently recommends two fixes,
  and neither compiles.
