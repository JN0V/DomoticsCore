# DomoticsCore Tests

Unit tests for DomoticsCore framework features.

## Structure

```
tests/
├── unit/
│   ├── 01-optional-dependencies/    # v1.0.3: Optional dependency support
│   ├── 02-lifecycle-callback/       # v1.1: afterAllComponentsReady() lifecycle
│   ├── 03-bug2-early-init/          # Bug reproduction: Early-init scenario
│   └── 04-system-scenario/          # Bug reproduction: System.begin() scenario
└── README.md
```

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

### 03-bug2-early-init

Reproduction test for reported Bug #2: Early-init + optional dependencies crash.

**What it tests:**
- Custom component with optional dependency on Storage
- Storage early-initialized before `core.begin()`
- Verifies no crash occurs
- Validates early-init detection works

**Result:** ✅ Bug NOT reproducible - test passes
**Conclusion:** Early-init pattern works correctly with optional dependencies

### 04-system-scenario

Exact reproduction of System::begin() scenario with early-init.

**What it tests:**
- WaterMeter component added before System::begin()
- WaterMeter declares optional dependency on Storage
- Storage early-initialized (System pattern)
- Core.begin() called
- Verifies no crash and Storage accessible

**Result:** ✅ No crash - System pattern works correctly
**Conclusion:** Bug reports not reproducible with v1.1.0 code

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
