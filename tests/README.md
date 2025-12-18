# DomoticsCore Tests

Unit tests and integration tests for DomoticsCore framework features.

## Structure

```
tests/
├── unit/                            # Unit tests (run on native platform)
│   ├── 01-optional-dependencies/    # v1.0.3: Optional dependency support
│   ├── 02-lifecycle-callback/       # v1.1: afterAllComponentsReady() lifecycle
│   ├── 05-storage-namespace/        # Storage namespace isolation
│   └── 06-webui-refactor/           # WebUI refactoring tests
├── integration/                     # Integration tests (run on ESP32 hardware)
│   └── (hardware-specific tests)
└── README.md
```

**Note**: Tests 03 and 04 (early-init bug reproductions) were deleted in v1.2.x as the early-init anti-pattern was eliminated from the codebase.

## Running Tests

Each test is a standalone PlatformIO project:

```bash
# Test 1: Optional Dependencies (v1.0.3)
cd tests/unit/01-optional-dependencies
pio run -t upload -t monitor

# Test 2: Lifecycle Callback (v1.1)
cd tests/unit/02-lifecycle-callback
pio run -t upload -t monitor
```

## Test Descriptions

### 01-optional-dependencies (v1.0.3)

Tests the optional dependency feature using `getDependenciesEx()`.

**What it tests:**
- Required dependencies must be present (fail if missing)
- Optional dependencies are OK if missing
- Framework logs informative messages for optional deps
- Components can access dependencies via `getCore()`

**Expected output:**
```
✅ Required dependency found
ℹ️ Optional dependency missing (expected)
🎉 TEST PASSED!
```

### 02-lifecycle-callback (v1.1)

Tests the `afterAllComponentsReady()` lifecycle hook.

**What it tests:**
- `begin()` called first for all components
- `afterAllComponentsReady()` called after all `begin()`
- All components guaranteed accessible in `afterAllComponentsReady()`
- Proper separation of initialization phases

**Expected output:**
```
✅ begin() called first for all components
✅ afterAllComponentsReady() called after all begin()
✅ All components accessible in afterAllComponentsReady()
🎉 TEST PASSED!
```

## Adding New Tests

1. Create new directory: `tests/unit/XX-test-name/`
2. Add `platformio.ini` with Core dependency
3. Create `src/main.cpp` with test logic
4. Update this README with test description
5. Ensure test validates specific feature

## CI/CD Integration

Future: These tests can be automated with PlatformIO CI:

```yaml
# .github/workflows/tests.yml
- name: Run Unit Tests
  run: |
    cd tests/unit/01-optional-dependencies && pio test
    cd tests/unit/02-lifecycle-callback && pio test
```

## Notes

- Tests are designed for ESP32 hardware
- Use `CORE_DEBUG_LEVEL=3` for verbose output
- Each test is self-contained and independent
- Tests validate both positive and negative cases
