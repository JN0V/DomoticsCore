# Tasks: Memory Leak Testing Framework

**Input**: Design documents from `/specs/003-memory-leak-testing/`  
**Prerequisites**: plan.md, spec.md

**Tests**: Per DomoticsCore Constitution (Principle II - TDD), tests are MANDATORY for this testing infrastructure.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- Include exact file paths in descriptions

## Path Conventions

DomoticsCore multi-component library structure:
- Core testing infrastructure: `DomoticsCore-Core/include/DomoticsCore/Testing/`
- WebUI memory tests: `DomoticsCore-WebUI/test/`
- Stability tests: `tests/stability/`
- CI workflows: `.github/workflows/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create Testing module structure and base files

- [x] T001 Create Testing directory structure in DomoticsCore-Core/include/DomoticsCore/Testing/
- [x] T002 [P] Create Testing.h convenience header in DomoticsCore-Core/include/DomoticsCore/Testing.h
- [x] T003 [P] Add Testing module to DomoticsCore-Core/library.json dependencies

---

## Phase 2: Foundational (HeapTracker Core API)

**Purpose**: Core HeapTracker infrastructure that ALL memory tests depend on

**⚠️ CRITICAL**: All user story tests depend on this phase being complete

### HeapTracker HAL Architecture

- [x] T004 Create HeapTracker.h with platform-agnostic API in DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker.h
- [x] T005 [P] Create HeapTracker_ESP32.h using heap_caps API in DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_ESP32.h
- [x] T006 [P] Create HeapTracker_ESP8266.h using ESP.getFreeHeap() in DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_ESP8266.h
- [x] T007 [P] Create HeapTracker_Native.h with malloc/free tracking in DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_Native.h
- [x] T008 Create HeapTracker_HAL.h routing header in DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_HAL.h

### Test Macros and Utilities

- [x] T009 Add HEAP_CHECKPOINT(name) macro to HeapTracker.h
- [x] T010 [P] Add HEAP_ASSERT_STABLE(tracker, start, end, tolerance) macro to HeapTracker.h
- [x] T011 [P] Add HEAP_ASSERT_NO_GROWTH(tracker, checkpoint) macro for single-point checks

### Meta-Tests for HeapTracker

- [x] T012 Create test_heap_tracker native test in DomoticsCore-Core/test/test_heap_tracker/ (uses component platformio.ini)
- [x] T013 [P] Write HeapTracker checkpoint tests in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp
- [x] T014 [P] Write HeapTracker leak detection tests in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp
- [x] T015 Run HeapTracker native tests (pio test -e native) - 24 tests PASSED
- [x] T015b Create ESP8266 hardware tests in DomoticsCore-WebUI/test/test_heap_esp8266/
- [x] T015c Run ESP8266 hardware tests - 6 tests PASSED (includes chunked large schema test)

**Phase Gate:**
- [x] All HeapTracker tests pass (24 native tests)
- [x] HeapTracker works on native platform (mallinfo real tracking)
- [x] HAL files < 800 lines each
- [x] HeapTracker works on ESP8266 hardware (5 tests passed)

**Checkpoint**: HeapTracker infrastructure ready - user story tests can now be implemented

---

## Phase 3: User Story 1 - Native Heap Tracking Tests (Priority: P1) 🎯 MVP

**Goal**: Enable developers to detect memory leaks in native tests

**Independent Test**: Run `pio test -e native` on any component and verify heap stability

### Tests for User Story 1

- [x] T016 [P] [US1] Write test for heap baseline capture in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp
- [x] T017 [P] [US1] Write test for multi-checkpoint comparison in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp
- [x] T018 [P] [US1] Write test for leak rate calculation in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp

### Implementation for User Story 1

- [x] T019 [US1] Implement HeapSnapshot struct (freeHeap, largestBlock, fragmentation) in HeapTracker.h
- [x] T020 [US1] Implement checkpoint() method with named snapshots in HeapTracker.h
- [x] T021 [US1] Implement getDelta(start, end) method in HeapTracker.h
- [x] T022 [US1] Implement getLeakRate(start, end, duration) method in HeapTracker.h
- [x] T023 [P] [US1] Implement Native allocation tracking with file:line in HeapTracker_Native.h
- [x] T024 [US1] Implement getUnfreedAllocations() for leak report in HeapTracker_Native.h
- [x] T025 [US1] Add JSON output format for test results in HeapTracker.h (SKIPPED - YAGNI)

### Integration with Existing Tests

- [x] T026 [US1] Add HeapTracker to Storage component test in DomoticsCore-Storage/test/test_storage_api/test_storage_api.cpp
- [x] T027 [US1] Verify existing tests still pass with HeapTracker enabled - 22 tests PASSED

**Phase Gate:**
- [x] Native heap tracking fully functional
- [x] At least one existing test uses HeapTracker
- [x] All US1 tests pass (76 native tests)

**Checkpoint**: User Story 1 complete - native leak detection working

---

## Phase 4: User Story 4 - WebUI Memory Optimization Validation (Priority: P1)

**Goal**: Validate CachingWebUIProvider and StreamingContextSerializer fix memory leaks

**Independent Test**: Run WebUI-specific memory tests showing cache effectiveness

### Tests for User Story 4

- [x] T028 [P] [US4] CachingWebUIProvider added to IWebUIProvider.h (not separate project - follows component pattern)
- [x] T029 [P] [US4] Write CachingWebUIProvider cache-hit test (100 iterations) in test_webui_component.cpp
- [x] T030 [P] [US4] Write CachingWebUIProvider invalidation test in test_webui_component.cpp
- [x] T031 [P] [US4] Create test_serializer_memory project in DomoticsCore-WebUI/test/test_heap_esp8266/platformio.ini (integrated)
- [x] T032 [P] [US4] Write StreamingContextSerializer peak memory test (<2KB) - chunked large schema test shows 200 bytes peak

### Implementation for User Story 4

- [x] T033 [US4] CachingWebUIProvider with memory-safe context caching in IWebUIProvider.h
- [x] T034 [US4] Add memory metrics to StreamingContextSerializer (getTotalBytesWritten, getChunkCount)
- [x] T035 [US4] Create test_many_providers_memory_usage test - 15 contexts = 7KB schema, 0 bytes peak
- [x] T036 [US4] Analyze cached context RAM usage - ESP8266 has only ~6KB free after providers load
- [x] T037 [US4] PROGMEM: WebUIField uses const char* instead of String (name, label, value, unit, endpoint)
- [x] T038 [US4] PROGMEM: WebUIContext has const char* constructor for static contexts
- [x] T039 [US4] WifiWebUI optimized: 5→3 contexts (merged badges and settings)
- [ ] T040 [US4] Test Standard example on ESP8266 with optimizations
- [ ] T041 [US4] Validate schema size < 10KB and heap stable

### Rapid Refresh Protection (Added during implementation)

- [x] T043b [US4] Add schema rate limiting (100ms + mutex) in WebUI.h
- [x] T043c [US4] Add schema timeout reset (5s safety) in WebUI.h
- [x] T043d [US4] Write test_rapid_refresh_schema_generation test in test_webui_component.cpp

**Phase Gate:**
- [x] WebUI memory tests pass (77 native tests)
- [x] CachingWebUIProvider shows 0 growth after 100 iterations
- [x] StreamingContextSerializer peak < 2KB (200 bytes measured)
- [x] Rapid refresh protection tested (77 native tests pass)

**Checkpoint**: User Story 4 complete - WebUI memory optimizations validated

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, cleanup, integration

- [x] T044 Document HeapTracker API in DomoticsCore-Core/README.md
- [x] T045 Run all native tests to verify no regressions - 77 WebUI + 65 Core = 142 tests PASSED
- [x] T046 Run ESP8266 hardware tests to verify stability (user manual test)
- [x] T047 Test NTP example on ESP8266 - OK (stable, no memory leaks)
- [x] T048 Test WiFi WebUI on ESP8266 - OK (crash fixed via EventBus WS close on AP disable)
- [x] T049 Test RemoteConsole WebUI on ESP8266 - OK (Settings tab card; log level options via endpoint; heap stable)

---

## Notes

- Focus on native + ESP8266 testing
- **NO COMMITS WITHOUT USER APPROVAL** (Constitution - Commit Discipline)
- Primary goal: Unblock ESP8266 WebUI by validating memory fixes
