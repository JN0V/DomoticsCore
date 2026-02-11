# Implementation Plan: ESP8266 Memory Optimization (3 Phases)

**Branch**: `004-esp8266-memory-optim` | **Date**: 2025-01-20 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification + research from `/specs/004-esp8266-memory-optim/`

## Summary

Structured 3-phase optimization reducing ESP8266 RAM usage by ~11–17 KB through:
1. Config char[] migration, EventBus fix, CachingWebUIProvider bug fix
2. Flash-resident static content, ComponentMetadata const char*, old API removal
3. Hybrid const char*/String storage for WebUIField/WebUIContext

**Approach**: Strict phase gates — each phase completed, tested, and measured before the next begins.

## Technical Context

**Language/Version**: C++17 (ESP8266), C++14 (ESP32)
**Primary Dependencies**: PlatformIO, ESP32Async/ESPAsyncWebServer ^3.8.0, bblanchon/ArduinoJson ^7.0.4
**Storage**: LittleFS (ESP8266), NVS/Preferences (ESP32)
**Testing**: PlatformIO Unity framework (`pio test -e native`)
**Target Platform**: ESP8266 (primary, d1_mini), ESP32 (secondary)
**Project Type**: Embedded library (multi-component monorepo)
**Current RAM usage**: FullStack 75.4%, Standard 64.4%
**Constraint**: No behavioral regression — JSON output must be byte-identical

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Checkpoint | Status |
|-----------|------------|--------|
| **I. SOLID** | SRP: Each optimization is self-contained? | ✅ |
| **I. SOLID** | Dependency Inversion: IWebUIProvider interface preserved (Phase 1-2), refined (Phase 2 cleanup)? | ✅ |
| **II. TDD** | Test plan defined for each phase? | ✅ |
| **II. TDD** | Existing tests must pass at each gate? | ✅ |
| **III. KISS** | Simplest solution chosen? No over-engineering? | ✅ char[] pattern reused, const ref is 1-line |
| **IV. YAGNI** | No speculative features? Only what's needed now? | ✅ ArduinoJson static reuse abandoned |
| **V. Performance** | Memory budget considered? Binary size estimated? | ✅ Per-phase RAM targets defined |
| **VI. EventBus** | Inter-component communication via EventBus only? | ✅ EventBus optimized, not replaced |
| **VII. File Size** | All files < 800 lines? | ✅ No new files, changes within existing |
| **VIII. Progressive** | One phase at a time? Compatibility preserved? | ✅ Strict phase gates |
| **IX. HAL** | Hardware-specific code in `*_HAL` files only? | ✅ No new HAL splits needed |
| **X. NonBlockingTimer** | No delay()? | ✅ N/A — no timing changes |
| **XI. Storage** | No direct Preferences access? | ✅ N/A |
| **XII. Multi-Registry** | Compatible with PlatformIO AND Arduino registries? | ✅ No dependency changes |
| **XIII. Anti-Pattern** | No singletons, god objects, circular deps? | ✅ |
| **XIV. Versioning** | Version consistency? | ✅ No version bump needed for internal optimization |

**Embedded Constraints Check:**
- [x] Flash target: No increase expected (code size neutral or slightly smaller)
- [x] RAM target: Phase 1 ≤74%, Phase 2 ≤72%, Runtime ≥8KB free heap
- [x] Loop latency: No impact (const ref is faster, not slower)
- [x] No blocking operations introduced

## Project Structure

### Documentation (this feature)

```text
specs/004-esp8266-memory-optim/
├── spec.md              # Feature specification
├── plan.md              # This file
├── research.md          # Phase 0 output (unknowns resolved)
├── data-model.md        # Phase 1 output (entity changes)
├── quickstart.md        # Phase 1 output (execution guide)
├── baseline.md          # Pre-Phase 1 measurements (to be captured)
├── checklists/
│   └── requirements.md  # Spec quality checklist
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code Changes (by phase)

```text
Phase 1 (~5 files):
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h        # char[] migration
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI.h                    # config access updates
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h   # config copy usage
├── DomoticsCore-Core/include/DomoticsCore/EventBus.h                  # const ref + dispatching_
├── DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h           # getWebUIContext() fix
├── DomoticsCore-System/include/DomoticsCore/System.h                  # config init
└── DomoticsCore-System/include/DomoticsCore/SystemWebUISetup.h        # deviceName callback

Phase 2 (~10 files):
├── DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h           # Flash Ptr + interface cleanup
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h  # writeJsonCString
├── DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h      # static return types
├── DomoticsCore-Core/include/DomoticsCore/IComponent.h                # metadata const char*
├── DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h         # map insertion update
├── DomoticsCore-LED/include/DomoticsCore/LEDWebUI.h                   # withCustomCss update
├── DomoticsCore-WebUI/examples/WebUIOnly/src/main.cpp                 # withCustomCss update
├── DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp  # ~57 migrations
├── DomoticsCore-WebUI/test/test_streaming_serializer/...              # ~6 migrations
└── DomoticsCore-WebUI/test/test_schema_memory/...                     # ~8 migrations

Phase 3 (~15 files):
├── DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h           # hybrid WebUIField/Context
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h  # hybrid dispatch
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI.h                    # serializeContext update
├── DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h   # context access update
├── All *WebUI.h provider files                                        # constructor updates
└── All test files                                                     # constructor updates
```

## Implementation Phases

### Phase 0: Baseline (Pre-implementation)

| Task | Output |
|------|--------|
| Build FullStack ESP8266, record RAM/Flash % | baseline.md |
| Build Standard ESP8266, record RAM/Flash % | baseline.md |
| Capture runtime heap (if device available) | baseline.md |
| Save `/api/ui/schema` JSON snapshot | baseline.md |

### Phase 1: Quick Wins

| # | Task | FR | Files | Est. |
|---|------|----|-------|------|
| 1.1 | Convert WebUIConfig to char[] with getters/setters + DLOG_W truncation | FR-P1.1 | WebUIConfig.h | 1h |
| 1.2 | Update all config access sites in WebUI.h | FR-P1.1c | WebUI.h | 1h |
| 1.3 | Update WebSocketHandler.h config usage | FR-P1.1c | WebSocketHandler.h | 30m |
| 1.4 | Update System.h + SystemWebUISetup.h config init | FR-P1.1c | System.h, SystemWebUISetup.h | 30m |
| 1.5 | EventBus: const auto& + dispatching_ flag + assert | FR-P1.2 | EventBus.h | 30m |
| 1.6 | CachingWebUIProvider: override getWebUIContext() | FR-P1.3 | IWebUIProvider.h | 15m |
| 1.7 | Run all tests, verify, measure build size | Gate | — | 30m |

**Phase 1 total**: ~4.5h

### Phase 2: Static Data Migration + API Cleanup

| # | Task | FR | Files | Est. |
|---|------|----|-------|------|
| 2.1 | Add customHtml/Css/JsPtr to WebUIContext + builders | FR-P2.1a-c | IWebUIProvider.h | 1h |
| 2.2 | Add writeJsonCString() to StreamingContextSerializer | FR-P2.1e | StreamingContextSerializer.h | 1h |
| 2.3 | Update serializer states for Ptr/String dispatch | FR-P2.1d | StreamingContextSerializer.h | 1h |
| 2.4 | Migrate BaseWebUIComponents (CSS static, JS template) | FR-P2.2 | BaseWebUIComponents.h | 1.5h |
| 2.5 | Update LEDWebUI + examples for new API | FR-P2.2 | LEDWebUI.h, main.cpp | 30m |
| 2.6 | Convert ComponentMetadata + Dependency to const char* | FR-P2.3 | IComponent.h, ComponentRegistry.h | 1h |
| 2.7 | Update all component constructors for metadata | FR-P2.3 | ~10 component headers | 1h |
| 2.8 | Remove getWebUIContexts(), make forEachContext pure virtual | NFR-1 | IWebUIProvider.h | 1h |
| 2.9 | Migrate test files (~70 call sites) | NFR-1 | 3 test files | 2h |
| 2.10 | Run all tests, verify, measure build size | Gate | — | 30m |

**Phase 2 total**: ~10.5h

### Phase 3: Structural Refactoring

| # | Task | FR | Files | Est. |
|---|------|----|-------|------|
| 3.1 | Add hybrid Ptr/String members to WebUIField + constructors | FR-P3.1 | IWebUIProvider.h | 2h |
| 3.2 | Update WebUIField copy/assignment for hybrid state | FR-P3.3 | IWebUIProvider.h | 1h |
| 3.3 | Add hybrid Ptr members to WebUIContext identity fields | FR-P3.1 | IWebUIProvider.h | 1h |
| 3.4 | Update WebUIContext copy/assignment + factory methods | FR-P3.3 | IWebUIProvider.h | 1h |
| 3.5 | Update StreamingContextSerializer for all hybrid fields | FR-P3.2 | StreamingContextSerializer.h | 2h |
| 3.6 | Update serializeContext() in WebUI.h | FR-P3.2 | WebUI.h | 1h |
| 3.7 | Update ProviderRegistry context access | FR-P3.2 | ProviderRegistry.h | 30m |
| 3.8 | Update all WebUI provider buildContexts() | FR-P3.1 | ~8 provider files | 2h |
| 3.9 | Write new unit tests for hybrid copy/assign/move | FR-P3.3 | test files | 2h |
| 3.10 | Run all tests, verify, measure build size + runtime heap | Gate | — | 1h |

**Phase 3 total**: ~13.5h

## Dependency Graph

```
Phase 0: Baseline
    │
    ▼
Phase 1: Quick Wins ──────────────────────────────────────────
    │  1.1 WebUIConfig ──► 1.2 WebUI.h ──► 1.3 WSHandler
    │       │                                    │
    │       └──► 1.4 System.h ◄──────────────────┘
    │  1.5 EventBus (independent)
    │  1.6 CachingWebUIProvider (independent)
    │       │
    │       ▼
    │  1.7 Gate Check
    │
    ▼
Phase 2: Static Migration + Cleanup ──────────────────────────
    │  2.1 WebUIContext Ptr ──► 2.2 writeJsonCString
    │       │                        │
    │       └──► 2.3 Serializer dispatch ◄──┘
    │                │
    │       2.4 BaseWebUIComponents ──► 2.5 LED/examples
    │       2.6 ComponentMetadata ──► 2.7 All components
    │       2.8 Remove getWebUIContexts ──► 2.9 Test migration
    │       │
    │       ▼
    │  2.10 Gate Check
    │
    ▼
Phase 3: Hybrid Storage ──────────────────────────────────────
    │  3.1 WebUIField hybrid ──► 3.2 Copy/assign
    │  3.3 WebUIContext hybrid ──► 3.4 Copy/assign + factories
    │       │                           │
    │       └──► 3.5 Serializer ◄───────┘
    │                │
    │       3.6 WebUI.h ──► 3.7 ProviderRegistry
    │       3.8 All providers
    │       3.9 New tests
    │       │
    │       ▼
    │  3.10 Final Gate Check
```

## Risk Tracking

| Risk | Phase | Mitigation | Owner |
|------|-------|------------|-------|
| char[] truncation data loss | P1 | DLOG_W + generous sizes | Dev |
| EventBus reentrancy | P1 | assert(!dispatching_) in debug | Dev |
| Dangling const char* pointer | P2/P3 | API naming (withCustomHtml vs withCustomHtmlDynamic) | Dev |
| Hybrid copy/assignment bugs | P3 | Dedicated unit tests (3.9) before gate | Dev |
| Test migration breaks | P2 | Mechanical replacement, run after each batch | Dev |
| JSON output difference | P2/P3 | Diff `/api/ui/schema` against baseline snapshot | Dev |

## Complexity Tracking

> No constitution violations identified. All principles can be followed.
> ArduinoJson 7 static reuse abandoned after research (YAGNI).
> Old API directly removed instead of deprecated (sole developer).
