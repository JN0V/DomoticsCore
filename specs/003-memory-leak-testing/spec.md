# Feature Specification: Memory Leak Testing Framework

**Feature Branch**: `003-memory-leak-testing`  
**Created**: 2026-01-04  
**Status**: Draft  
**Input**: User description: "Implement memory leak tracking and autonomous testing mechanisms for native and hardware platforms to eliminate all memory leaks"

## Context

DomoticsCore is an ESP32/ESP8266 IoT framework. The WebUI component experiences memory leaks on ESP8266 due to:
- Repeated allocation of `String` objects in `getWebUIContexts()`
- Heap fragmentation from dynamic context creation
- ~30KB free heap on ESP8266 insufficient for unbounded allocations

This spec defines infrastructure to **detect, track, and eliminate memory leaks** through automated testing on both native (desktop) and hardware (ESP32/ESP8266) platforms.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Native Heap Tracking Tests (Priority: P1) 🎯 MVP

As a developer, I want to run memory leak detection tests on my desktop (native platform) so that I can catch allocation issues without deploying to hardware.

**Why this priority**: Native tests run in seconds vs. minutes for hardware. Catching leaks early in the development cycle saves enormous time. This is the foundation for all other memory testing.

**Independent Test**: Run `pio test -e native` and see heap tracking output for any component. A leak-free component shows stable heap before/after operations.

**Acceptance Scenarios**:

1. **Given** a component with no leaks, **When** I run its native test with heap tracking, **Then** heap usage returns to baseline after component shutdown
2. **Given** a component with a known leak, **When** I run its native test with heap tracking, **Then** the test fails with a report showing bytes leaked and suspected allocation site
3. **Given** multiple test iterations, **When** heap tracking is enabled, **Then** each iteration reports delta heap usage for trend analysis

---

### User Story 2 - Hardware Heap Monitoring (Priority: P2)

As a developer, I want to run long-duration heap stability tests on real ESP32/ESP8266 hardware so that I can verify no memory leaks occur in production conditions over time.

**Why this priority**: Native tests cannot catch all platform-specific leaks (e.g., WiFi stack, AsyncWebServer internals). Hardware validation is essential for production confidence.

**Independent Test**: Flash a test firmware, let it run for a configurable duration (10 min to 24 hours), and receive a pass/fail report with heap metrics.

**Acceptance Scenarios**:

1. **Given** a device running stability test firmware, **When** 10 minutes elapse, **Then** the test reports heap min/max/avg and passes if delta < threshold
2. **Given** WebUI with active WebSocket connections, **When** stability test runs for 1 hour, **Then** heap remains within 5% of initial value
3. **Given** a heap leak of >100 bytes/minute, **When** stability test runs, **Then** test fails with leak rate report

---

### User Story 3 - Autonomous CI/CD Integration (Priority: P2)

As a maintainer, I want memory tests to run automatically on every commit so that regressions are caught immediately without manual intervention.

**Why this priority**: Manual testing doesn't scale. Autonomous testing in CI ensures every change is validated for memory stability.

**Independent Test**: Push a commit and verify GitHub Actions runs memory tests, reporting pass/fail status.

**Acceptance Scenarios**:

1. **Given** a PR with memory leak, **When** CI runs, **Then** PR is blocked with failing memory test status
2. **Given** a clean commit, **When** CI runs native memory tests, **Then** all tests pass within 5 minutes
3. **Given** hardware test configuration, **When** CI triggers (optional self-hosted runner), **Then** hardware tests execute autonomously

---

### User Story 4 - WebUI Memory Optimization Validation (Priority: P1)

As a developer, I want specific tests for WebUI memory patterns so that I can validate fixes like `CachingWebUIProvider` and `StreamingContextSerializer` actually reduce heap usage.

**Why this priority**: WebUI is the current blocker. Targeted tests for WebUI memory patterns are essential to unblock the ESP8266 support feature.

**Independent Test**: Run WebUI-specific memory tests that measure context creation, schema serialization, and WebSocket updates.

**Acceptance Scenarios**:

1. **Given** `CachingWebUIProvider`, **When** `getWebUIContexts()` is called 100 times, **Then** heap increases only on first call (cache hit verification)
2. **Given** `StreamingContextSerializer`, **When** serializing large contexts, **Then** peak heap usage < 2KB (vs. full buffer approach)
3. **Given** WebSocket schema broadcast, **When** 10 clients connect, **Then** per-client heap overhead < 500 bytes

---

### Edge Cases

- What happens when heap is nearly exhausted during test? → Test harness must handle gracefully, report OOM condition
- How does system handle leak in constructor vs. loop()? → Track allocations per phase (init, loop, shutdown)
- What if leak only occurs under specific timing? → Support configurable iteration counts and delays
- How to handle false positives from legitimate caching? → Allow "expected growth" annotations per test

## Requirements *(mandatory)*

### Functional Requirements

#### Heap Tracking Infrastructure

- **FR-001**: System MUST provide a `HeapTracker` class that records heap usage at marked checkpoints
- **FR-002**: System MUST support heap tracking on native (mock), ESP32, and ESP8266 platforms via HAL
- **FR-003**: `HeapTracker` MUST capture: free heap, largest free block, heap fragmentation percentage
- **FR-004**: System MUST provide macros `HEAP_CHECKPOINT(name)` and `HEAP_ASSERT_STABLE(tolerance_bytes)` for test code

#### Native Memory Tests

- **FR-005**: System MUST provide a native-compatible heap allocator mock that tracks all malloc/free calls
- **FR-006**: Native tests MUST be able to detect unreleased allocations at test end
- **FR-007**: System MUST report allocation site (file:line) for leaked memory when possible
- **FR-008**: Native memory tests MUST run in the existing `pio test -e native` framework

#### Hardware Stability Tests

- **FR-009**: System MUST provide a `StabilityTestRunner` class for long-duration hardware tests
- **FR-010**: `StabilityTestRunner` MUST sample heap at configurable intervals (default: 1 second)
- **FR-011**: System MUST detect heap growth trends and report leak rate (bytes/minute)
- **FR-012**: Hardware tests MUST support configurable duration (10 min, 1 hour, 24 hours)
- **FR-013**: System MUST provide test result output via Serial and optional RemoteConsole

#### WebUI-Specific Tests

- **FR-014**: System MUST provide tests verifying `CachingWebUIProvider` prevents repeated allocations
- **FR-015**: System MUST provide tests measuring `StreamingContextSerializer` peak memory usage
- **FR-016**: System MUST provide tests for WebSocket connection/disconnection heap stability
- **FR-017**: Tests MUST cover all existing WebUI providers (Wifi, NTP, MQTT, OTA, SystemInfo, etc.)

#### CI/CD Integration

- **FR-018**: Native memory tests MUST be runnable via `pio test -e native` in GitHub Actions
- **FR-019**: System MUST provide clear pass/fail exit codes for CI integration
- **FR-020**: Test output MUST include structured data (JSON) for automated parsing

### Key Entities

- **HeapSnapshot**: Represents heap state at a point in time (free bytes, largest block, fragmentation)
- **HeapCheckpoint**: Named snapshot with timestamp for comparison
- **AllocationRecord**: Tracks individual allocation (size, file, line, freed status) - native only
- **StabilityReport**: Summary of long-duration test (duration, samples, min/max/avg heap, leak rate, verdict)
- **MemoryTestResult**: Pass/fail with details (expected, actual, tolerance, delta)

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All existing 158+ native tests continue to pass with heap tracking enabled
- **SC-002**: Native memory test suite executes in under 60 seconds total
- **SC-003**: WebUI `CachingWebUIProvider` tests show 0 bytes leaked after 100 iterations
- **SC-004**: Hardware stability test detects artificial 1KB/minute leak within 5 minutes
- **SC-005**: ESP8266 WebUI passes 10-minute stability test with heap delta < 1KB
- **SC-006**: ESP32 full-stack passes 1-hour stability test with heap delta < 2KB
- **SC-007**: CI pipeline includes memory tests and completes in under 10 minutes
- **SC-008**: All WebUI components (8 providers) have memory tests with 100% pass rate

## Assumptions

- Native heap tracking uses standard malloc/free interception (available on Linux/macOS/Windows)
- Hardware heap metrics available via existing `HAL::Platform::getFreeHeap()` API
- ESP8266 baseline free heap after full init is ~25-30KB
- CI uses GitHub Actions with native environment available
- Hardware CI (optional) requires self-hosted runner with connected devices

## Out of Scope

- Profiling CPU usage or timing (memory focus only)
- Stack overflow detection (separate concern)
- Memory-mapped I/O or DMA tracking
- Third-party library internal leak detection (only detect impact on total heap)
