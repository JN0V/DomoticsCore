# Research: ESP8266 Memory Optimization

**Feature**: 004-esp8266-memory-optim
**Date**: 2025-01-20

---

## R1: ArduinoJson 7 — Static JsonDocument Reuse

**Decision**: Abandoned — not reliably beneficial with ArduinoJson 7

**Rationale**: In ArduinoJson 7, `JsonDocument` always uses `malloc()/realloc()/free()`. The behavior of `clear()` regarding buffer retention is undocumented and may vary between 7.x releases. Additionally, the existing code calls `shrinkToFit()` in several places, which would reallocate and negate any reuse benefit. The actual per-call allocation cost (~128–256 bytes for a typical `getWebUIData()`) is modest compared to the structural issues targeted by this feature.

**Alternatives considered**:
- Custom `ArduinoJson::Allocator` with static buffer: Documented but impractical — requires knowing max JSON size upfront, wastes the static buffer when not in use
- Direct `snprintf` to WebSocket buffer: Already used for `sendWebSocketUpdates()` header; extending to full data would require API change (`getWebUIData(char*, size_t)` instead of returning `String`) — deferred to a potential future feature

---

## R2: ESP8266 String Literals — Flash vs RAM Residency

**Decision**: String literals (`"..."` and `R"()"`) are Flash-resident by default on ESP8266 Arduino

**Rationale**: Verified that no `board_build.mmu` setting is configured in any `platformio.ini`. With the default ESP8266 Arduino core, string constants are placed in the `.rodata` section mapped to Flash (0x40200000–0x402FFFFF) and read via the instruction cache. The core's unaligned-access exception handler transparently supports character-by-character reads from Flash pointers. Therefore, `const char*` pointing to a string literal is effectively "in Flash" without explicit `PROGMEM` annotation.

**Key implication**: When `String s = R"(some CSS)"` is executed, the String constructor copies ~N bytes from Flash to heap RAM. Storing a `const char*` instead eliminates this heap copy entirely. The pointer itself costs 4 bytes (on 32-bit ESP8266) vs. 12+ bytes for an empty String object + N bytes of heap data.

**Alternatives considered**:
- Explicit `PROGMEM` + `pgm_read_byte()`: Not needed for string literals on modern ESP8266 Arduino core (2.x+). Would be needed only for large arrays or data in custom sections.

---

## R3: getWebUIContexts() Removal — Impact Analysis

**Decision**: Remove `getWebUIContexts()` from `IWebUIProvider` and migrate all callers

**Rationale**: Code search reveals:
- **Interface**: `IWebUIProvider::getWebUIContexts()` is pure virtual (line 418 of `IWebUIProvider.h`)
- **Default impls**: `forEachContext()`, `getContextCount()`, `getContextAt()`, `getWebUIContext()` all fall back to `getWebUIContexts()` — these must be made abstract or given cache-based defaults
- **CachingWebUIProvider**: Already overrides `getWebUIContexts()` + `forEachContext()` + `getContextCount()` + `getContextAt()` + `getContextAtRef()` — but NOT `getWebUIContext()`
- **Test files**: ~57 usages in `test_webui_component.cpp`, ~6 in `test_streaming_serializer.cpp`, ~8 in `test_schema_memory.cpp`
- **Production providers**: All inherit from `CachingWebUIProvider` and implement `buildContexts()` — none override `getWebUIContexts()` directly

**Migration path**:
1. Phase 1: Override `getWebUIContext()` in `CachingWebUIProvider` (fix the bug)
2. Phase 2: Remove `getWebUIContexts()` from interface. Make `forEachContext()` pure virtual instead. Update `CachingWebUIProvider` to be the canonical base. Update all tests to use `forEachContext()` or `getContextAtRef()`.

**Risk**: Test updates are mechanical but numerous (~70 call sites). No production code impact since all providers already use `CachingWebUIProvider`.

---

## R4: withCustomHtml/Css/Js — Unified API Design

**Decision**: Replace `withCustomHtml(const String&)` with a single `withCustomHtml(const char*)` that stores the pointer directly

**Rationale**: Since all call sites use either:
- String literals: `withCustomHtml("<div>...</div>")` or `withCustomHtml(R"(...)")`
- `BaseWebUIComponents` return values (which can be changed to return `const char*` for static parts)

A single `withCustomHtml(const char*)` accepting a `const char*` covers both cases. For the rare dynamic case (e.g., interpolated chart JS), the caller constructs a `String` and the field stores it via a separate `withCustomHtmlDynamic(const String&)` or by explicitly calling `.customHtml = myDynamicString`.

**Call site analysis** (production code):
- `BaseWebUIComponents::createLineChart()` → uses `.withCustomHtml(generateChartHtml(...))` where `generateChartHtml` returns `String` (has interpolation) → needs `withCustomHtmlDynamic()` or direct assignment
- `BaseWebUIComponents::createLineChart()` → uses `.withCustomCss(generateChartCss())` where `generateChartCss` returns `String` but content is 100% static → change to return `const char*`
- `LEDWebUI` → `.withCustomCss(R"(...)")` → direct `const char*`, perfect fit
- `WebUIOnly example` → `.withCustomCss(R"(...)")` → direct `const char*`
- All test files → string literals → direct `const char*`

**Design**:
```
withCustomHtml(const char* ptr)     → stores pointer, clears String
withCustomHtmlDynamic(const String&) → stores in String, clears pointer
```

**Alternatives considered**:
- Keep both `withCustomHtml(const String&)` and `withCustomHtmlFlash(const char*)`: More verbose, confusing naming. Since sole developer, cleaner to just change the API.
- Single overloaded method with `const char*` and `const String&`: Ambiguous — `const char*` would match string literals but `String` overload would match String variables. Risk of silent wrong dispatch.

---

## R5: WebUIConfig char[] — Pattern Verification

**Decision**: Apply exact same pattern as MQTTConfig, WifiConfig, OTAConfig, HAConfig, NTPConfig

**Rationale**: All 5 other config structs have been migrated in a previous session (memory 731dea88). The pattern is:
- `char field[N]` member
- `String getField() const { return String(field); }` getter
- `void setField(const char* v) { strncpy(field, v, sizeof(field)); field[sizeof(field)-1] = '\0'; }` setter
- NEW per spec: Add `DLOG_W` on truncation

No research needed — pattern is proven and well-tested.

---

## R6: EventBus dispatching_ Guard — Implementation Pattern

**Decision**: Add `bool dispatching_ = false` flag + `assert()` in debug builds

**Rationale**: The `const auto&` iteration change is the primary optimization. The `dispatching_` flag is a safety net for future development.

**Implementation**:
```cpp
// In EventBus class:
bool dispatching_ = false;

// In poll(), wrapping the dispatch loop:
dispatching_ = true;
// ... iterate with const auto& ...
dispatching_ = false;

// In subscribe() and unsubscribe():
#ifndef NDEBUG
assert(!dispatching_ && "Cannot subscribe/unsubscribe during EventBus dispatch");
#endif
```

This uses standard `assert()` which is stripped in release builds (when `NDEBUG` is defined). On Arduino/ESP8266 debug builds, `assert()` prints to Serial and aborts, which is the desired behavior for catching invariant violations during development.

---

## R7: ComponentMetadata const char* — Dependency on ComponentRegistry

**Decision**: Convert to `const char*` with String conversion at map insertion only

**Rationale**: `ComponentRegistry::componentMap` is `std::map<String, IComponent*>`. Changing the map key type is out of scope (high risk, P2 in the deferred list). The `const char*` to `String` conversion at `registerComponent()` time is a one-time cost per component (happens once at boot). After that, lookups use `String` keys (from user input or API params).

All `ComponentMetadata` fields (`name`, `version`, `author`, `description`) are always initialized with string literals in component constructors. No runtime mutation observed.

`Dependency::name` is also always a literal. Same treatment.

---

## Summary of Unknowns Resolved

| Unknown | Resolution | Impact |
|---------|------------|--------|
| ArduinoJson 7 static reuse | Abandoned | P8 removed from plan |
| Flash residency of literals | Confirmed without PROGMEM | P2/P3 feasibility confirmed |
| getWebUIContexts() removal scope | ~70 test call sites, 0 production impact | Effort estimated accurately |
| withCustomHtml API design | Single `const char*` + `withCustomHtmlDynamic` | Cleaner than dual API |
| WebUIConfig pattern | Reuse proven pattern from 5 other configs | Low risk confirmed |
| EventBus guard pattern | `assert()` + `NDEBUG` | Standard C++ approach |
| ComponentMetadata map interaction | Convert at insertion time only | No map key change needed |
