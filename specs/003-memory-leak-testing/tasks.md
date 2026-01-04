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
- [x] T015c Run ESP8266 hardware tests - 5 tests PASSED, memory leak detected (5 bytes/call)

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

- [ ] T016 [P] [US1] Write test for heap baseline capture in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp
- [ ] T017 [P] [US1] Write test for multi-checkpoint comparison in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp
- [ ] T018 [P] [US1] Write test for leak rate calculation in DomoticsCore-Core/test/test_heap_tracker/test_heap_tracker.cpp

### Implementation for User Story 1

- [ ] T019 [US1] Implement HeapSnapshot struct (freeHeap, largestBlock, fragmentation) in HeapTracker.h
- [ ] T020 [US1] Implement checkpoint() method with named snapshots in HeapTracker.h
- [ ] T021 [US1] Implement getDelta(start, end) method in HeapTracker.h
- [ ] T022 [US1] Implement getLeakRate(start, end, duration) method in HeapTracker.h
- [ ] T023 [P] [US1] Implement Native allocation tracking with file:line in HeapTracker_Native.h
- [ ] T024 [US1] Implement getUnfreedAllocations() for leak report in HeapTracker_Native.h
- [ ] T025 [US1] Add JSON output format for test results in HeapTracker.h

### Integration with Existing Tests

- [x] T026 [US1] Add HeapTracker to Storage component test in DomoticsCore-Storage/test/test_storage_api/test_storage_api.cpp
- [x] T027 [US1] Verify existing tests still pass with HeapTracker enabled - 22 tests PASSED

**Phase Gate:**
- [ ] Native heap tracking fully functional
- [ ] At least one existing test uses HeapTracker
- [ ] All US1 tests pass

**Checkpoint**: User Story 1 complete - native leak detection working

---

## Phase 4: User Story 4 - WebUI Memory Optimization Validation (Priority: P1)

**Goal**: Validate CachingWebUIProvider and StreamingContextSerializer fix memory leaks

**Independent Test**: Run WebUI-specific memory tests showing cache effectiveness

### Tests for User Story 4

- [x] T028 [P] [US4] CachingWebUIProvider added to IWebUIProvider.h (not separate project - follows component pattern)
- [x] T029 [P] [US4] Write CachingWebUIProvider cache-hit test (100 iterations) in test_webui_component.cpp
- [x] T030 [P] [US4] Write CachingWebUIProvider invalidation test in test_webui_component.cpp
- [ ] T031 [P] [US4] Create test_serializer_memory project in DomoticsCore-WebUI/test/test_serializer_memory/platformio.ini
- [ ] T032 [P] [US4] Write StreamingContextSerializer peak memory test (<2KB) in DomoticsCore-WebUI/test/test_serializer_memory/test_serializer_memory.cpp

### Implementation for User Story 4

- [x] T033 [US4] CachingWebUIProvider with memory-safe context caching in IWebUIProvider.h
- [ ] T034 [US4] Add memory metrics to StreamingContextSerializer in DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h
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

**Phase Gate:**
- [ ] All WebUI providers pass memory tests
- [ ] CachingWebUIProvider shows 0 growth after 100 iterations
- [ ] StreamingContextSerializer peak < 2KB

**Checkpoint**: User Story 4 complete - WebUI memory optimizations validated

---

## Phase 5: User Story 2 - Hardware Heap Monitoring (Priority: P2)

**Goal**: Long-duration stability tests on real hardware

**Independent Test**: Flash stability firmware, run 10 minutes, get pass/fail report

### Tests for User Story 2

- [ ] T044 [P] [US2] Write StabilityTestRunner unit tests in DomoticsCore-Core/test/test_heap_tracker/test_stability_runner.cpp
- [ ] T045 [P] [US2] Write leak rate detection test (simulated) in DomoticsCore-Core/test/test_heap_tracker/test_stability_runner.cpp

### Implementation for User Story 2

- [ ] T046 [US2] Create StabilityTestRunner.h in DomoticsCore-Core/include/DomoticsCore/Testing/StabilityTestRunner.h
- [ ] T047 [US2] Implement setDuration(), setSampleInterval(), setLeakThreshold() in StabilityTestRunner.h
- [ ] T048 [US2] Implement begin(), sample(), isRunning() loop methods in StabilityTestRunner.h
- [ ] T049 [US2] Implement StabilityReport struct (min, max, avg, leakRate, verdict) in StabilityTestRunner.h
- [ ] T050 [US2] Implement getReport() with JSON output in StabilityTestRunner.h
- [ ] T051 [US2] Add Serial output for real-time monitoring in StabilityTestRunner.h

### Hardware Stability Test Firmwares

- [ ] T052 [US2] Create webui_stability test firmware in tests/stability/webui_stability/platformio.ini
- [ ] T053 [US2] Implement WebUI stability test main.cpp in tests/stability/webui_stability/src/main.cpp
- [ ] T054 [P] [US2] Create fullstack_stability test firmware in tests/stability/fullstack_stability/platformio.ini
- [ ] T055 [US2] Implement full-stack stability test main.cpp in tests/stability/fullstack_stability/src/main.cpp

**Phase Gate:**
- [ ] StabilityTestRunner works on native
- [ ] Stability firmwares compile for ESP32 and ESP8266
- [ ] Manual 10-minute test passes on ESP8266

**Checkpoint**: User Story 2 complete - hardware stability testing available

---

## Phase 6: User Story 3 - Autonomous CI/CD Integration (Priority: P2)

**Goal**: Automated memory tests in GitHub Actions

**Independent Test**: Push commit, see memory tests run and pass in CI

### Implementation for User Story 3

- [ ] T056 [US3] Create memory-tests.yml workflow in .github/workflows/memory-tests.yml
- [ ] T057 [US3] Add native memory test job (pio test -e native) to workflow
- [ ] T058 [US3] Add test result parsing and status reporting to workflow
- [ ] T059 [US3] Configure workflow to run on PR and push to main
- [ ] T060 [P] [US3] Add memory test badge to README.md

### CI Validation

- [ ] T061 [US3] Verify workflow runs successfully on test PR
- [ ] T062 [US3] Verify failing memory test blocks PR merge

**Phase Gate:**
- [ ] CI workflow executes successfully
- [ ] Memory tests integrated into PR checks
- [ ] Test time < 10 minutes

**Checkpoint**: User Story 3 complete - autonomous CI/CD working

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, cleanup, integration

- [ ] T063 [P] Document HeapTracker API in DomoticsCore-Core/README.md
- [ ] T064 [P] Document StabilityTestRunner usage in tests/stability/README.md
- [ ] T065 [P] Add memory testing section to docs/guides/webui-developer.md
- [ ] T066 Update specs/003-memory-leak-testing/tasks.md with completion status
- [ ] T067 Run all native tests to verify no regressions (pio test -e native)
- [ ] T068 Verify all files < 800 lines

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup - HeapTracker core required for all stories
- **User Story 1 (Phase 3)**: Depends on Phase 2 - Native heap tracking
- **User Story 4 (Phase 4)**: Depends on Phase 3 - WebUI memory tests (uses HeapTracker)
- **User Story 2 (Phase 5)**: Depends on Phase 2 - Hardware stability tests
- **User Story 3 (Phase 6)**: Depends on Phase 3 - CI integration (uses native tests)
- **Polish (Phase 7)**: Depends on all user stories complete

### User Story Dependencies

```
Phase 2 (HeapTracker Core)
    ↓
    ├─→ US1 (Native Heap Tracking) ─→ US4 (WebUI Memory) ─┐
    │                                                      │
    └─→ US2 (Hardware Stability) ─────────────────────────┤
                                                          ↓
                                    US3 (CI/CD) ←─────────┘
                                          ↓
                                    Phase 7 (Polish)
```

### Parallel Opportunities

**Phase 2 (HeapTracker HAL)**:
```
T005: HeapTracker_ESP32.h
T006: HeapTracker_ESP8266.h    } All parallel - different files
T007: HeapTracker_Native.h
```

**Phase 4 (WebUI Provider Tests)**:
```
T036-T043: All 8 WebUI providers can be tested in parallel
```

---

## Implementation Strategy

### MVP First (US1 + US4)

1. Complete Phase 1: Setup
2. Complete Phase 2: HeapTracker Core
3. Complete Phase 3: US1 (Native Heap Tracking)
4. Complete Phase 4: US4 (WebUI Memory Tests)
5. **STOP and VALIDATE**: WebUI memory leaks identified and fixed
6. Deploy/merge if memory issues resolved

### Incremental Delivery

1. Setup + HeapTracker Core → Testing infrastructure ready
2. US1 → Native leak detection → Can test any component
3. US4 → WebUI validated → Main blocker resolved
4. US2 → Hardware stability → Production confidence
5. US3 → CI/CD → Automated regression detection

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story is independently testable
- **NO COMMITS WITHOUT USER APPROVAL** (Constitution - Commit Discipline)
- Stop at any checkpoint to validate story independently
- Primary goal: Unblock ESP8266 WebUI by validating memory fixes

## Constitution Compliance Checklist

| Gate | Requirement | Status |
|------|-------------|--------|
| TDD | All tests written BEFORE implementation | ☐ |
| TDD | All tests pass (100% green) | ☐ |
| SOLID | SRP: HeapTracker, StabilityRunner, Tests separated | ☐ |
| KISS | Minimal API (checkpoint + assert) | ☐ |
| YAGNI | Only leak detection, no profiling | ☐ |
| HAL | HeapTracker uses Platform HAL pattern | ☐ |
| File Size | All files < 800 lines | ☐ |
