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
- [ ] T003 [P] Add Testing module to DomoticsCore-Core/library.json dependencies

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
- [ ] T025 [US1] Add JSON output format for test results in HeapTracker.h

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
- [ ] T035 [US4] Create memory test for each WebUI provider (8 total) in DomoticsCore-WebUI/test/test_provider_memory/test_provider_memory.cpp

### WebUI Provider Memory Tests

- [ ] T036 [P] [US4] Test WifiWebUI memory stability in test_provider_memory.cpp
- [ ] T037 [P] [US4] Test NTPWebUI memory stability in test_provider_memory.cpp
- [ ] T038 [P] [US4] Test MQTTWebUI memory stability in test_provider_memory.cpp
- [ ] T039 [P] [US4] Test OTAWebUI memory stability in test_provider_memory.cpp
- [ ] T040 [P] [US4] Test SystemInfoWebUI memory stability in test_provider_memory.cpp
- [ ] T041 [P] [US4] Test StorageWebUI memory stability in test_provider_memory.cpp
- [ ] T042 [P] [US4] Test HomeAssistantWebUI memory stability in test_provider_memory.cpp
- [ ] T043 [P] [US4] Test RemoteConsoleWebUI memory stability in test_provider_memory.cpp

### Rapid Refresh Protection (Added during implementation)

- [x] T043b [US4] Add schema rate limiting (100ms + mutex) in WebUI.h
- [x] T043c [US4] Add schema timeout reset (5s safety) in WebUI.h
- [x] T043d [US4] Write test_rapid_refresh_schema_generation test in test_webui_component.cpp

**Phase Gate:**
- [ ] All WebUI providers pass memory tests
- [x] CachingWebUIProvider shows 0 growth after 100 iterations
- [x] StreamingContextSerializer peak < 2KB (200 bytes measured)
- [x] Rapid refresh protection tested (77 native tests pass)

**Checkpoint**: User Story 4 complete - WebUI memory optimizations validated

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, cleanup, integration

- [ ] T044 Document HeapTracker API in DomoticsCore-Core/README.md
- [ ] T045 Run all native tests to verify no regressions (pio test -e native)
- [ ] T046 Run ESP8266 hardware tests to verify stability

---

## Notes

- Focus on native + ESP8266 testing
- **NO COMMITS WITHOUT USER APPROVAL** (Constitution - Commit Discipline)
- Primary goal: Unblock ESP8266 WebUI by validating memory fixes
