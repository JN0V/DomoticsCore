<!--
Sync Impact Report:
- Version change: 1.4.0 → 1.5.0 (strengthened HAL isolation rule)
- Modified principles: IX. Hardware Abstraction Layer (HAL) - strengthened #ifdef isolation
- Added sections: None
- Removed sections: None
- Templates updated:
  - .specify/templates/plan-template.md: ⚠ Check HAL compliance
  - .specify/templates/spec-template.md: ⚠ Check HAL compliance
  - .specify/templates/tasks-template.md: ⚠ Check HAL compliance
- Follow-up TODOs: None
-->

# DomoticsCore Constitution

**Purpose**: This constitution defines the non-negotiable principles governing all development on the DomoticsCore ESP32/ESP8266 IoT framework. It ensures code quality, maintainability, and performance on resource-constrained embedded devices.

## Core Principles

### I. SOLID Principles

All code MUST adhere to SOLID principles:

- **Single Responsibility (SRP)**: Each class/module has ONE reason to change. One file = one responsibility.
- **Open/Closed**: Components are open for extension, closed for modification. Use interfaces and inheritance.
- **Liskov Substitution**: Derived classes MUST be substitutable for their base classes without altering correctness.
- **Interface Segregation**: Prefer small, focused interfaces over large monolithic ones.
- **Dependency Inversion**: Depend on abstractions (IComponent, IWebUIProvider), not concrete implementations.

**Rationale**: SOLID ensures maintainable, testable, and extensible code essential for long-term IoT device firmware.

### II. Test-Driven Development (NON-NEGOTIABLE)

TDD is MANDATORY for all development:

- **Red-Green-Refactor**: Write failing test → Implement minimal code → Refactor
- **100% Coverage Gate**: A phase CANNOT proceed to the next phase until:
  1. All tests for the current phase are written
  2. All tests for the current phase pass
  3. Test coverage for the phase is 100%
- **Test Types**: Unit tests for logic, integration tests for component interactions, contract tests for APIs
- **No Untested Code**: Every function, every branch, every edge case MUST have corresponding tests

**Rationale**: Embedded systems are hard to debug in production. Tests catch bugs before they reach devices.

### III. KISS (Keep It Simple, Stupid)

Simplicity is a feature, not a compromise:

- **Simple Solutions First**: Choose the simplest approach that solves the problem
- **No Premature Optimization**: Optimize only after measuring actual bottlenecks
- **Clear Over Clever**: Readable code trumps "clever" solutions
- **Minimal Dependencies**: Each dependency adds complexity and binary size
- **Flat Hierarchies**: Avoid deep inheritance chains (max 3 levels)

**Rationale**: Simple code is easier to test, debug, and maintain on resource-constrained devices.

### IV. YAGNI (You Aren't Gonna Need It)

Build only what is needed NOW:

- **No Speculative Features**: Do not implement features "for the future"
- **No Dead Code**: Remove unused code, commented blocks, and deprecated functions
- **Minimal Interfaces**: Expose only what consumers actually need
- **Incremental Delivery**: Add features when requirements are concrete, not anticipated

**Rationale**: Every line of code consumes flash memory and increases maintenance burden.

### V. Performance First

Embedded constraints drive all design decisions:

- **Memory Budget**: ESP32 has ~320KB RAM, ESP8266 has ~80KB RAM. Every allocation matters.
- **Flash Budget**: Typical ESP32 has 4MB flash. Monitor binary size continuously.
- **Power Awareness**: Minimize CPU cycles; use non-blocking patterns; avoid busy-waiting
- **Heap Fragmentation**: Prefer stack allocation; avoid frequent malloc/free in loops
- **String Operations**: Use PROGMEM for static strings; avoid String concatenation in hot paths

**Rationale**: IoT devices run 24/7 on limited resources. Performance bugs cause real-world failures.

### VI. Modular EventBus Architecture

All inter-component communication MUST use the EventBus:

- **Decoupled Components**: Components MUST NOT have direct references to each other
- **Topic-Based Messaging**: Use hierarchical topics (e.g., `wifi/connected`, `mqtt/message`)
- **Type-Safe Payloads**: Define payload structs for each event type
- **Sticky Events**: Use sticky events for state that late subscribers need
- **No Circular Dependencies**: Event flow MUST be unidirectional where possible

**Rationale**: EventBus enables testable, modular components that can be included/excluded independently.

### VII. File Size Limits

Code files MUST remain small and focused:

- **Target**: 200-500 lines per file (ideal)
- **Maximum**: 800 lines per file (hard limit)
- **Violation Handling**: Files exceeding 800 lines MUST be split in the next refactoring phase
- **Metrics**: Line count excludes blank lines and comments
- **Header Files**: Same limits apply; split into multiple headers if needed

**Rationale**: Small files are easier to understand, test, and review. They enforce SRP naturally.

### VIII. Progressive Refactoring

This is an EXISTING project being brought to best practices:

- **Component by Component**: Refactor one component at a time, starting with Core
- **No Big Bang Rewrites**: Never rewrite more than one component simultaneously
- **Maintain Compatibility**: Preserve existing APIs during refactoring; deprecate before removing
- **Test Before Refactor**: Ensure existing behavior is captured in tests before changing code
- **Green-to-Green**: All tests MUST pass before AND after each refactoring step

**Rationale**: Progressive refactoring minimizes risk and keeps the system functional throughout.

## Code Organization Standards

### IX. Hardware Abstraction Layer (HAL) — #ifdef Isolation (NON-NEGOTIABLE)

All platform-specific code MUST be isolated in HAL files. **`#ifdef` platform directives are FORBIDDEN everywhere except HAL files.**

- **File Naming**: Platform-specific files use `{Component}_HAL.h` (routing header) and `{Component}_ESP32.h`, `{Component}_ESP8266.h`, `{Component}_Stub.h` (implementations)
- **Location**: HAL files reside in `DomoticsCore-{Component}/include/DomoticsCore/`
- **#ifdef ONLY in HAL**: The ONLY place `#ifdef DOMOTICS_PLATFORM_*` or `#if defined(ESP32)` may appear is inside HAL routing headers and platform-specific implementation files
- **FORBIDDEN Locations**: Business logic, components, examples, applications, and utility files MUST NOT contain any `#ifdef` for platform detection
- **Platform Utilities**: If a platform provides utilities (e.g., ESP32 logging macros), the HAL MUST define equivalent macros/functions for other platforms so consumers see a unified API
- **Portability Guarantee**: Adding a new platform requires ONLY changes to HAL files — zero changes to business code

**Example — Logging:**
```cpp
// WRONG - #ifdef in Logger.h (business code)
#if DOMOTICS_PLATFORM_ESP8266
    #define log_i(...) Serial.printf(...)
#endif

// CORRECT - Platform_ESP8266.h defines log macros
// Logger.h uses log_i() without any #ifdef
```

**Rationale**: HAL isolation guarantees that business code is platform-agnostic, testable in native environments, and maintainable without platform expertise.

### X. Non-Blocking Timer Pattern

All timing operations MUST use the Core NonBlockingTimer:

- **No delay()**: The `delay()` function is FORBIDDEN in all code except boot sequences
- **No busy-wait**: While loops waiting for conditions MUST have timeouts
- **Timer Usage**: Use `DomoticsCore::Timer::NonBlockingDelay` from `Utils/Timer.h`
- **Callback Pattern**: For periodic tasks, use timer callbacks instead of polling
- **Main Loop**: The `loop()` function MUST complete in < 10ms

**Rationale**: Blocking operations freeze the entire system, breaking watchdogs and WebSocket connections.

### XI. Centralized Storage

All persistent data MUST go through the Storage component:

- **No Direct Preferences**: Components MUST NOT directly access ESP32 Preferences or SPIFFS
- **Storage API**: Use `StorageComponent::put*()` and `get*()` methods exclusively
- **Namespace Isolation**: Each component gets its own storage namespace via Storage
- **Declaration Pattern**: Components declare storage needs; Storage handles persistence
- **Event-Driven**: Storage changes emit events for subscribers to react

**Example Pattern**:
```cpp
// WRONG - Direct Preferences access
Preferences prefs;
prefs.begin("mqtt");
prefs.putString("broker", broker);

// CORRECT - Via Storage component
storage->putString("mqtt", "broker", broker);
```

**Rationale**: Centralized storage prevents namespace conflicts, enables unified backup/restore, and simplifies testing.

### XII. Multi-Registry Compatibility

The library MUST be publishable to multiple package registries:

- **Target Registries**: PlatformIO Registry (primary), Arduino Library Registry (secondary)
- **Structure Compliance**: Follow both PlatformIO `library.json` AND Arduino `library.properties` formats
- **Naming Conventions**: Use registry-compatible names (no special characters, proper versioning)
- **Dependency Declaration**: Dependencies MUST be declared in both formats
- **Examples Location**: Examples MUST be in `examples/` folder (Arduino standard)
- **Include Path**: Use `#include <LibraryName/Header.h>` pattern for both registries
- **No Platform Lock-in**: Avoid registry-specific features that break cross-registry compatibility

**Validation Checklist**:
- [ ] `library.json` valid for PlatformIO
- [ ] `library.properties` valid for Arduino
- [ ] Examples compile on both platforms
- [ ] Dependencies resolvable on both registries

**Rationale**: Multi-registry support maximizes library reach and allows users to choose their preferred toolchain.

### XIII. Anti-Pattern Avoidance

The codebase MUST avoid known anti-patterns that compromise reliability and testability:

**Forbidden Anti-Patterns:**

| Anti-Pattern | Description | Correct Pattern |
|--------------|-------------|-----------------|
| **Early-Init** | Manually calling `begin()` before `core.begin()` | Let ComponentRegistry handle initialization order |
| **Singleton Abuse** | Global singletons for components | Use ComponentRegistry for component access |
| **God Object** | Classes with too many responsibilities | Split into focused components (SRP) |
| **Circular Dependencies** | A depends on B, B depends on A | Refactor to use EventBus for decoupling |
| **Magic Strings** | Hardcoded event/topic names scattered in code | Use centralized constants (Events.h) |
| **Blocking Callbacks** | Long operations in event handlers | Queue work, process in loop() |
| **Hidden Dependencies** | Components accessing others without declaration | Declare all dependencies via getDependencies() |

**Testing Anti-Patterns to Avoid:**

- Tests requiring real hardware when mocks would suffice
- Tests with hidden dependencies on execution order
- Tests that modify global state without cleanup
- Tests that cannot run in isolation

**Code Smell Indicators:**

- File > 800 lines → Split responsibility
- Function > 50 lines → Extract methods
- Inheritance depth > 3 → Flatten or use composition
- Component with > 5 dependencies → Consider splitting

**Rationale**: Anti-patterns increase technical debt, reduce testability, and make bugs harder to diagnose. Early detection prevents accumulation.

### XIV. Semantic Versioning Discipline

DomoticsCore uses [Semantic Versioning](https://semver.org/) with per-component versions and a root framework version:

**Version Structure:**
- **Root library**: Top-level `library.json` defines the DomoticsCore framework version (`X.Y.Z`)
- **Component libraries**: Each `DomoticsCore-*` has its own `library.json` version and matching `metadata.version` in C++ code

**Propagation Rules:**

| Component Change | Root Change | Reset |
|------------------|-------------|-------|
| Component **PATCH** | Root **PATCH** | None |
| Component **MINOR** | Root **MINOR** | Reset root patch |
| Component **MAJOR** | Root **MAJOR** | Reset root minor/patch |

**Important**: Only the changed component and root are bumped; other components keep their current versions.

**Required Tools:**
```bash
# Check version consistency (CI)
python tools/check_versions.py --verbose

# Bump component version (propagates to root)
python tools/bump_version.py MQTT minor

# Preview changes without modifying
python tools/bump_version.py MQTT minor --dry-run --verbose
```

**Version Bump Rules:**
- MUST use `tools/bump_version.py` for all version changes
- MUST NOT manually edit version numbers in multiple files
- MUST verify consistency with `tools/check_versions.py` before commit
- `library.json.version` MUST match all `metadata.version` assignments in component code

**Rationale**: Consistent versioning ensures dependency resolution works correctly across PlatformIO and Arduino registries, and users can trust version numbers to indicate breaking changes.

## Embedded Constraints

### Hardware Targets

| Platform | RAM | Flash | Status |
|----------|-----|-------|--------|
| ESP32 | 320KB | 4MB | Primary target |
| ESP8266 | 80KB | 4MB | Secondary target |
| ESP32-S2/S3/C3 | Varies | Varies | Compatible |

### Binary Size Guidelines

| Configuration | Flash Target | RAM Target |
|---------------|--------------|------------|
| Core Only | < 300KB | < 20KB |
| Core + WiFi + LED | < 400KB | < 30KB |
| Full Stack | < 1MB | < 60KB |

### Performance Requirements

- **Boot Time**: System ready < 5 seconds (excluding WiFi connection)
- **Loop Latency**: Main loop iteration < 10ms
- **Memory Stability**: No heap growth over 24h continuous operation
- **WebSocket Response**: < 100ms for UI updates

## Quality Gates

### Phase Transition Requirements

**A phase CANNOT transition to the next phase unless:**

1. ✅ All tasks in current phase are complete
2. ✅ All tests for current phase are written
3. ✅ All tests for current phase pass (100% green)
4. ✅ Test coverage for current phase is 100%
5. ✅ No compiler warnings
6. ✅ File size limits respected (< 800 lines)
7. ✅ Code review completed (if applicable)

### Commit Discipline

- **NO COMMITS WITHOUT USER APPROVAL**: Every commit requires explicit user validation
- **Atomic Commits**: One logical change per commit
- **Descriptive Messages**: Follow conventional commits format
- **Green Commits Only**: Never commit code with failing tests

### Documentation Standards

- **Language**: ALL documentation, specs, tasks, and code comments MUST be in English
- **README Required**: Every component MUST have a README.md
- **API Documentation**: Public interfaces MUST have doc comments
- **Architecture Docs**: Significant design decisions MUST be documented

## Governance

### Constitution Authority

- This constitution SUPERSEDES all other development practices
- Violations MUST be flagged and corrected before merge
- Exceptions require explicit justification in the PR/commit message

### Amendment Process

1. Propose amendment with rationale
2. Assess impact on existing code and tests
3. Update constitution version (SemVer)
4. Update dependent templates if affected
5. Document migration path for affected code

### Compliance Verification

- **Pre-commit**: Verify file sizes, test coverage, no warnings
- **PR Review**: Verify SOLID compliance, EventBus usage, performance impact
- **Release**: Verify binary sizes, memory usage, documentation completeness

**Version**: 1.5.0 | **Ratified**: 2025-12-17 | **Last Amended**: 2025-12-19
