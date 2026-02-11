# Feature Specification: ESP8266 WebUI Memory Optimization (3 Phases)

**Feature ID**: 004-esp8266-memory-optim
**Status**: Complete
**Created**: 2025-01-20
**Branch**: `004-esp8266-memory-optim`

---

## 1. Overview

### 1.1 Problem Statement

The DomoticsCore framework running on ESP8266 suffers from excessive RAM consumption and heap fragmentation, primarily caused by the WebUI subsystem. The ESP8266 has only ~80 KB of usable RAM, of which the current firmware consumes 64–75% at compile time (before runtime allocations). At runtime, repeated dynamic memory allocations (Arduino `String` objects, `std::map<String, ...>`, `JsonDocument` temporaries, and deep copies of `WebUIContext` vectors) cause heap fragmentation that leads to instability, failed allocations, and eventual crashes — especially when multiple WebSocket clients are connected.

### 1.2 Feature Description

Reduce the runtime RAM footprint and heap fragmentation of the DomoticsCore framework on ESP8266 through a structured 3-phase optimization plan:

- **Phase 1 (Quick wins)**: Targeted fixes to configuration storage, event bus iteration, and a provider cache bug — minimal risk, immediate impact.
- **Phase 2 (Static data migration)**: Move static HTML/CSS/JS content and component metadata out of heap-allocated `String` objects into Flash-resident `const char*` pointers.
- **Phase 3 (Structural refactoring)**: Refactor the core `WebUIField`/`WebUIContext` data structures to use a hybrid storage model, reducing per-field heap allocations.

### 1.3 Target Platform

- **Primary**: ESP8266 (Wemos D1 Mini, NodeMCU) — ~80 KB usable RAM
- **Secondary**: ESP32 (benefits from reduced fragmentation even with more RAM)
- **Constraint**: All changes must remain backward-compatible with ESP32 builds and the existing WebUI frontend protocol

### 1.4 Scope

**In Scope**:
- Optimizing RAM usage of `WebUIConfig`, `EventBus`, `CachingWebUIProvider`, `BaseWebUIComponents`, `ComponentMetadata`, `WebUIField`, and `WebUIContext`
- Modifying `StreamingContextSerializer` to support `const char*` data sources
- Maintaining full functional equivalence with the current WebUI behavior

**Out of Scope**:
- Replacing `std::map` with custom hash/array structures (deferred — high risk, moderate gain)
- Changing the WebSocket protocol or buffering strategy
- Rewriting `ArduinoJson` usage patterns (static `JsonDocument` reuse is not reliably supported in ArduinoJson 7)
- Frontend (JavaScript/HTML) changes
- Adding new features — this is a pure optimization effort

---

## Clarifications

### Session 2025-01-20

- Q: Les phases doivent-elles être strictement séquentielles (gate dur) ou peuvent-elles se chevaucher ? → A: **Gate strict** — Phase N terminée et vérifiée (code + tests + build) avant de démarrer Phase N+1.
- Q: Quand une valeur est tronquée lors de l'écriture dans un champ char[], faut-il émettre un log d'avertissement ? → A: **Oui** — Émettre un `DLOG_W` dans chaque méthode `setXxx()` quand la valeur dépasse la taille du buffer.
- Q: Pour l'EventBus, faut-il ajouter une assertion runtime en debug en plus du commentaire documentant l'invariant single-thread ? → A: **Oui** — Ajouter un flag `dispatching_` et un `assert(!dispatching_)` dans `subscribe()`/`unsubscribe()` en debug builds.
- Q: Faut-il capturer une mesure baseline formelle (taille build + heap snapshots) avant toute modification ? → A: **Oui** — Capturer et documenter le build size ESP8266 FullStack + heap snapshot comme référence avant la Phase 1.
- Q: Après Phase 2, les anciennes API (`getWebUIContexts()`, `withCustomHtml(String)`) doivent-elles être marquées deprecated ? → A: **Suppression directe** — Seul utilisateur du codebase, pas besoin de période de dépréciation. Nettoyer directement.

---

## 2. User Scenarios & Acceptance Criteria

### Scenario 1: Stable Operation Under Memory Pressure

**As a** device user running a FullStack configuration (WiFi + MQTT + WebUI + OTA + HomeAssistant + NTP + SystemInfo) on ESP8266,
**I want** the system to remain stable and responsive,
**So that** the WebUI loads correctly and real-time updates continue without interruption.

**Acceptance Criteria**:
- AC-1.1: After Phase 1, the FullStack example compiles with static RAM usage ≤ 74% (currently 75.4%)
- AC-1.2: After Phase 2, static RAM usage ≤ 72%
- AC-1.3: After all 3 phases, free heap at runtime remains above 8 KB (MINIMAL memory profile threshold) during normal operation with 2 WebSocket clients connected
- AC-1.4: No regression in WebUI functionality — all existing unit tests pass, WebSocket updates are received correctly by connected clients

### Scenario 2: WebUI Schema Delivery

**As a** browser client connecting to the WebUI,
**I want** to receive the complete UI schema via the chunked HTTP response,
**So that** the dashboard renders all cards, fields, and controls correctly.

**Acceptance Criteria**:
- AC-2.1: The `/api/ui/schema` endpoint returns valid JSON containing all registered contexts and fields — identical structure to the pre-optimization output
- AC-2.2: `customHtml`, `customCss`, and `customJs` content is correctly JSON-escaped in the schema response even when sourced from Flash-resident `const char*` pointers
- AC-2.3: Chart components (using `BaseWebUIComponents`) render and update correctly

### Scenario 3: Configuration Persistence

**As a** user modifying WebUI settings (theme, color, credentials) through the settings panel,
**I want** my changes to be saved and applied,
**So that** the device retains my preferences across reboots.

**Acceptance Criteria**:
- AC-3.1: Setting `theme`, `primaryColor`, `username`, and `password` via the WebUI API writes values correctly to the `WebUIConfig` struct (now using fixed-size `char[]` arrays)
- AC-3.2: Values exceeding the maximum field length are safely truncated without memory corruption
- AC-3.3: Authentication via `request->authenticate()` works correctly with `char[]`-based credentials

### Scenario 4: Event-Driven Component Coordination

**As a** system integrator using the EventBus for WiFi→MQTT→HomeAssistant coordination,
**I want** events to be dispatched reliably without unnecessary memory spikes,
**So that** the device doesn't run out of heap during event bursts.

**Acceptance Criteria**:
- AC-4.1: EventBus `poll()` dispatches events without copying the handler vector (uses `const` reference iteration)
- AC-4.2: All existing event-driven coordination (WiFi→MQTT reconnect, NTP sync notification, HA discovery) continues to function correctly
- AC-4.3: Peak heap usage during a burst of 8 events is reduced compared to pre-optimization baseline

### Scenario 5: Developer Experience — Adding a New Provider

**As a** developer creating a new `CachingWebUIProvider` subclass,
**I want** the provider API to remain familiar and well-documented,
**So that** I can add WebUI cards without understanding the internal optimization details.

**Acceptance Criteria**:
- AC-5.1: The `CachingWebUIProvider` API (`buildContexts`, `withField`, `withCustomHtml`) evolves in place — existing provider code compiles after updating to the new API (old `String`-based methods removed in Phase 2, see NFR-1)
- AC-5.2: `withCustomHtml(const char*)` is the primary API for static content; `withCustomHtmlDynamic(const String&)` is available for runtime-generated content
- AC-5.3: `getWebUIContext(contextId)` on a `CachingWebUIProvider` searches the cache directly without triggering a full vector copy

---

## 3. Functional Requirements

### Phase 1 — Quick Wins (Low Risk, Immediate Impact)

#### FR-P1.1: WebUIConfig Fixed-Size Storage

Convert `WebUIConfig` from `String` members to fixed-size `char[]` arrays, following the established pattern already applied to `MQTTConfig`, `WifiConfig`, `OTAConfig`, `HAConfig`, and `NTPConfig`.

| Field | Type | Max Size |
|-------|------|----------|
| `deviceName` | `char[32]` | 31 chars + null |
| `theme` | `char[8]` | 7 chars + null (auto/dark/light) |
| `staticPath` | `char[16]` | 15 chars + null |
| `primaryColor` | `char[8]` | 7 chars + null (#RRGGBB) |
| `username` | `char[32]` | 31 chars + null |
| `password` | `char[48]` | 47 chars + null |

- FR-P1.1a: Provide `getXxx()` methods returning `String` for backward compatibility
- FR-P1.1b: Provide `setXxx(const char*)` methods using safe `strncpy` with null-termination. Each setter must emit a `DLOG_W` warning when the input value is truncated (i.e., input length ≥ buffer size)
- FR-P1.1c: All existing read/write access points in `WebUI.h`, `WebSocketHandler.h`, and `System.h` must be updated to use the new API

**Expected gain**: ~200–350 bytes permanent RAM + elimination of 6 heap-allocated String copies per `WebUIConfig` instance (2 instances: `WebUIComponent` + `WebSocketHandler`)

#### FR-P1.2: EventBus Iteration Without Copy

Replace the defensive copy pattern in `EventBus::poll()` with `const` reference iteration.

- FR-P1.2a: Change `auto handlers = itT->second;` to `const auto& handlers = itT->second;` for topic subscriptions, wildcard subscriptions, and typed subscriptions (3 locations in `poll()`)
- FR-P1.2b: Add a `bool dispatching_` flag set to `true` during `poll()` iteration. In debug builds, add `assert(!dispatching_)` guards in `subscribe()` and `unsubscribe()` to catch accidental reentrancy. Add a code comment documenting the single-threaded safety assumption
- FR-P1.2c: No behavioral change in event delivery order or semantics

**Expected gain**: ~400 bytes – 1.2 KB peak heap reduction per `poll()` cycle (proportional to number of subscriptions)

#### FR-P1.3: Fix CachingWebUIProvider getWebUIContext()

Override `getWebUIContext(const String& contextId)` in `CachingWebUIProvider` to search the internal cache directly, avoiding the current behavior that copies the entire context vector.

- FR-P1.3a: The override must call `ensureContextsCached()` then iterate `cachedContexts_` by reference
- FR-P1.3b: Return a copy of the matching `WebUIContext` (same return type as base class)
- FR-P1.3c: Return an empty `WebUIContext()` if no match is found

**Expected gain**: ~2–3 KB peak heap reduction per `getWebUIContext()` call (eliminates full vector deep-copy)

### Phase 2 — Static Data Migration (Moderate Complexity)

#### FR-P2.1: Flash-Resident Custom HTML/CSS/JS

Add support for storing `customHtml`, `customCss`, and `customJs` as `const char*` pointers to Flash-resident string literals, instead of heap-allocated `String` objects.

- FR-P2.1a: Add `const char* customHtmlPtr = nullptr`, `customCssPtr`, `customJsPtr` members to `WebUIContext`
- FR-P2.1b: Add `withCustomHtml(const char*)`, `withCustomCss(const char*)`, `withCustomJs(const char*)` builder methods that store the pointer and clear the String. Add `withCustomHtmlDynamic(const String&)`, `withCustomCssDynamic(const String&)`, `withCustomJsDynamic(const String&)` for runtime-generated content
- FR-P2.1c: When a Flash pointer is set, the corresponding `String` member must remain empty (no duplication)
- FR-P2.1d: `StreamingContextSerializer` must check Flash pointers first, falling back to `String` members if the pointer is null
- FR-P2.1e: Add `writeJsonCString(const char*)` method to `StreamingContextSerializer` for character-by-character streaming from `const char*` (same JSON-escaping logic as `writeJsonString`)

**Expected gain**: ~2–4 KB permanent RAM for providers using static HTML/CSS/JS (e.g., `BaseWebUIComponents` chart CSS is ~500 bytes, chart JS template is ~800 bytes)

#### FR-P2.2: Migrate BaseWebUIComponents to Flash Pointers

Update `BaseWebUIComponents` static methods to use Flash-resident strings where content is fully static (no interpolation).

- FR-P2.2a: `generateChartCss()` — return a `const char*` to a static `R"(...)"` literal (100% static content, no interpolation)
- FR-P2.2b: For methods with interpolation (e.g., `generateChartJs()`), separate the static template portion from the dynamic parameters. The static template resides in Flash; only the interpolated result (~100–200 bytes) uses a `String`
- FR-P2.2c: Update `createLineChart()` to use `withCustomCss(const char*)` for the CSS portion
- FR-P2.2d: All other `BaseWebUIComponents` helper methods (`progressBar`, `toggleSwitch`, `button`, `textInput`, etc.) remain unchanged in Phase 2 — they return small HTML fragments where the overhead is minimal

#### FR-P2.3: ComponentMetadata Fixed Pointers

Convert `ComponentMetadata` members (`name`, `version`, `author`, `description`) from `String` to `const char*`, since these are always initialized with string literals.

- FR-P2.3a: All constructors must accept `const char*` and store pointers directly (no copy)
- FR-P2.3b: Callers must ensure the pointed-to strings outlive the component (guaranteed for literals)
- FR-P2.3c: `ComponentRegistry::componentMap` continues to use `String` keys — the conversion from `const char*` to `String` happens at map insertion time only
- FR-P2.3d: `Dependency::name` is also converted to `const char*`

**Expected gain**: ~600 bytes – 1 KB permanent RAM (eliminates ~15 String objects across a typical FullStack configuration)

### Phase 3 — Structural Refactoring (High Complexity)

#### FR-P3.1: Hybrid Storage for WebUIField

Introduce a hybrid storage model for `WebUIField` where fields initialized with string literals store `const char*` pointers, while fields with dynamic values (e.g., from runtime `String` variables) continue to use `String`.

- FR-P3.1a: Add `const char*` pointer members alongside existing `String` members for `name`, `label`, `value`, `unit`, and `endpoint`
- FR-P3.1b: When a `const char*` pointer is set (non-null), the corresponding `String` member must be empty
- FR-P3.1c: Add accessor methods (e.g., `const char* getNameCStr() const`) that return the pointer if set, or `String::c_str()` otherwise
- FR-P3.1d: The existing constructor `WebUIField(const char*, const char*, WebUIFieldType, const char*, const char*, bool)` must store pointers directly without creating `String` copies
- FR-P3.1e: Add a constructor overload accepting `const String&` for dynamic values that stores in the `String` member

#### FR-P3.2: Update StreamingContextSerializer for Hybrid Fields

Extend `StreamingContextSerializer` to read field data from either `const char*` or `String` sources transparently.

- FR-P3.2a: For each field property serialized (`name`, `label`, `value`, `unit`, `endpoint`), check the `const char*` pointer first; if non-null, use `writeJsonCString()`; otherwise use `writeJsonString()` with the `String` member
- FR-P3.2b: For `contextId`, `title`, `icon`, and `apiEndpoint` in `WebUIContext`, apply the same hybrid read pattern
- FR-P3.2c: Ensure no regression in JSON output format — the serialized output must be byte-identical for the same input data regardless of storage backend

#### FR-P3.3: Maintain Copy and Assignment Semantics

Ensure `WebUIField` and `WebUIContext` copy constructors and assignment operators correctly handle the hybrid state.

- FR-P3.3a: When copying a field with a `const char*` pointer, the copy must also store the pointer (not convert to `String`)
- FR-P3.3b: When copying a field with a `String` value, the copy must create a new `String` (existing behavior)
- FR-P3.3c: The `CachingWebUIProvider` cache must correctly store and return hybrid-storage contexts

**Expected gain**: ~5–7 KB permanent RAM (eliminates ~60–80 String objects in a typical FullStack configuration with 8 providers × 3–5 fields each)

---

## 4. Non-Functional Requirements

### NFR-1: Backward Compatibility

All changes must be source-compatible with existing provider code. Existing calls to `withCustomHtml(const String&)`, `WebUIField(const char*, ...)`, etc. must continue to compile and work correctly. New optimized methods are additive.

After Phase 2 completion, the following methods are **removed** (sole-developer codebase — no deprecation period needed):
- `getWebUIContexts()` → replaced by `forEachContext()` or `getContextAtRef()`
- `withCustomHtml(const String&)` → replaced by `withCustomHtml(const char*)` for static content, `withCustomHtmlDynamic(const String&)` for dynamic content
- `withCustomCss(const String&)` → same pattern
- `withCustomJs(const String&)` → same pattern

All call sites and tests are updated in the same phase.

### NFR-2: Platform Neutrality

All changes must compile and run correctly on both ESP8266 and ESP32. Flash-pointer optimizations benefit both platforms. No `#ifdef` platform splits are introduced for the optimization logic itself (platform-specific constants like `WEBUI_WS_BUFFER_SIZE` remain unchanged).

### NFR-3: No Behavioral Regression

The WebUI frontend must receive identical JSON data structures before and after optimization. WebSocket update format, schema endpoint response format, and API response formats must not change.

### NFR-4: Testability

Each phase must be independently testable. Existing unit tests must pass after each phase. New tests should validate the specific optimizations (e.g., `getWebUIContext()` not triggering a full copy, `char[]` truncation behavior).

### NFR-5: Performance

Optimization must not introduce measurable latency in WebSocket update cycles or HTTP response times. The `StreamingContextSerializer` reading from `const char*` must be at least as fast as reading from `String` (character-by-character access is equivalent).

---

## 5. Key Entities

| Entity | Current Storage | Target Storage | Phase |
|--------|----------------|----------------|-------|
| `WebUIConfig` | 6 `String` members | 6 `char[]` arrays (144 bytes fixed) | Phase 1 |
| `EventBus::poll()` handler vectors | Copied per dispatch | `const` reference iteration | Phase 1 |
| `CachingWebUIProvider::getWebUIContext()` | Full vector deep-copy | Direct cache search | Phase 1 |
| `WebUIContext::customHtml/Css/Js` | `String` (heap) | `const char*` (Flash) + `String` fallback | Phase 2 |
| `ComponentMetadata` | 4 `String` members | 4 `const char*` pointers | Phase 2 |
| `WebUIField` members | `String` (heap) | Hybrid `const char*` / `String` | Phase 3 |
| `WebUIContext` identity fields | `String` (heap) | Hybrid `const char*` / `String` | Phase 3 |

---

## 6. Dependencies & Assumptions

### Dependencies

- **ArduinoJson ^7.0.4**: No changes to ArduinoJson usage. `JsonDocument` remains stack-allocated with default heap allocator. The `shrinkToFit()` calls in existing code are unaffected.
- **ESPAsyncWebServer**: No changes to server or WebSocket handling.
- **Existing config migration pattern**: Phase 1 (FR-P1.1) reuses the exact same `char[]` + getter/setter pattern already applied to `MQTTConfig`, `WifiConfig`, `OTAConfig`, `HAConfig`, and `NTPConfig`.

### Assumptions

- **Single-threaded execution**: ESP8266 runs a single-threaded cooperative loop. EventBus handlers do not call `subscribe()`/`unsubscribe()` during dispatch. This assumption is validated by code review of all current handlers.
- **String literals lifetime**: All `const char*` values stored in `ComponentMetadata` and Flash-pointer fields point to string literals with static storage duration. No runtime-generated strings are stored as raw pointers.
- **WebUIField initialization**: The majority (~80%) of `WebUIField` objects are initialized with string literal arguments in `buildContexts()` methods. The remaining ~20% use runtime `String` values (e.g., LED names, scan results).
- **Maximum field sizes**: The `char[]` sizes chosen for `WebUIConfig` are sufficient for all reasonable use cases. Values exceeding the limit are truncated — this is acceptable for configuration fields.

---

## 7. Success Criteria

| Criterion | Metric | Target | Verification Method |
|-----------|--------|--------|---------------------|
| Static RAM reduction (Phase 1) | Compiler-reported RAM % for FullStack example | ≤ 74% (from 75.4%) | PlatformIO build output |
| Static RAM reduction (Phase 2) | Compiler-reported RAM % for FullStack example | ≤ 72% | PlatformIO build output |
| Runtime free heap (all phases) | `ESP.getFreeHeap()` during normal operation with 2 WS clients | ≥ 8 KB | Runtime measurement via `/api/system/info` |
| Peak heap reduction | Maximum heap usage during WebSocket update cycle | Measurable reduction vs. baseline | HeapTracker instrumentation |
| Test pass rate | Existing unit tests | 100% pass | PlatformIO test runner |
| Functional equivalence | Schema JSON output comparison | Byte-identical structure | Diff comparison of `/api/ui/schema` output before/after |
| Build compatibility | ESP32 builds | No new warnings or errors | PlatformIO build for ESP32 targets |
| Baseline reference | ESP8266 FullStack build size + runtime heap snapshot | Captured and documented before Phase 1 | PlatformIO build output + `/api/system/info` snapshot committed to `specs/004-esp8266-memory-optim/baseline.md` |

---

## 8. Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| `char[]` truncation causes data loss | Low | Medium | Sizes chosen to exceed all reasonable values; log a warning on truncation |
| EventBus handler modifies subscriptions during dispatch | Low | High | Code comment + assertion in debug builds; documented API contract |
| Flash pointer stored for runtime-generated string | Low | Critical (dangling pointer) | Clear API distinction (`withCustomHtml(const char*)` vs `withCustomHtmlDynamic(const String&)`); naming convention signals intent |
| Phase 3 hybrid storage introduces copy/assignment bugs | Medium | High | Comprehensive unit tests for copy, assignment, move semantics; test both pointer and String paths |
| Optimization gains less than estimated | Medium | Low | Each phase is independent — partial delivery still provides value |

---

## 9. Phasing Summary

| Phase | Scope | Estimated Gain | Effort | Risk | Files Modified |
|-------|-------|----------------|--------|------|----------------|
| **Phase 1** | WebUIConfig char[], EventBus const ref, CachingWebUIProvider fix | ~3–5 KB (peaks + permanent) | 1–2 days | Low | ~5 files |
| **Phase 2** | Flash pointers for static content, ComponentMetadata const char*, old API cleanup | ~3–5 KB permanent | 2–3 days | Moderate | ~10 files |
| **Phase 3** | Hybrid WebUIField/Context storage | ~5–7 KB permanent | 3–4 days | High | ~15 files |
| **Total** | | ~11–17 KB RAM freed | 6–9 days | | |

### Phase Gate Criteria

Each phase is a **strict gate**: all items in Phase N must be completed, tested (unit tests pass), and verified (build size + heap measured) before any Phase N+1 work begins. This ensures:
- Clean isolation of changes for debugging
- Accurate measurement of per-phase gains
- A stable checkpoint to roll back to if Phase N+1 introduces regressions

### Baseline Measurement (Pre-Phase 1)

Before starting Phase 1, capture and commit to `specs/004-esp8266-memory-optim/baseline.md`:
- ESP8266 FullStack example: PlatformIO build output (RAM %, Flash %)
- Runtime free heap at boot, after init, and during steady-state with 2 WS clients
- `/api/ui/schema` JSON output (reference snapshot for NFR-3 verification)
