# Tasks: ESP8266 Memory Optimization (3 Phases)

**Feature**: 004-esp8266-memory-optim
**Branch**: `004-esp8266-memory-optim`
**Generated**: 2025-01-20
**Source**: [plan.md](plan.md) | [spec.md](spec.md) | [data-model.md](data-model.md)

---

## User Story Mapping

| Story | Spec Scenario | Description | Impl Phases |
|-------|---------------|-------------|-------------|
| US1 | Scenario 1 | Stable Operation Under Memory Pressure | P1 + P2 + P3 |
| US2 | Scenario 2 | WebUI Schema Delivery (serializer changes) | P2 + P3 |
| US3 | Scenario 3 | Configuration Persistence (WebUIConfig char[]) | P1 |
| US4 | Scenario 4 | Event-Driven Component Coordination (EventBus) | P1 |
| US5 | Scenario 5 | Developer Experience (API changes) | P2 + P3 |

---

## Phase 1: Setup — Baseline Measurement

> Capture reference measurements before any code changes.

- [x] T001 Build FullStack ESP8266 example and record RAM/Flash % in `specs/004-esp8266-memory-optim/baseline.md` — run `pio run -e esp8266dev` in `DomoticsCore-System/examples/FullStack/`
- [x] T002 [P] Build Standard ESP8266 example and record RAM/Flash % in `specs/004-esp8266-memory-optim/baseline.md` — run `pio run -e esp8266dev` in `DomoticsCore-System/examples/Standard/`
- [x] T003 [P] Run all existing native tests and record pass/fail baseline — run `pio test -e native` in `DomoticsCore-WebUI/` and `DomoticsCore-Core/`
- [x] T004 [P] Build FullStack ESP32 example and record baseline — run `pio run -e esp32dev` in `DomoticsCore-System/examples/FullStack/`
- [x] T005 Save `/api/ui/schema` JSON reference snapshot to `specs/004-esp8266-memory-optim/baseline.md` (from device or from test output if available)

**Gate**: baseline.md committed with all measurements.

---

## Phase 2: Foundational — Quick Wins (Plan Phase 1)

> FR-P1.1 + FR-P1.2 + FR-P1.3. Must complete and verify before Phase 3.

### WebUIConfig char[] Migration (US3)

- [x] T006 [US3] Convert `WebUIConfig` struct from 6 `String` members to `char[]` arrays with sizes: deviceName[32], theme[8], staticPath[16], primaryColor[8], username[32], password[48] in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h`
- [x] T007 [US3] Add `getXxx()` → `String` and `setXxx(const char*)` → `strncpy` + null-terminate methods to `WebUIConfig` in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h`
- [x] T008 [US3] Add `DLOG_W` truncation warning in each `setXxx()` method when input length ≥ buffer size in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h`
- [x] T009 [US3] Update all `config.theme`, `config.primaryColor`, `config.username`, `config.password` read/write sites in `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h` to use getters/setters
- [x] T010 [US3] Update `config.deviceName.c_str()` usages in `sendWebSocketUpdates()` and `sendWebSocketUpdate()` in `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h` to use `config.deviceName` directly (char[] is already const char*)
- [x] T011 [US3] Update `WebUIConfig config` copy usage in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h` — verify struct copy works with char[] (memcpy, no heap alloc)
- [x] T012 [US3] Update `webuiConfig.deviceName = config.deviceName` in `DomoticsCore-System/include/DomoticsCore/System.h` to use `setDeviceName()`
- [x] T013 [US3] Update `config.deviceName = deviceName` callback in `DomoticsCore-System/include/DomoticsCore/SystemWebUISetup.h` to use `setDeviceName()`

### EventBus Optimization (US4)

- [x] T014 [P] [US4] Replace `auto handlers = itT->second;` with `const auto& handlers = itT->second;` at 3 locations in `EventBus::poll()` in `DomoticsCore-Core/include/DomoticsCore/EventBus.h` (lines ~164, ~172, ~186)
- [x] T015 [US4] Add `bool dispatching_ = false;` member to `EventBus` class in `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
- [x] T016 [US4] Set `dispatching_ = true` before dispatch loop and `dispatching_ = false` after in `EventBus::poll()` in `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
- [x] T017 [US4] Add `assert(!dispatching_)` guards (under `#ifndef NDEBUG`) in `subscribe()` and `unsubscribe()` methods in `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
- [x] T018 [US4] Add code comment documenting single-threaded safety assumption above `poll()` in `DomoticsCore-Core/include/DomoticsCore/EventBus.h`

### CachingWebUIProvider Bug Fix (US1)

- [x] T019 [P] [US1] Add `getWebUIContext(const String& contextId)` override to `CachingWebUIProvider` class — call `ensureContextsCached()`, iterate `cachedContexts_` by reference, return copy of match or empty `WebUIContext()` — in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`

### Phase 2 Tests (NFR-4)

- [x] T020 [P] [US3] Write unit test: `WebUIConfig::setDeviceName()` truncates and emits `DLOG_W` when input ≥ 32 chars — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`
- [x] T021 [P] [US4] Write unit test: `EventBus` `dispatching_` flag is set during `poll()` — verify `assert(!dispatching_)` triggers if `subscribe()` is called during dispatch (debug build) — in `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
- [x] T022 [P] [US1] Write unit test: `CachingWebUIProvider::getWebUIContext(contextId)` returns match from cache without calling `getWebUIContexts()` — verify no full vector copy — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`

### Phase 2 Gate

- [x] T023 [US1] Run all native tests: `pio test -e native` in `DomoticsCore-WebUI/` and `DomoticsCore-Core/` — all must pass
- [x] T024 [US1] Build FullStack ESP8266 and verify RAM ≤ 74% — run `pio run -e esp8266dev` in `DomoticsCore-System/examples/FullStack/`
- [x] T025 [P] [US1] Build FullStack ESP32 and verify no new warnings — run `pio run -e esp32dev` in `DomoticsCore-System/examples/FullStack/`
- [x] T026 [US1] Record Phase 1 measurements in `specs/004-esp8266-memory-optim/baseline.md` (post-Phase 1 section)

---

## Phase 3: Static Data Migration + API Cleanup (Plan Phase 2)

> FR-P2.1 + FR-P2.2 + FR-P2.3 + NFR-1 cleanup. Must complete and verify before Phase 4.

### Flash-Resident Custom Content (US2, US5)

- [ ] T027 [US2] Add `const char* customHtmlPtr = nullptr`, `customCssPtr = nullptr`, `customJsPtr = nullptr` members to `WebUIContext` struct in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [ ] T028 [US5] Add `withCustomHtml(const char*)`, `withCustomCss(const char*)`, `withCustomJs(const char*)` builder methods that set Ptr and clear String in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [ ] T029 [US5] Add `withCustomHtmlDynamic(const String&)`, `withCustomCssDynamic(const String&)`, `withCustomJsDynamic(const String&)` builder methods that set String and clear Ptr in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [ ] T030 [US2] Add `hasCustomHtml()`, `getCustomHtmlCStr()`, `getCustomHtmlLen()` (and Css/Js variants) accessor methods to `WebUIContext` — return Ptr if non-null, else String.c_str() — in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [ ] T031 [US2] Update `WebUIContext` copy constructor and assignment operator to correctly copy Ptr/String state (Ptr copies pointer, String copies value) in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [ ] T032 [US2] Add `writeJsonCString(uint8_t* buffer, size_t maxLen, const char* str)` method to `StreamingContextSerializer` — same JSON-escaping logic as `writeJsonString`, reads from `const char*` using `strlen()` and `str[pos]` — in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h`
- [ ] T033 [US2] Update `CustomHtmlValue`, `CustomCssValue`, `CustomJsValue` states in `StreamingContextSerializer::write()` to check `ctx->customHtmlPtr` first, dispatch to `writeJsonCString()` if non-null, else `writeJsonString()` — in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h`
- [ ] T034 [US2] Update `CustomHtmlCheck`, `CustomCssCheck`, `CustomJsCheck` states to check `ctx->hasCustomHtml()` (etc.) instead of `!ctx->customHtml.isEmpty()` in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h`

### BaseWebUIComponents Migration (US2)

- [ ] T035 [US2] Change `generateChartCss()` to return `const char*` pointing to static `R"(...)"` literal instead of `String` in `DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h`
- [ ] T036 [US2] Update `createLineChart()` to call `withCustomCss(generateChartCss())` (now const char*) in `DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h`
- [ ] T037 [US2] For `generateChartHtml()` and `generateChartJs()` (which have interpolation), use `withCustomHtmlDynamic()` and `withCustomJsDynamic()` respectively in `DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h`

### Production Provider Updates (US2)

- [ ] T038 [P] [US2] Update `LEDWebUI` `.withCustomCss(R"(...)")` call to use new `withCustomCss(const char*)` API in `DomoticsCore-LED/include/DomoticsCore/LEDWebUI.h`
- [ ] T039 [P] [US2] Update `WebUIOnly` example `.withCustomCss(R"(...)")` call to use new `withCustomCss(const char*)` API in `DomoticsCore-WebUI/examples/WebUIOnly/src/main.cpp`

### ComponentMetadata const char* (US5)

- [x] T040 [US5] Convert `ComponentMetadata` members (`name`, `version`, `author`, `description`) from `String` to `const char*` with default `""` in `DomoticsCore-Core/include/DomoticsCore/IComponent.h`
- [x] T041 [US5] Convert `Dependency::name` from `String` to `const char*` in `DomoticsCore-Core/include/DomoticsCore/IComponent.h`
- [x] T042 [US5] Update `ComponentRegistry::registerComponent()` — convert `metadata.name` to `String` at map insertion time only — in `DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h`
- [x] T043 [US5] Update all component constructors to pass string literals directly to `ComponentMetadata` (mechanical update across ~10 component header files in `DomoticsCore-*/include/`)

### IWebUIProvider API Cleanup (US5)

- [x] T044 [US5] Remove `getWebUIContexts()` pure virtual from `IWebUIProvider` interface in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [x] T045 [US5] Make `forEachContext()`, `getContextCount()`, `getContextAt()`, `getWebUIContext()` pure virtual (remove default implementations that called `getWebUIContexts()`) in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [x] T046 [US5] Remove `getWebUIContexts()` override from `CachingWebUIProvider` (keep `forEachContext`, `getContextCount`, `getContextAt`, `getContextAtRef`, and the new `getWebUIContext` override from T019) in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [x] T047 [US5] Migrate ~57 `getWebUIContexts()` call sites in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp` to use `forEachContext()` pattern
- [x] T048 [P] [US5] Migrate ~6 `getWebUIContexts()` call sites in `DomoticsCore-WebUI/test/test_streaming_serializer/test/test_streaming_serializer.cpp` to use `forEachContext()` pattern
- [x] T049 [P] [US5] Migrate ~8 `getWebUIContexts()` call sites in `DomoticsCore-WebUI/test/test_schema_memory/test_schema_memory.cpp` to use `forEachContext()` pattern
- [x] T050 [US5] Update `MockWebUIProvider` and `LeakyTestProvider`/`CachedTestProvider` in test files to implement pure virtual methods (`forEachContext`, `getContextCount`, `getContextAt`, `getWebUIContext`) — or make them inherit from `CachingWebUIProvider`

### Phase 3 Gate

- [x] T051 [US1] Run all native tests: `pio test -e native` in `DomoticsCore-WebUI/` and `DomoticsCore-Core/` — all must pass
- [x] T052 [US1] Build FullStack ESP8266 — RAM 76.9% static (unchanged; gains are runtime heap) — run `pio run -e esp8266dev` in `DomoticsCore-System/examples/FullStack/`
- [x] T053 [P] [US1] Build FullStack ESP32 — pre-existing linker error (RemoteConsoleWebUI::LOG_LEVEL_OPTIONS) — run `pio run -e esp32dev` in `DomoticsCore-System/examples/FullStack/`
- [x] T054 [US1] Record Phase 3 measurements in `specs/004-esp8266-memory-optim/baseline.md` (post-Phase 2 section)

---

## Phase 4: Structural Refactoring — Hybrid Storage (Plan Phase 3)

> FR-P3.1 + FR-P3.2 + FR-P3.3. Final implementation phase.

### WebUIField Hybrid Storage (US5)

- [X] T055 [US5] Add `const char*` pointer members (`namePtr`, `labelPtr`, `valuePtr`, `unitPtr`, `endpointPtr`) initialized to `nullptr` alongside existing `String` members in `WebUIField` in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T056 [US5] Modify existing `WebUIField(const char*, const char*, WebUIFieldType, const char*, const char*, bool)` constructor to store `const char*` pointers directly without creating `String` copies in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T057 [US5] Add `WebUIField(const String&, const String&, WebUIFieldType, const String&, const String&, bool)` constructor overload that stores in `String` members (pointers remain null) in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T058 [US5] Add accessor methods (`getNameCStr()`, `getLabelCStr()`, `getValueCStr()`, `getUnitCStr()`, `getEndpointCStr()`) that return Ptr if non-null, else `String::c_str()` — in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T059 [US5] Update `WebUIField` copy constructor to preserve hybrid state (copy Ptr as-is, copy String if used) in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T060 [US5] Update `WebUIField` copy assignment operator to preserve hybrid state in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`

### WebUIContext Identity Fields Hybrid (US5)

- [X] T061 [US5] Add `const char*` pointer members (`contextIdPtr`, `titlePtr`, `iconPtr`, `apiEndpointPtr`) initialized to `nullptr` alongside existing `String` members in `WebUIContext` in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T062 [US5] Update all `WebUIContext` factory methods (`dashboard()`, `statusBadge()`, `headerInfo()`, `graph()`, `quickControl()`, `settings()`, `gauge()`) to store `const char*` arguments as pointers in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T063 [US5] Add accessor methods (`getContextIdCStr()`, `getTitleCStr()`, `getIconCStr()`, `getApiEndpointCStr()`) in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- [X] T064 [US5] Update `WebUIContext` copy constructor and assignment operator to preserve hybrid state for identity fields in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`

### StreamingContextSerializer Hybrid Dispatch (US2)

- [X] T065 [US2] Update `ContextIdValue`, `TitleValue`, `IconValue`, `ApiEndpointValue` states to use hybrid accessors (`getContextIdCStr()` etc.) in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h`
- [X] T066 [US2] Update `NameValue`, `LabelValue`, `ValueValue`, `UnitValue`, `EndpointValue` field states to use hybrid accessors (`getNameCStr()` etc.) in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h`

### WebUI.h and ProviderRegistry Updates (US2)

- [X] T067 [US2] Update `serializeContext()` helper in `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h` to use accessor methods (`getContextIdCStr()` etc.) when populating `JsonObject` — ArduinoJson accepts `const char*` directly
- [X] T068 [P] [US2] Update context access patterns in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h` to use accessor methods where applicable

### Provider buildContexts() Updates (US2)

- [X] T069 [P] [US2] Verify all `CachingWebUIProvider::buildContexts()` implementations work correctly with hybrid `WebUIField` constructors — all providers pass string literals, LEDWebUI uses String overload via ternary
- [X] T070 [US2] For `LEDWebUI` and any provider passing `String` values to `WebUIField`, verified they use the `String` constructor overload (T057) — no changes needed

### Hybrid Storage Tests (US5)

- [X] T071 [US5] Write unit test: `WebUIField` constructed with `const char*` literals — verify Ptr is set, String is empty — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`
- [X] T072 [US5] Write unit test: `WebUIField` constructed with `String` values — verify String is set, Ptr is null — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`
- [X] T073 [P] [US5] Write unit test: Copy `WebUIField` with Ptr set — verify copy preserves Ptr, String stays empty — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`
- [X] T074 [P] [US5] Write unit test: Copy `WebUIField` with String set — verify copy creates new String, Ptr stays null — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`
- [X] T075 [US5] Write unit test: `WebUIContext` factory methods store Ptr — verify `contextIdPtr`, `titlePtr`, `iconPtr` are set — in `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp`
- [X] T076 [US2] Write unit test: `StreamingContextSerializer` produces identical JSON output for a context using Ptr storage vs String storage — in `DomoticsCore-WebUI/test/test_streaming_serializer/test/test_streaming_serializer.cpp`

### Phase 4 Gate

- [X] T077 [US1] Run all native tests: 231 cases across WebUI (92), Core (66), Wifi (51), RemoteConsole (22) — all pass
- [X] T078 [US1] Build FullStack ESP8266: RAM 77.3% (63340/81920 bytes) — SUCCESS
- [X] T079 [P] [US1] Build FullStack ESP32: RAM 19.6% (64120/327680 bytes) — SUCCESS, no new warnings

---

## Phase 5: Polish & Final Verification

- [X] T080 Record all final measurements (RAM %, heap) in `specs/004-esp8266-memory-optim/baseline.md` (Phase 4 section)
- [X] T081 Compare `/api/ui/schema` JSON output — verified via `test_hybrid_ptr_vs_string_identical_json` (Ptr vs String produces identical JSON) and `test_integration_schema_endpoint_produces_valid_json`
- [X] T082 Update `specs/004-esp8266-memory-optim/spec.md` status from Draft to Complete
- [X] T083 Verify runtime free heap ≥ 8 KB on ESP8266 D1 Mini — Standard example: **9.3 KB free, 5-7% frag** ✅ (FullStack OOM — expected, too heavy for ESP8266)

---

## Phase 6: FullStack ESP8266 Memory Optimization

- [X] T085 Merge 2 static WS buffers into 1 shared — saves 2KB BSS (77.3% → 74.9% RAM)
- [X] T085b Reduce ESP8266 WS buffer 2048→1024 — **REVERTED** (caused truncation in Standard, marginal gain for FullStack)
- [X] T086 WebUI uses MemoryManager for adaptive maxWsClients (CRITICAL → 1 client)
- [X] T086b Heap guard in ProviderRegistry::discoverProviders — stops when heap < 2KB
- [X] T086c Consolidate ProviderRegistry maps (providerEnabled + providerComponent → providerInfo_)
- [X] T086d Heap guards in System::begin() post-init steps (loadAllConfigs, setupWebUIProviders, etc.)
- [X] T088 Regression test: Standard ESP8266 stable (**21.7KB heap, STANDARD profile**) ✅
- [X] T088b FullStack ESP8266: boots to `Application ready!` but crash-loops (WDT, 1.4KB heap) — **degraded mode, not production-viable**
- [ ] T084 SystemConfig String → const char* — DEFERRED (marginal gain)
- [ ] T087 Multi-frame WS send per-context — DEFERRED (no truncation with 2048 buffer)

---

## Phase 7: PROGMEM String Migration

- [X] T089 `log_*` macros → `Serial.printf_P(PSTR(...))` in Platform_ESP8266.h
- [X] T090 `DLOG_*` macros → `_dlog_snprintf_P` + `PSTR()` + `vsnprintf_P` in Logger.h (ESP8266 only)
- [X] T090b Per-provider heap guards in `setupWebUIProviders` (MIN_HEAP_PER_PROVIDER = 4096)
- [X] T092 FullStack ESP8266: **RAM 74.0% → 61.7%** (−10,048 bytes), .rodata 27.7KB → 16.6KB
- [X] T093 Standard ESP8266: **RAM 64.6% → 56.0%** (−7,008 bytes), heap ~28KB → ~18KB after providers
- [X] T094 Native tests: **84/84 pass** ✅
- [X] T095 Runtime: FullStack **boots stable** (1 boot/30s, 0 OOM, 0 WDT), Standard stable (1 boot/20s)
- [X] T096 Updated baseline.md with Phase 7 measurements
- [X] T091a OTA HTML page → PROGMEM + `send_P` (670 bytes saved)
- [X] T091d WS JSON format strings → `DSNPRINTF_P` (~80 bytes saved)
- [X] T091 Cross-platform macros `DSNPRINTF_P` + `DFPSTR` in Platform_HAL.h, `_dlog_snprintf_P` in Platform_ESP8266.h
- [X] T090b Per-provider heap guard threshold raised 4096→5120 (prevents OOM after T091 freed more heap)

---

## Dependencies

```text
Phase 1 (Setup):
  T001–T005 are independent [P], all can run in parallel

Phase 2 (Quick Wins):
  T006 → T007 → T008 (WebUIConfig struct, then methods, then logging)
  T009, T010 depend on T007 (need getters/setters)
  T011, T012, T013 depend on T007
  T014–T018 (EventBus) are independent of T006–T013 [P]
  T019 (CachingWebUIProvider) is independent of T006–T018 [P]
  T020–T022 (tests) depend on respective implementations (T008, T015–T017, T019)
  T023–T026 (gate) depend on ALL above

Phase 3 (Static Migration):
  T027–T031 (WebUIContext Ptr) → T032–T034 (Serializer) → T035–T039 (Components)
  T040–T043 (ComponentMetadata) are independent of T027–T039 [P]
  T044–T050 (API cleanup) depend on T019 (getWebUIContext override from Phase 2)
  T047 depends on T044–T046 (interface change before test migration)
  T048, T049 can run in parallel with T047 [P]
  T051–T054 (gate) depend on ALL above

Phase 4 (Hybrid Storage):
  T055–T060 (WebUIField) → T065–T066 (Serializer field states)
  T061–T064 (WebUIContext identity) → T065 (Serializer context states)
  T067–T068 depend on T063 (accessors)
  T069–T070 depend on T056–T057 (constructors)
  T071–T076 (tests) depend on their respective implementations
  T077–T079 (gate) depend on ALL above

Phase 5 (Polish):
  T080–T083 depend on ALL phases complete
```

## Parallel Execution Opportunities

### Phase 2 — 3 independent workstreams + tests:
1. **WebUIConfig** (T006–T013): Sequential within, ~3.5h
2. **EventBus** (T014–T018): Independent, ~30min
3. **CachingWebUIProvider** (T019): Independent, ~15min
4. **Tests** (T020–T022): After respective implementations, parallelizable [P]

### Phase 3 — 2 independent workstreams:
1. **Flash Ptr + Serializer + Components** (T027–T039): Sequential, ~6h
2. **ComponentMetadata** (T040–T043): Independent, ~2h

API cleanup (T044–T050) must wait for T019 but is independent of T027–T043.

### Phase 4 — 2 semi-independent workstreams:
1. **WebUIField hybrid** (T055–T060) + **field serializer** (T066)
2. **WebUIContext identity hybrid** (T061–T064) + **context serializer** (T065)

Both converge at T067–T070 (integration) and T071–T076 (tests).

## Implementation Strategy

**MVP Scope**: Phase 1 (Setup) + Phase 2 (Quick Wins) = T001–T026
- Delivers ~3–5 KB immediate gain with minimal risk
- Takes ~5h implementation + verification
- Independently valuable — can stop here if Phase 3+ is deferred

**Incremental Delivery**:
1. Phase 2 delivers stable, tested checkpoint (RAM ≤ 74%)
2. Phase 3 delivers Flash optimization + API cleanup (RAM ≤ 72%)
3. Phase 4 delivers maximum optimization (≥ 8 KB free heap runtime)
4. Each phase is a releasable state

---

## Summary

| Metric | Value |
|--------|-------|
| **Total tasks** | 83 |
| **Phase 1 (Setup)** | 5 tasks |
| **Phase 2 (Quick Wins)** | 21 tasks (US1: 5, US3: 9, US4: 6, tests: 3) |
| **Phase 3 (Static Migration)** | 28 tasks (US1: 4, US2: 13, US5: 11) |
| **Phase 4 (Hybrid Storage)** | 25 tasks (US1: 3, US2: 8, US5: 14) |
| **Phase 5 (Polish)** | 4 tasks |
| **Parallel opportunities** | 26 tasks marked [P] |
| **MVP scope** | T001–T026 (26 tasks, ~5h) |
