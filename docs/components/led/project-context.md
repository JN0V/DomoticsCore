# DomoticsCore-LED -- Project Context (AI Agent Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document is intended for AI coding agents working on the DomoticsCore-LED component. It provides the essential context needed to make safe, constitution-compliant changes.

---

## Component Identity

| Field | Value |
|-------|-------|
| **Library name** | `DomoticsCore-LED` |
| **Component class** | `DomoticsCore::Components::LEDComponent` |
| **Registered name** | `"LEDComponent"` (set in `metadata.name`) -- **Bug:** inconsistent with all other components (should be `"LED"`) |
| **Version** | `1.3.0` (must match in both `library.json` and `metadata.version`) |
| **Category** | `Hardware` |
| **Platforms** | `espressif32`, `espressif8266` |
| **Framework** | Arduino |
| **License** | MIT |
| **Role** | Manages one or more single-color or RGB LEDs with PWM brightness control and a built-in non-blocking effects engine. |

---

## File Inventory

All paths are relative to the repository root.

| File | Purpose |
|------|---------|
| `DomoticsCore-LED/library.json` | PlatformIO library manifest (name, version, dependencies) |
| `DomoticsCore-LED/platformio.ini` | Native test environment configuration |
| `DomoticsCore-LED/README.md` | Component-level README |
| `DomoticsCore-LED/include/DomoticsCore/LED.h` | Main header: `LEDEffect`, `LEDColor`, `LEDConfig`, `LEDState`, `LEDComponent` |
| `DomoticsCore-LED/include/DomoticsCore/LEDWebUI.h` | WebUI provider: `LEDWebUI` (extends `CachingWebUIProvider`) |
| `DomoticsCore-LED/test/test_led_types/test_led_types.cpp` | Unity tests for `LEDColor`, `LEDEffect`, `LEDConfig`, `LEDState` |
| `DomoticsCore-LED/examples/BasicLED/src/main.cpp` | Standalone demo cycling six effects |
| `DomoticsCore-LED/examples/BasicLED/README.md` | Hardware wiring guide and expected output |
| `DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp` | Browser-controlled LED via WiFi AP |

**Note**: This component is header-only. There is no `src/` directory; all implementation lives in the header files under `include/DomoticsCore/`.

---

## Key Classes and Types

| Type | Header | Role |
|------|--------|------|
| `LEDEffect` | `LED.h` | Enum class: `Solid`, `Blink`, `Fade`, `Pulse`, `Rainbow`, `Breathing` |
| `LEDColor` | `LED.h` | RGB color struct with predefined factory methods (`White()`, `Red()`, etc.) |
| `LEDConfig` | `LED.h` | Per-LED configuration: pin(s), RGB flag, max brightness, invert logic, name |
| `LEDState` | `LED.h` | Per-LED runtime state: color, brightness, effect, phase, timing |
| `LEDComponent` | `LED.h` | Main component class; implements `IComponent` lifecycle |
| `LEDWebUI` | `LEDWebUI.h` | WebUI provider; extends `CachingWebUIProvider`; holds a non-owning pointer to `LEDComponent` |

---

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| `DomoticsCore-Core` | `^1.3.0` | Provides `IComponent`, `Core`, `NonBlockingDelay`, `Platform_HAL`, `Logger` |

`LEDWebUI.h` additionally depends on headers from Core's WebUI subsystem (`IWebUIProvider.h`, `BaseWebUIComponents.h`) and `ArduinoJson`. These are transitive through Core.

There are **no other component dependencies**. The LED component does not depend on WiFi, MQTT, Storage, or any other DomoticsCore component.

---

## Conventions

### Naming

- LED configurations are registered by calling `addSingleLED()` or `addRGBLED()` before `begin()`.
- If no name is provided, names are auto-generated as `"LED_<index>"` or `"RGB_<index>"`.
- Control methods accept both a `size_t` index and a `const String&` name. Name-based lookup performs a linear scan.

### Effect Defaults

When `setLEDEffect()` is called:
- If brightness is `0`, it is promoted to `config.maxBrightness`.
- If color is `Off()` (all channels zero), it is promoted to `White()`.
- The LED is force-enabled.

This prevents the common mistake of applying an effect to an LED that has never been explicitly colored/brightened.

### Update Rate

The effect engine ticks at 20 Hz (50 ms `NonBlockingDelay`). The `loop()` method early-returns if the component status is not `Success`.

### WebUI Provider Lifecycle

`LEDWebUI` holds a **non-owning raw pointer** to `LEDComponent`. It must be registered after both the `WebUIComponent` and `LEDComponent` have been added to Core:

```cpp
webui->registerProviderWithComponent(new LEDWebUI(ledComp), ledComp);
```

The WebUI component takes ownership of the `LEDWebUI` pointer.

---

## Pitfalls and Common Mistakes

1. **Calling control methods before `begin()`**: `ledStates` is empty until `begin()` resizes it to match `ledConfigs`. Calls to `setLED()` or `setLEDEffect()` will return `false`.

2. **Forgetting `invertLogic` for common-anode LEDs**: Common-anode RGB LEDs require `invertLogic = true`. Without it, the PWM output is reversed (full brightness produces off, and vice versa).

3. **Rainbow on single-color LEDs**: `LEDEffect::Rainbow` only produces visible color changes on RGB LEDs. On single-color LEDs it has no visible effect because the HSV-to-RGB conversion is applied to channels that are collapsed into a single output.

4. **Adding LEDs after `begin()`**: `addLED()` / `addSingleLED()` / `addRGBLED()` modify only `ledConfigs`. The `ledStates` vector is resized once during `begin()`. LEDs added after `begin()` will not have corresponding state entries and will be ignored.

5. **Name collisions**: There is no uniqueness enforcement on LED names. Duplicate names cause `setLED(name, ...)` to always match the first occurrence.

6. **WebUI initial state**: `LEDWebUI` starts with `enabled = false`. The LED will be OFF until the user toggles the enable switch in the dashboard or `setLED()` is called programmatically.

7. **String allocations in `getLEDNames()`**: This method allocates a new `std::vector<String>` on each call. Avoid calling it in tight loops. The `LEDWebUI` caches names internally for this reason.

8. **`effectDirection` is dead code**: The `LEDState::effectDirection` field is never read or written by any effect update logic. Do not rely on it for new effect implementations. It may be removed in a future cleanup.

---

## Constitution Compliance Reminders

When modifying this component, ensure adherence to these key constitution principles:

- **SOLID (Section I)**: `LEDComponent` implements `IComponent` (DIP). `LEDWebUI` is a separate class (SRP). Keep it that way.
- **TDD (Section II)**: All new behavior must have corresponding tests in `test/test_led_types/`. Write the test first.
- **KISS (Section III)**: The component is header-only and intentionally simple. Resist adding unnecessary abstractions.
- **YAGNI (Section IV)**: Do not add speculative features. Implement only what is required now.
- **Performance (Section V)**: Avoid heap allocations in `loop()` and `updateEffects()`. The 20 Hz tick runs on every frame.
- **HAL Isolation (Section IX)**: All GPIO and timing calls go through `HAL::`. No `#ifdef` platform checks are permitted in LED.h or LEDWebUI.h.
- **Non-Blocking Timer (Section X)**: The component already uses `NonBlockingDelay`. Never introduce `delay()`.
- **File Size (Section VII)**: `LED.h` is approximately 480 lines. Monitor this; the hard limit is 800 lines.
- **Memory Leak Prevention (Section XIV)**: `LEDWebUI` caches LED names. Ensure any new caches are invalidated or bounded. No `new` without corresponding ownership transfer.
- **Semantic Versioning (Section XV)**: Version changes must use `tools/bump_version.py`. The version in `library.json` must match `metadata.version` in `LEDComponent`'s constructor.
- **Documentation (Quality Gates)**: All documentation, specs, and code comments must be in English.

---

## See Also

- [README (Overview)](README.md)
- [Technical Reference](technical-reference.md)
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md)
