# Baseline Measurements: ESP8266 Memory Optimization

**Feature**: 004-esp8266-memory-optim
**Date**: 2025-01-20
**Branch**: `004-esp8266-memory-optim`

---

## Pre-Phase 1 Baseline

### Build Sizes

| Example | Target | RAM | RAM bytes | Flash | Flash bytes |
|---------|--------|-----|-----------|-------|-------------|
| FullStack | esp8266dev | **76.9%** | 62972 / 81920 | 65.9% | 688707 / 1044464 |
| Standard | esp8266dev | **66.2%** | 54260 / 81920 | 50.0% | 521775 / 1044464 |
| FullStack | esp32dev | **FAILED** | — | — | — |

**ESP32 build failure**: Pre-existing linker error — `undefined reference to DomoticsCore::Components::WebUI::RemoteConsoleWebUI::LOG_LEVEL_OPTIONS`. Not related to this feature.

### Native Tests

| Library | Tests | Status |
|---------|-------|--------|
| DomoticsCore-WebUI | 82 test cases | ✅ ALL PASSED |
| DomoticsCore-Core | 65 test cases | ✅ ALL PASSED |
| **Total** | **147 test cases** | **✅ ALL PASSED** |

### Runtime Measurements

*(Requires device — deferred to on-device testing)*

- Free heap at boot: TBD
- Free heap after init: TBD
- Free heap steady-state (2 WS clients): TBD

### JSON Schema Snapshot

*(Requires running device or test harness — deferred to T081)*

---

## Post-Phase 1 (Quick Wins)

**Date**: 2025-01-20

### Build Sizes

| Example | Target | RAM | RAM bytes | Flash | Flash bytes | Delta vs Baseline |
|---------|--------|-----|-----------|-------|-------------|-------------------|
| FullStack | esp8266dev | **76.9%** | 62972 / 81920 | 65.9% | 688707 / 1044464 | 0 bytes |

**Note**: Static (linker-reported) RAM is unchanged. The WebUIConfig char[] migration eliminates **heap fragmentation** at runtime — String objects performed malloc/free on config copy, now struct copy is a flat memcpy. EventBus const ref avoids per-poll vector copies (~3 × sizeof(vector<Subscription>)). These gains are **runtime heap**, not static RAM.

### Native Tests

| Library | Tests | Status |
|---------|-------|--------|
| DomoticsCore-WebUI | 84 test cases (+2 new) | ✅ ALL PASSED |
| DomoticsCore-Core | 66 test cases (+1 new) | ✅ ALL PASSED |
| **Total** | **150 test cases** | **✅ ALL PASSED** |

### Changes Applied
- **T006-T008**: WebUIConfig 6×String → char[] with getters/setters + DLOG_W truncation
- **T009-T013**: Updated all config access sites (WebUI.h, System.h, SystemPersistence.h)
- **T014-T018**: EventBus const ref iteration + dispatching_ guard + assert
- **T019**: CachingWebUIProvider getWebUIContext() already present
- **T020**: Test: WebUIConfig truncation
- **T021**: Test: EventBus publish during dispatch (const ref safe)
- **T022**: Test: CachingWebUIProvider getWebUIContext cache lookup

---

## Post-Phase 2 (Static Migration) — Phase 3 complete

**Date**: 2025-01-21

### Build Sizes

| Example | Target | RAM | RAM bytes | Flash | Flash bytes | Delta vs Baseline |
|---------|--------|-----|-----------|-------|-------------|-------------------|
| FullStack | esp8266dev | **76.9%** | 62972 / 81920 | 65.9% | 688707 / 1044464 | 0 bytes |
| FullStack | esp32dev | **FAILED** | — | — | — | Pre-existing linker error |

**Note**: Static (linker-reported) RAM is unchanged as expected. Phase 3 gains are **runtime heap**:
- `ComponentMetadata` members (`name`, `version`, `author`, `description`) changed from `String` to `const char*` — eliminates 4 heap allocations per component instance
- `Dependency::name` changed from `String` to `const char*` — eliminates heap allocation per dependency
- `getWebUIContexts()` removed from `IWebUIProvider` — no more `std::vector<WebUIContext>` copies on every API call
- All providers now use `forEachContext()` / `getContextCount()` / `getContextAtRef()` — zero-copy iteration

### Native Tests

| Library | Tests | Status |
|---------|-------|--------|
| DomoticsCore-WebUI (test_webui_component) | 84 test cases | ✅ ALL PASSED |
| DomoticsCore-WebUI (test_streaming_serializer) | 7 test cases | ✅ ALL PASSED |
| DomoticsCore-Core | 66 test cases | ✅ ALL PASSED |
| DomoticsCore-Wifi | 51 test cases | ✅ ALL PASSED |
| DomoticsCore-MQTT | 25 test cases | ✅ ALL PASSED |
| DomoticsCore-NTP | 34 test cases | ✅ ALL PASSED |
| DomoticsCore-OTA | 29 test cases | ✅ ALL PASSED |
| DomoticsCore-HomeAssistant | 31 test cases | ✅ ALL PASSED |
| DomoticsCore-RemoteConsole | 22 test cases | ✅ ALL PASSED |
| DomoticsCore-SystemInfo | 45 test cases | ✅ ALL PASSED |
| DomoticsCore-Storage | 25 test cases | ✅ ALL PASSED |
| **Total** | **419 test cases** | **✅ ALL PASSED** |

### Changes Applied (T040-T051)
- **T040-T041**: `ComponentMetadata` + `Dependency` members from `String` to `const char*`
- **T042**: `ComponentRegistry::registerComponent()` converts to `String` only at map insertion
- **T043**: Updated all component constructors across ~10 modules
- **T044-T046**: Removed `getWebUIContexts()` from `IWebUIProvider`, made iterator methods pure virtual
- **T047-T049**: Migrated all test call sites to `forEachContext()` / `getContextCount()` / `getContextAtRef()`
- **T050**: Converted test providers (`MockWebUIProvider`, `LeakyTestProvider`, `CachedTestProvider`, `TestProvider`) to `CachingWebUIProvider`
- **T051**: All native tests passing across all modules

---

## Post-Phase 4 (Hybrid Storage) — Phase 4 complete

**Date**: 2025-01-21

### Build Sizes

| Example | Target | RAM | RAM bytes | Flash | Flash bytes | Delta vs Phase 3 |
|---------|--------|-----|-----------|-------|-------------|-------------------|
| FullStack | esp8266dev | **77.3%** | 63340 / 81920 | ~66% | — | +368 bytes static |
| FullStack | esp32dev | **19.6%** | 64120 / 327680 | 84.0% | 1321801 / 1572864 | ✅ SUCCESS |

**Note**: Static RAM increased +368 bytes due to new `const char*` pointer members in `WebUIField` (5 ptrs × 4 bytes) and `WebUIContext` (4 ptrs × 4 bytes) structs. However, **runtime heap savings** are significant:
- `WebUIField` constructed with `const char*` literals → zero String heap allocation (saves ~5 Strings × ~12 bytes SSO = ~60 bytes per field)
- `WebUIContext` constructed with `const char*` literals → zero String heap for identity fields (saves ~4 Strings × ~20 bytes avg = ~80 bytes per context)
- With ~30 fields and ~10 contexts in FullStack → estimated **~2.6 KB runtime heap saved**
- All serialization paths (`StreamingContextSerializer`, `serializeContext()`, `ProviderRegistry`) use hybrid accessors — no intermediate String creation

### Native Tests

| Library | Tests | Status |
|---------|-------|--------|
| DomoticsCore-WebUI (test_webui_component) | 94 test cases | ✅ ALL PASSED |
| DomoticsCore-WebUI (test_streaming_serializer) | 8 test cases | ✅ ALL PASSED |
| DomoticsCore-Core | 66 test cases | ✅ ALL PASSED |
| DomoticsCore-Wifi | 51 test cases | ✅ ALL PASSED |
| DomoticsCore-RemoteConsole | 22 test cases | ✅ ALL PASSED |
| **Total** | **241 test cases** | **✅ ALL PASSED** |

### Changes Applied (T055-T079)
- **T055-T060**: `WebUIField` hybrid `const char*`/`String` storage with pointer members, dual constructors, accessors, copy semantics
- **T061-T064**: `WebUIContext` identity fields (`contextId`, `title`, `icon`, `apiEndpoint`) hybrid storage with pointer members, dual constructors/factories, accessors
- **T065-T066**: `StreamingContextSerializer` updated to use hybrid accessors for all context and field string values
- **T067-T068**: `WebUI.h` `serializeContext()` and `ProviderRegistry.h` updated to use hybrid accessors
- **T069-T070**: All provider `buildContexts()` verified — string literals use zero-heap Ptr path, dynamic values (LEDWebUI) use String path
- **T071-T076**: 11 new hybrid storage tests (10 in test_webui_component, 1 in test_streaming_serializer)
- **T077-T079**: Phase 4 gate passed — all native tests, ESP8266 and ESP32 builds successful
- **Bonus fixes**: `config.deviceName.c_str()` → direct char[] usage in WebUI.h, `cfg.password.length()` → `strlen()` in SystemWebUISetup.h, ESP32 `RemoteConsoleWebUI::LOG_LEVEL_OPTIONS` ODR fix

---

## Final — Runtime Verification on ESP8266 (T083)

**Date**: 2025-01-21
**Device**: D1 Mini (ESP8266), `/dev/ttyUSB0`
**Firmware**: Standard example (WiFi + LED + RemoteConsole + WebUI + NTP + Storage)

### Runtime Heap Measurements (AP mode, 0 WebSocket clients)

| Metric | Value | Target | Status |
|--------|-------|--------|--------|
| Free heap | **9248–9312 bytes** | ≥ 8192 bytes (AC-1.3) | ✅ PASS |
| Heap fragmentation | **5–7%** | Low | ✅ PASS |
| Max free block | **8632–8872 bytes** | — | ✅ Good |
| Heap stability | Stable (no drift over 30s) | No leaks | ✅ PASS |

### FullStack Example (all 10 components) — Before Phase 6

| Metric | Value | Status |
|--------|-------|--------|
| Static RAM | **77.3%** (63340/81920) | ❌ Too high |
| Free heap at detect | **1744 bytes** | ❌ OOM crash at discoverProviders |
| Root cause | 2× static WS buffers (4KB BSS) + 10 components exhaust heap | Known limitation |

---

## Phase 6 — FullStack ESP8266 Memory Optimization

**Date**: 2025-01-21
**Device**: D1 Mini (ESP8266), `/dev/ttyUSB0`

### Optimizations Applied

| Optimization | BSS/Heap Savings | RAM % After |
|---|---|---|
| T085: Merge 2 static WS buffers → 1 shared | ~2KB BSS | 74.9% |
| T085b: Reduce ESP8266 WS buffer 2048→1024 | ~1KB BSS | 73.7% — **REVERTED** (caused truncation in Standard) |
| T086: MemoryManager adaptive WS clients | Runtime (CRITICAL→1 client) | — |
| T086b: Heap guard in discoverProviders | Prevents OOM in discovery | — |
| T086c: Consolidate ProviderRegistry maps | ~40 bytes/provider | — |
| T086d: Heap guards in System::begin() | Prevents OOM in post-init | 74.0% |

### FullStack ESP8266 — After Phase 6

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Static RAM | 77.3% (63340) | 75.0% (61440 est.) | **−1900 bytes** |
| Heap at detect | 1744 bytes | ~3600 bytes | **+1856 bytes** |
| Heap after init | OOM crash | ~1448 bytes | Now boots |
| Boot status | ❌ OOM crash | ⚠️ Boots in degraded mode | Improved |
| Loop stability | N/A | ❌ WDT crash-loop (~7s) | Not stable |

### Standard ESP8266 — Regression Check (Phase 6)

| Metric | Before Phase 6 | After Phase 6 | Status |
|--------|----------------|---------------|--------|
| Static RAM | ~64% | **64.6%** (52920) | ✅ OK |
| Heap at detect | ~9.3KB | **20.7KB** (STANDARD profile) | ✅ Improved |
| Heap after full init | ~9.3KB | **11.3KB** (after all providers) | ✅ PASS |
| Boot stability | Stable | **Stable (1 boot / 20s)** | ✅ PASS |
| WebUI providers | All registered | All registered | ✅ PASS |
| WS buffer truncation | None | **None** (buffer=2048) | ✅ PASS |

### Phase 6 Conclusion

- **Standard ESP8266**: Production-ready. 11.3KB free heap, stable, all WebUI providers work.
- **FullStack ESP8266**: Boots but WDT crash-loops with 1.4KB heap. Not production-viable.

---

## Phase 7 — PROGMEM String Migration (DLOG + log_*)

**Date**: 2025-01-21
**Device**: D1 Mini (ESP8266), `/dev/ttyUSB0`

### Root Cause

On ESP8266, `.rodata` (string literals, const data) is placed in **DRAM** (not Flash like ESP32).
Before Phase 7: `.rodata` = **27,692 bytes** = 33.8% of total 81,920 bytes RAM.

381 DLOG format strings alone = ~11.4KB in DRAM. Moving them to Flash via `PSTR()` + `vsnprintf_P` frees this RAM.

### Optimizations Applied

| Optimization | .rodata Savings | RAM % After |
|---|---|---|
| T089: `log_*` macros → `Serial.printf_P(PSTR(...))` | ~200 bytes | — |
| T090: `DLOG_*` macros → `_dlog_snprintf_P(PSTR(...))` | **~10,320 bytes** | 62.6% |
| T090b: Per-provider heap guards in `setupWebUIProviders` (5KB threshold) | Prevents OOM | — |
| T091a: OTA HTML page → `PROGMEM` + `send_P` | **670 bytes** | — |
| T091d: WS JSON format strings → `DSNPRINTF_P` | ~80 bytes | 61.7% |

### Section Breakdown (FullStack)

| Section | Phase 6 | Phase 7 | Delta |
|---------|---------|---------|-------|
| `.rodata` | 27,692 | **16,620** | **−11,072** |
| `.bss` | 31,216 | 32,240 | +1,024 (PSTR locals) |
| `.data` | 1,696 | 1,696 | 0 |
| `.irom0.text` | 629,756 | ~645,000 | +15,244 (strings in Flash) |
| **Total RAM** | 60,604 (74.0%) | **50,556 (61.7%)** | **−10,048** |

### FullStack ESP8266 — After Phase 7

| Metric | Phase 6 | Phase 7 | Delta |
|--------|---------|---------|-------|
| Static RAM | 74.0% (60604) | **61.7%** (50556) | **−10,048 bytes** |
| Heap at detect | ~3,600 | **14,664** | **+11,064** |
| Heap after discovery | OOM | **10,104** | Now works |
| Heap after providers | N/A | **4,584** (WiFi+NTP) | 2 providers registered |
| Providers skipped | All | MQTT, OTA, SysInfo, Console, HA | Heap guard at 5KB |
| Boot status | ❌ WDT crash-loop | ✅ **Stable** (`Application ready!`) | **Fixed** |
| Loop stability | ❌ WDT 7s | ✅ **Stable 30s+** (temp readings) | **Fixed** |
| WebUI accessible | No | ✅ `http://192.168.4.1:80` | **Working** |
| HA entities | No | ✅ 4 sensors registered | **Working** |

### Standard ESP8266 — Regression Check (Phase 7)

| Metric | Phase 6 | Phase 7 | Status |
|--------|---------|---------|--------|
| Static RAM | 64.6% (52920) | **56.0%** (45912) | ✅ **−7,008 bytes** |
| Heap at detect | 20.7KB | **~28KB** (STANDARD profile) | ✅ **+7KB** |
| Heap after full init | 11.3KB | **~18KB** (WiFi+NTP+Console) | ✅ **+6.7KB** |
| Boot stability | Stable | **Stable (1 boot / 20s)** | ✅ PASS |
| WebUI providers | All 3 registered | All 3 registered | ✅ PASS |
| WS buffer truncation | None | **None** | ✅ PASS |
| Native tests | 84/84 | **84/84** | ✅ PASS |

### Final Conclusion

- **Standard ESP8266**: Production-ready. **~18KB free heap**, stable, all WebUI providers work. Massive improvement from 9.3KB (Phase 5) → 18KB (Phase 7).
- **FullStack ESP8266**: **Now boots and runs stable!** 2/7 WebUI providers registered (WiFi + NTP), HA entities work, WebUI accessible. MQTT/OTA/SysInfo/Console WebUI providers skipped by heap guard (4.6KB remaining). Core functionality works; WebUI is partial.
- **Key optimization**: Moving DLOG format strings to PROGMEM freed **11KB of DRAM** (.rodata 27.7KB → 16.6KB) — the single most impactful change.
- **Cross-platform macros**: `DSNPRINTF_P` and `DFPSTR` defined in `Platform_HAL.h` for ESP8266 PROGMEM / ESP32 no-op.
- **Remaining .rodata**: 16.6KB still in DRAM (vtables ~4KB, component descriptions, JSON response strings ~1.5KB, other const data).
