# Quickstart: Component Refactoring Guide

**Date**: 2025-12-17
**Feature**: 001-core-refactoring

## Before You Start

Ensure you understand:
1. [Constitution v1.4.0](../../.specify/memory/constitution.md) - 14 principles
2. [Specification](spec.md) - Requirements and success criteria
3. [Data Model](data-model.md) - Entity definitions

## Per-Component Refactoring Checklist

Use this checklist for EACH component refactoring phase.

### Pre-Refactoring

- [ ] Read existing component code completely
- [ ] Document current public API (preserve compatibility)
- [ ] Identify files > 800 lines to split
- [ ] Identify HAL code to isolate
- [ ] List existing tests and examples

### Phase Gate: Tests First (TDD)

- [ ] Create `DomoticsCore-{Component}/test/` directory
- [ ] Write unit tests for ALL public methods
- [ ] Tests must be runnable with `pio test -e native`
- [ ] Verify tests FAIL before implementation (Red)

### Implementation

- [ ] Refactor code to pass tests (Green)
- [ ] Apply SOLID principles
- [ ] Move hardware code to `*_HAL` files
- [ ] Remove any `delay()` calls → use NonBlockingDelay
- [ ] Remove any direct Preferences access → use Storage
- [ ] Ensure all inter-component communication via EventBus
- [ ] Split files > 800 lines

### Post-Refactoring

- [ ] All tests pass (100% green)
- [ ] Coverage = 100% for non-HAL code
- [ ] No compiler warnings
- [ ] All files < 800 lines
- [ ] README.md updated with API docs
- [ ] Examples compile and run
- [ ] Version bumped with `tools/bump_version.py`

### Quality Gate

Run local CI before commit:
```bash
./tools/local_ci.sh
```

## Component Order

Follow this order strictly (respects dependencies):

| Order | Component | Command |
|-------|-----------|---------|
| 1 | Core | `cd DomoticsCore-Core && pio test -e native` |
| 2 | Storage | `cd DomoticsCore-Storage && pio test -e native` |
| 3 | LED | `cd DomoticsCore-LED && pio test -e native` |
| 4 | Wifi | `cd DomoticsCore-Wifi && pio test -e native` |
| 5 | NTP | `cd DomoticsCore-NTP && pio test -e native` |
| 6 | MQTT | `cd DomoticsCore-MQTT && pio test -e native` |
| 7 | OTA | `cd DomoticsCore-OTA && pio test -e native` |
| 8 | RemoteConsole | `cd DomoticsCore-RemoteConsole && pio test -e native` |
| 9 | WebUI | `cd DomoticsCore-WebUI && pio test -e native` |
| 10 | SystemInfo | `cd DomoticsCore-SystemInfo && pio test -e native` |
| 11 | HomeAssistant | `cd DomoticsCore-HomeAssistant && pio test -e native` |
| 12 | System | `cd DomoticsCore-System && pio test -e native` |

## Local CI Script

Create `tools/local_ci.sh`:

```bash
#!/bin/bash
set -e

echo "=== DomoticsCore Local CI ==="

echo "1. Checking version consistency..."
python tools/check_versions.py --verbose

echo "2. Running unit tests..."
pio test -e native

echo "3. Checking file sizes..."
find . -name "*.h" -o -name "*.cpp" -not -path "./.pio/*" | while read f; do
  lines=$(wc -l < "$f")
  if [ "$lines" -gt 800 ]; then
    echo "ERROR: $f has $lines lines (max 800)"
    exit 1
  fi
done

echo "4. Checking for compiler warnings..."
pio run -e esp32dev 2>&1 | grep -i "warning:" && echo "WARNING: Compiler warnings detected" || echo "No warnings"

echo "5. Building all examples..."
./build_all_examples.sh

echo "=== All checks passed! ==="
```

## Version Bump After Refactoring

After completing a component:

```bash
# Patch bump (bug fixes, refactoring)
python tools/bump_version.py Core patch

# Minor bump (new features)
python tools/bump_version.py Core minor

# Always verify
python tools/check_versions.py --verbose
```

## Common Patterns

### Converting delay() to NonBlockingDelay

```cpp
// BEFORE (forbidden)
delay(1000);

// AFTER (correct)
#include <DomoticsCore/Utils/Timer.h>
using namespace DomoticsCore::Timer;

NonBlockingDelay timer;
timer.start(1000, []() {
    // Action after 1 second
});
```

### Converting direct Preferences to Storage

```cpp
// BEFORE (forbidden)
Preferences prefs;
prefs.begin("mqtt");
String broker = prefs.getString("broker", "");

// AFTER (correct)
auto* storage = core.getComponent<StorageComponent>("Storage");
String broker = storage->getString("mqtt", "broker", "");
```

### Declaring Dependencies

```cpp
std::vector<Dependency> getDependencies() const override {
    return {
        {"Core", true},      // Required
        {"Storage", true},   // Required
        {"WebUI", false}     // Optional
    };
}
```

### Publishing Lifecycle Events

```cpp
ComponentStatus begin() override {
    // ... initialization ...
    
    if (success) {
        getCore()->getEventBus().publish(Events::COMPONENT_READY, metadata.name);
        return ComponentStatus::Success;
    }
    return ComponentStatus::HardwareError;
}
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Tests require hardware | Move to integration tests, use mocks for unit tests |
| File too large | Split by responsibility (SRP) |
| Circular dependency | Use EventBus for decoupling |
| Coverage < 100% | Check for untested branches, edge cases |
