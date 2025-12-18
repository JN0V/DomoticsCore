# Research: DomoticsCore Library Refactoring

**Date**: 2025-12-17
**Feature**: 001-core-refactoring
**Status**: Complete

## Research Summary

All technical decisions for this refactoring are already established through the existing codebase, constitution, and clarifications. No external research needed - this is a refactoring of existing code to meet quality standards.

## Decisions

### 1. Test Framework

**Decision**: PlatformIO Unity framework with native platform execution
**Rationale**: Already in use, supports `pio test -e native` for hardware-independent unit tests
**Alternatives Considered**:
- GoogleTest: More features but heavier, not native to PlatformIO
- Catch2: Good but Unity already integrated

### 2. HAL Testing Strategy

**Decision**: HAL code tested via integration tests on real hardware; 100% coverage applies to non-HAL code only
**Rationale**: HAL code by definition requires hardware - mocking defeats the purpose
**Alternatives Considered**:
- Mock HAL implementations: Added complexity, may not catch real hardware issues
- Lower coverage targets: Would compromise quality standards

### 3. Component Refactoring Completion Criteria

**Decision**: Tests pass + coverage met + constitution checklist + documentation updated
**Rationale**: Comprehensive checklist ensures nothing is missed
**Alternatives Considered**:
- Tests only: Insufficient - doesn't ensure documentation
- Peer review: Good practice but not blocking for solo development

### 4. CI/CD Strategy

**Decision**: Local pre-commit script (`tools/local_ci.sh`) for quality gates; minimal GitHub Actions
**Rationale**: Run validation locally before commit to avoid polluting git history with failures
**Alternatives Considered**:
- Full GitHub Actions: Requires pushing to test, wastes CI minutes
- No CI: Quality gates become manual and error-prone

### 5. Version Bumping

**Decision**: Use `tools/bump_version.py` with propagation rules (component → root)
**Rationale**: Already established in README, ensures consistency across components
**Alternatives Considered**:
- Manual version editing: Error-prone, hard to maintain consistency
- Single version for all: Loses granularity for independent component updates

### 6. Early-Init Pattern

**Decision**: Eliminated as anti-pattern (Constitution XIII)
**Rationale**: Was a workaround that bypassed ComponentRegistry's dependency resolution
**Alternatives Considered**:
- Keep early-init: Violates DIP, makes testing harder
- Exception for more components: Only LED exception justified for boot visualization

### 7. Example Consolidation

**Decision**: Merge 3 EventBus examples → 1, remove MQTTWifiWithWebUI (redundant)
**Rationale**: Reduce maintenance burden while keeping educational value
**Alternatives Considered**:
- Keep all examples: More maintenance, some redundant
- Remove more examples: Loses educational coverage

## Technology Choices (Confirmed)

| Area | Choice | Version | Notes |
|------|--------|---------|-------|
| Build System | PlatformIO | 6.x | |
| Test Framework | Unity | via PlatformIO | |
| Web Server | ESP32Async/ESPAsyncWebServer | ^3.8.0 (3.9.3) | ESP32Async org, NOT me-no-dev |
| TCP Stack | ESP32Async/AsyncTCP | ^3.4.8 (3.4.9) | ESP32Async org, NOT me-no-dev |
| JSON | bblanchon/ArduinoJson | ^7.0.0 (7.4.2) | v7 for perf improvements |
| WiFi Manager | ESPAsync_WiFiManager | ^1.15.1 | Shares AsyncWebServer instance |
| MQTT | knolleary/PubSubClient | ^2.8 | |

## No Further Research Required

All technical decisions are established. Proceed to Phase 1: Design & Contracts.
