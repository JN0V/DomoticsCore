# Implementation Plan: Memory Leak Testing Framework

**Branch**: `003-memory-leak-testing` | **Date**: 2026-01-04 | **Spec**: [spec.md](spec.md)  
**Input**: Feature specification from `/specs/003-memory-leak-testing/spec.md`

## Summary

Build a comprehensive memory leak testing framework for DomoticsCore that enables:
1. Native heap tracking for desktop testing (fast iteration)
2. Hardware stability testing for ESP32/ESP8266 (production validation)
3. CI/CD integration for autonomous regression detection
4. WebUI-specific memory tests to eliminate current leaks

## Technical Context

**Language/Version**: C++14 (gnu++14), Arduino Framework  
**Primary Dependencies**: Unity test framework (existing), Platform HAL (existing)  
**Testing**: PlatformIO native tests + hardware stability tests  
**Target Platforms**: Native (Linux/macOS/Windows), ESP32, ESP8266  
**Project Type**: Embedded IoT library extension (test infrastructure)  
**Performance Goals**: Native tests < 60s total, leak detection within 5 minutes  
**Constraints**: No external dependencies, must work within existing HAL architecture

## Constitution Check

| Principle | Checkpoint | Status |
|-----------|------------|--------|
| **I. SOLID** | SRP: HeapTracker separate from StabilityRunner | ✅ |
| **II. TDD** | Tests for memory tests (meta-testing) | ✅ |
| **III. KISS** | Minimal API: checkpoint + assert | ✅ |
| **IV. YAGNI** | Only what's needed for leak detection | ✅ |
| **V. Performance** | Zero overhead when disabled | ✅ |
| **VI. EventBus** | N/A - Test infrastructure | ✅ |
| **VII. File Size** | Each file < 800 lines | ✅ |
| **VIII. Progressive** | Non-breaking addition to existing tests | ✅ |
| **IX. HAL** | HeapTracker uses Platform HAL | ✅ |

## Project Structure

### New Files (Test Infrastructure)

```text
DomoticsCore-Core/
├── include/DomoticsCore/
│   ├── Testing/
│   │   ├── HeapTracker.h           # Core heap tracking API
│   │   ├── HeapTracker_Native.h    # Native malloc/free interception
│   │   ├── HeapTracker_ESP32.h     # ESP32 heap_caps API
│   │   ├── HeapTracker_ESP8266.h   # ESP8266 ESP.getFreeHeap()
│   │   └── StabilityTestRunner.h   # Long-duration test harness
│   └── Testing.h                   # Convenience include
├── test/
│   └── test_heap_tracker/          # Meta-tests for heap tracker
│       ├── platformio.ini
│       └── test_heap_tracker.cpp

DomoticsCore-WebUI/
├── test/
│   ├── test_provider_memory/       # CachingWebUIProvider tests
│   │   ├── platformio.ini
│   │   └── test_provider_memory.cpp
│   └── test_serializer_memory/     # StreamingContextSerializer tests
│       ├── platformio.ini
│       └── test_serializer_memory.cpp

tests/
├── stability/                      # Hardware stability test firmwares
│   ├── webui_stability/
│   │   ├── platformio.ini
│   │   └── src/main.cpp
│   └── fullstack_stability/
│       ├── platformio.ini
│       └── src/main.cpp

.github/workflows/
└── memory-tests.yml                # CI workflow for memory tests
```

## Key Design Decisions

### 1. HeapTracker API (Minimal)

```cpp
// Usage in tests
HeapTracker tracker;
tracker.checkpoint("before_init");
component.begin();
tracker.checkpoint("after_init");
component.shutdown();
tracker.checkpoint("after_shutdown");

// Assert no leak (with tolerance)
HEAP_ASSERT_STABLE(tracker, "before_init", "after_shutdown", 100); // 100 bytes tolerance
```

### 2. Native Allocation Tracking

On native platform, wrap malloc/free to track allocations:
- Use `__wrap_malloc` / `__wrap_free` with linker flags
- Track allocation site via `__FILE__` and `__LINE__` macros
- Report leaked allocations at test end

### 3. Hardware Stability Test

```cpp
StabilityTestRunner runner;
runner.setDuration(10 * 60 * 1000);  // 10 minutes
runner.setSampleInterval(1000);       // 1 second
runner.setLeakThreshold(100);         // 100 bytes/minute

runner.begin();
while (runner.isRunning()) {
    core.loop();
    runner.sample();
}
StabilityReport report = runner.getReport();
```

## Complexity Tracking

> No Constitution violations anticipated. All new code follows HAL pattern.
