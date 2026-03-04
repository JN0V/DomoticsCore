# DomoticsCore Documentation - Adversarial Review Findings

> Generated: 2026-03-04 | Review scope: All 39 documentation files + source code cross-reference
> Total findings: **104** (21 CRITICAL, 33 MAJOR, 27 MINOR, 23 ROADMAP)

---

## Executive Summary

The documentation was generated from a high-level understanding of the framework rather than from the actual source headers. As a result, **nearly every API signature in the global `technical-reference.md` is wrong** when cross-referenced against real code. Component-level docs are more accurate but still contain fabricated method signatures, dead config fields documented as functional, and missing `shrink_to_fit()` calls across the codebase.

The review also uncovered **significant code-level issues** (dead features, constitution violations, unimplemented methods) that should be tracked as roadmap items.

---

## Part 1: CRITICAL Findings (Must Fix)

### Documentation Accuracy — Global Docs

| # | File | Issue | Fix |
|---|------|-------|-----|
| C1 | technical-reference.md | `IComponent` interface is entirely fabricated — `getName()`, `isEnabled()`, `getState()`, `getOptionalDependencies()` don't exist. Actual API: `isActive()`, `getLastStatus()`, `getTypeKey()`, `on<T>()`, `emit<T>()` | Rewrite from actual `IComponent.h` |
| C2 | technical-reference.md | EventBus shown as static class (`EventBus::subscribe()`). It's instance-based via `core.getEventBus()` or component `on<T>()`/`emit<T>()`. Handler signature is `std::function<void(const void*)>`, not `const Event&` | Rewrite EventBus section |
| C3 | technical-reference.md | Event topics use dots (`wifi.connected`) — actual code uses slashes (`wifi/sta/connected`) | Replace all `.` with `/` in event names |
| C4 | technical-reference.md | `Timer` class doesn't exist — actual class is `NonBlockingDelay` | Replace `Timer` with `NonBlockingDelay` |
| C5 | technical-reference.md | `IWebUIProvider` interface fabricated — no `getProviderName()`, `getHeaderBadge()`, etc. Actual: `getWebUIName()`, `getWebUIData()`, `handleWebUIRequest()`, context-based system | Rewrite from actual `IWebUIProvider.h` |
| C6 | technical-reference.md + project-context.md | Logging macros `LOG_*` don't exist — actual: `DLOG_I()`, `DLOG_W()`, `DLOG_E()`, `DLOG_D()`, `DLOG_V()` | Replace `LOG_*` with `DLOG_*` everywhere |
| C7 | technical-reference.md | Logger API fabricated — no `Logger` class, no `Logger::setLevel()`, no `Logger::addCallback()`. Logging is macro-based, compile-time via `CORE_DEBUG_LEVEL` | Rewrite logging section |
| C8 | technical-reference.md | Storage API: `setString()` doesn't exist — actual: `putString()` | Replace `setString` with `putString` |
| C9 | technical-reference.md | `MemoryProfile::Constrained` doesn't exist — actual: `FULL`, `STANDARD`, `MINIMAL`, `CRITICAL` | Fix enum values |

### Documentation Accuracy — Component Docs

| # | Component | Issue | Fix |
|---|-----------|-------|-----|
| C10 | MQTT | `isValidTopic()` declared but NEVER implemented — linker error | Implement or remove declaration + doc refs |
| C11 | MQTT | `maxQueueSize`, `publishRateLimit`, `maxSubscriptions` config fields NEVER enforced | Implement enforcement or add bold warnings |
| C12 | MQTT | Header `@example` block references deleted `onMessage()` API | Update example to EventBus pattern |
| C13 | NTP | `retryDelayMs` config field declared but NEVER used | Implement or remove |
| C14 | OTA | `EVENT_START` and `EVENT_END` events NEVER emitted | Emit events or remove from docs |
| C15 | OTA | `Applying` state NEVER entered in code | Remove from state machine or implement |
| C16 | RemoteConsole | `requireAuth`/`password` config: authentication NEVER implemented | Implement or remove (security issue) |
| C17 | RemoteConsole | `allowCommands` config field NEVER checked | Implement guard |
| C18 | Storage | Quick start uses possibly non-existent `CoreConfig::deviceName` | Verify and fix example |
| C19 | System | `otaPassword` config field completely disconnected from OTA | Wire to OTAConfig or remove |
| C20 | WebUI | 3 files exceed 800-line limit: `WebUI.h` (951), `StreamingContextSerializer.h` (922), `Wifi.h` (881) | Split files per constitution |
| C21 | HA | `HAEntityAddedEvent` struct defined but EventBus emits `String` instead | Align struct with actual emission |

---

## Part 2: MAJOR Findings (Should Fix)

### Documentation Gaps

| # | Scope | Issue | Fix |
|---|-------|-------|-----|
| M1 | All docs | Missing namespace documentation — code in `DomoticsCore::`, `Components::`, `Utils::` — examples won't compile without `using` | Add namespace section |
| M2 | technical-reference.md | Lifecycle states diagram fabricated — no `ComponentState` enum exists | Document actual `active`/`ComponentStatus` model |
| M3 | technical-reference.md | `BaseWebUIComponents` class doesn't exist — actual: `WebUIField` + `WebUIContext` fluent builders | Rewrite WebUI components section |
| M4 | project-context.md | EventBus is queue-based with `poll()` dispatch + 32-event backpressure — not documented | Add queue/poll documentation |
| M5 | project-context.md | WebUI has 6 `WebUILocation` values, not 4 as documented | Update to 6 locations |
| M6 | project-context.md | `delay()` exception "except boot sequences" omitted | Add caveat |
| M7 | technical-reference.md | `HeapTracker` API fabricated — no `begin()`/`end()`. Actual: `checkpoint()` + assertion macros | Rewrite from source |
| M8 | README.md | "SSE real-time updates" — actual: WebSocket (with SSE fallback) | Correct terminology |

### Code Issues Found During Review

| # | Component | Issue | Fix |
|---|-----------|-------|-----|
| M9 | Core | `EventBus::reset()` doesn't clear `wildcardTopicSubscriptions`, `lastByTopic`, `pendingByTopic` | Fix reset() |
| M10 | Core | `unsubscribe()`/`unsubscribeOwner()` skip `wildcardTopicSubscriptions` — wildcards can't be cleaned up | Fix unsubscribe |
| M11 | Core | `Core::emit()` lacks `sticky` parameter unlike `IComponent::emit()` | Add sticky param or document difference |
| M12 | LED | `metadata.name = "LEDComponent"` — inconsistent with all other components (should be `"LED"`) | Rename to `"LED"` |
| M13 | MQTT | `MQTTStatistics.reconnectCount` incremented before attempt, not after failure — subtle semantics | Clarify in docs |
| M14 | Storage | `StorageWebUI::handleWebUIRequest()` returns `{success:false}` for ALL requests — read-only but not documented | Document read-only |
| M15 | Storage | No change events emitted (only `storage/ready`) — constitution XI requires change events | Design `storage/changed` event |
| M16 | System | `millis()` called directly in `getSystemStatus()` — bypasses HAL | Replace with `HAL::Platform::getMillis()` |
| M17 | System | States `WIFI_CONNECTING`/`WIFI_CONNECTED`/`SERVICES_STARTING` never entered during default boot — misleading docs | Clarify as "available for external use" |
| M18 | System | `StorageWebUI` never registered by `SystemWebUISetup` — unreachable in standard deployment | Add registration |
| M19 | HA | `addBinarySensor()` doesn't emit `EVENT_ENTITY_ADDED` — inconsistent with other add methods | Add event emission |

---

## Part 3: MINOR Findings

| # | Scope | Issue |
|---|-------|-------|
| m1 | technical-reference.md | Version table will become stale — no auto-generation mechanism |
| m2 | technical-reference.md | `SystemInfo_HAL.h` missing from HAL table |
| m3 | project-context.md | `shrink_to_fit()` guidance missing from technical-reference Memory section |
| m4 | Core tech-ref | `Core::loop()` "emits heartbeat" — actually just logs via `DLOG_D` |
| m5 | Core tech-ref | EventBus wildcards use `*` but MQTT uses `+`/`#` — confusion risk |
| m6 | Core tech-ref | `HAL::delay` in API list without constitution warning |
| m7 | LED tech-ref | `effectDirection` field is dead code — never read or written |
| m8 | LED project-ctx | `platformio.ini` listed but may not exist |
| m9 | NTP tech-ref | `isSynced()` threshold mismatch: component (2001) vs HAL (2020) |
| m10 | NTP tech-ref | `init()` method attributed to NTPComponent instead of NTPWebUI |
| m11 | OTA project-ctx | Line counts use `~` prefix — imprecise for monitoring |
| m12 | OTA tech-ref | `signaturePublicKey` field documented as functional but never used |
| m13 | RC tech-ref | Log format says "single-character" but NONE is 4 chars |
| m14 | RC project-ctx | `RemoteConsoleWebUI.h` missing from line count analysis |
| m15 | Storage project-ctx | Approximate line counts inaccurate (650 vs "~651") |
| m16 | System project-ctx | RemoteConsole dependency `>= 0.1.0` should be `>= 1.4.0` |
| m17 | HA project-ctx | `ha/entity_added` documented as emitted only by `addSensor()` — actually 4 of 5 types |
| m18 | WebUI tech-ref | Cargo-cult `shrinkToFit()` calls before JsonDocument goes out of scope |

---

## Part 4: ROADMAP — Code vs Constitution Gaps

These items represent areas where the **existing code** deviates from the **constitution's requirements**. The documentation should present best practices and flag these as known TODOs.

### Memory Safety (Constitution XIV — ABSOLUTE PRIORITY)

| # | Location | Issue | Action |
|---|----------|-------|--------|
| R1 | EventBus.h | `unsubscribe()`/`unsubscribeOwner()`/`reset()` — no `shrink_to_fit()` after erase/clear | Add calls |
| R2 | MQTT_impl.h | `messageQueue.erase()` in `processMessageQueue()` — no `shrink_to_fit()` | Add call |
| R3 | Storage.h | `cache.clear()` in shutdown/clear, `cache.erase()` in remove — no `shrink_to_fit()` | Add calls |
| R4 | RemoteConsole.h | `clients.erase()` on disconnect — no `shrink_to_fit()` | Add call |
| R5 | HAEntity.h | String concatenation in topic generation (4 concats x N entities during discovery) | Use `snprintf()` |
| R6 | HAConfig | 9 String fields cause heap fragmentation on ESP8266 — should use `char[]` | Refactor to fixed arrays |
| R7 | Multiple | String concatenation in log formatting hot paths (RemoteConsole, Storage, System) | Use `snprintf()` + static buffers |

### HAL Isolation (Constitution IX — NON-NEGOTIABLE)

| # | Location | Issue | Action |
|---|----------|-------|--------|
| R8 | System.h:530 | Direct `millis()` call bypassing HAL | Replace with `HAL::Platform::getMillis()` |
| R9 | RemoteConsole.h | `HAL::delay(100)` in reboot handler — blocking in non-boot code | Use non-blocking reboot flag |
| R10 | Storage.h:588 | `#if DOMOTICSCORE_WEBUI_ENABLED` in business logic file | Remove conditional block |

### File Size (Constitution VII — 800 lines max)

| # | File | Lines | Action |
|---|------|-------|--------|
| R11 | WebUI.h | 951 | Split into WebUI + WebUICore |
| R12 | StreamingContextSerializer.h | 922 | Extract serialization helpers |
| R13 | Wifi.h | 881 | Extract connection management |

### Anti-Patterns (Constitution XIII)

| # | Location | Issue | Action |
|---|----------|-------|--------|
| R14 | MemoryManager | Singleton pattern (constitution forbids) | Document exception or refactor |
| R15 | MQTT | Static `instance` pointer (singleton) | Evaluate alternatives |
| R16 | System.h | 21 `__has_include()` directives — grey area | Document as intentional deviation |

### Dead Code / YAGNI (Constitution IV)

| # | Location | Issue | Action |
|---|----------|-------|--------|
| R17 | MQTT.h | `isValidTopic()` declared, never implemented | Implement or remove |
| R18 | MQTT Config | `maxQueueSize`, `publishRateLimit`, `maxSubscriptions` — never enforced | Implement or remove |
| R19 | NTP Config | `retryDelayMs` — never used | Implement or remove |
| R20 | System Config | `otaPassword` — disconnected from OTA | Wire or remove |
| R21 | RemoteConsole Config | `requireAuth`, `password`, `allowCommands` — never enforced | Implement or remove |
| R22 | LED | `effectDirection` field — never read/written | Remove |
| R23 | Storage.h | Commented-out WebUI block — dead code in `#if` guard | Remove |

---

## Remediation Priority

### Phase 1: Documentation Accuracy (Immediate)
Fix all CRITICAL findings C1-C9 in global docs. These make the `technical-reference.md` essentially unusable — every code example will fail to compile.

### Phase 2: Component Doc Fixes
Fix component-level CRITICALs (C10-C21) and MAJORs — especially dead features documented as functional.

### Phase 3: Code Fixes (Constitution Compliance)
Address ROADMAP items in priority order:
1. **Memory safety** (R1-R7) — constitution ABSOLUTE PRIORITY
2. **HAL isolation** (R8-R10) — constitution NON-NEGOTIABLE
3. **File size splits** (R11-R13) — hard limit violations
4. **Dead code removal** (R17-R23) — YAGNI compliance

### Phase 4: Second Review Pass
Re-run adversarial review after fixes to verify accuracy.

---

*This review was conducted by 4 parallel adversarial agents cross-referencing all 39 documentation files against actual source headers.*
