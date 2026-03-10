# DomoticsCore-LED -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides the complete API surface of the DomoticsCore-LED component, including all public types, structs, methods, and the WebUI provider.

---

## Namespace

All types live under:

```cpp
namespace DomoticsCore {
namespace Components {
    // LEDEffect, LEDColor, LEDConfig, LEDState, LEDComponent, LEDWebUI
}}
```

---

## LEDEffect Enum

```cpp
enum class LEDEffect {
    Solid,      // Constant brightness -- no animation
    Blink,      // On/off toggle; 50 % duty cycle
    Fade,       // Sine-wave brightness oscillation
    Pulse,      // Heartbeat double-bump then rest
    Rainbow,    // HSV hue rotation (RGB LEDs only)
    Breathing   // Cosine-based smooth inhale/exhale
};
```

### Effect Timing

Each effect uses a phase variable (`0.0` to `1.0`) that advances according to `effectSpeed` (milliseconds for one full cycle). The update tick runs at 20 Hz (50 ms non-blocking timer).

| Effect | Formula (simplified) | Notes |
|--------|----------------------|-------|
| Blink | `phase < 0.5 ? brightness : 0` | Hard on/off |
| Fade | `brightness * (sin(phase * 2PI) + 1) / 2` | Smooth sine wave |
| Pulse | First sine bump over 0-30 % of cycle, second bump over 30-50 %, off for 50-100 % | Heartbeat feel |
| Rainbow | Phase mapped to 0-360 hue, simple HSV-to-RGB | Only affects RGB LEDs |
| Breathing | `brightness * (1 - cos(phase * 2PI)) / 2` | Perceptually smooth |

---

## LEDColor Struct

```cpp
struct LEDColor {
    uint8_t red   = 0;
    uint8_t green = 0;
    uint8_t blue  = 0;

    LEDColor();
    LEDColor(uint8_t r, uint8_t g, uint8_t b);

    // Predefined factory methods
    static LEDColor White();    // (255, 255, 255)
    static LEDColor Red();      // (255, 0, 0)
    static LEDColor Green();    // (0, 255, 0)
    static LEDColor Blue();     // (0, 0, 255)
    static LEDColor Yellow();   // (255, 255, 0)
    static LEDColor Cyan();     // (0, 255, 255)
    static LEDColor Magenta();  // (255, 0, 255)
    static LEDColor Off();      // (0, 0, 0)
};
```

For single-color LEDs, `LEDColor` still applies: any non-zero channel maps to "on" at the configured brightness. Only RGB LEDs produce actual color mixing.

---

## LEDConfig Struct

```cpp
struct LEDConfig {
    int      pin            = -1;     // GPIO for single-color LED
    bool     isRGB          = false;  // true for RGB mode
    int      redPin         = -1;     // GPIO for red channel
    int      greenPin       = -1;     // GPIO for green channel
    int      bluePin        = -1;     // GPIO for blue channel
    uint8_t  maxBrightness  = 255;    // PWM ceiling (0-255)
    bool     invertLogic    = false;  // true for common-anode RGB LEDs
    String   name           = "";     // Friendly identifier
};
```

### Pin Validation Rules

- **Single-color LED**: `pin` must be >= 0.
- **RGB LED**: all three of `redPin`, `greenPin`, `bluePin` must be >= 0.
- Validation runs during `begin()`. Invalid pins cause `ComponentStatus::ConfigError`.

---

## LEDState Struct

```cpp
struct LEDState {
    LEDColor      currentColor;
    uint8_t       brightness      = 0;
    LEDEffect     effect          = LEDEffect::Solid;
    unsigned long effectSpeed     = 1000;   // ms per cycle
    bool          enabled         = true;

    // Internal effect engine state
    unsigned long lastUpdate      = 0;
    float         effectPhase     = 0.0;    // 0.0 -- 1.0
};
```

`LEDState` is managed internally. One entry exists per configured LED.

---

## LEDComponent Class

```cpp
class LEDComponent : public IComponent { ... };
```

### Metadata

| Field | Value |
|-------|-------|
| `metadata.name` | `"LED"` |
| `metadata.version` | `"1.4.0"` |
| `metadata.author` | `"DomoticsCore"` |
| `metadata.category` | `"Hardware"` |
| `metadata.tags` | `{"led", "pwm", "effects", "hardware"}` |

### Constructor

```cpp
LEDComponent();
```

Initializes the 20 Hz update timer (`Utils::NonBlockingDelay(50)`).

### IComponent Lifecycle Methods

#### `ComponentStatus begin()`

1. Validates all LED pin assignments via `validateLEDPins()`.
2. Configures GPIO pins as `OUTPUT` and writes initial PWM value of 0.
3. Resizes `ledStates` to match `ledConfigs`, initializing each to off/solid/enabled.
4. Returns `ComponentStatus::Success` or `ComponentStatus::ConfigError`.

#### `void loop()`

If the component status is `Success` and the non-blocking timer has fired, calls `updateEffects()` to advance all active LED effect animations and write PWM outputs.

#### `ComponentStatus shutdown()`

Turns off all LEDs by writing `LEDColor::Off()` at brightness 0 to every configured output.

### Configuration Methods

#### `void addLED(const LEDConfig& config)`

Appends a fully specified `LEDConfig` to the internal configuration list. Must be called before `begin()`.

#### `void addSingleLED(int pin, const String& name = "", uint8_t maxBrightness = 255, bool invertLogic = false)`

Convenience wrapper that builds an `LEDConfig` for a single-channel LED and calls `addLED()`. If `name` is empty, auto-generates `"LED_<index>"`.

#### `void addRGBLED(int redPin, int greenPin, int bluePin, const String& name = "", uint8_t maxBrightness = 255, bool invertLogic = false)`

Convenience wrapper for a three-channel RGB LED. If `name` is empty, auto-generates `"RGB_<index>"`.

### Control Methods

#### `bool setLED(size_t ledIndex, const LEDColor& color, uint8_t brightness = 255)`

Sets the color and brightness for the LED at `ledIndex`. Resets the effect to `LEDEffect::Solid`. Returns `false` if `ledIndex` is out of range.

#### `bool setLED(const String& name, const LEDColor& color, uint8_t brightness = 255)`

Name-based overload. Performs a linear scan of `ledConfigs` to find the matching name.

#### `bool setLEDEffect(size_t ledIndex, LEDEffect effect, unsigned long speed = 1000)`

Applies an animated effect to the specified LED. Behavior details:
- Resets `effectPhase` to `0.0` and records `lastUpdate`.
- Enables the LED if it was disabled.
- If `brightness` is `0`, defaults to `maxBrightness` from the LED config.
- If `currentColor` is `Off()`, defaults to `White()`.

Returns `false` if `ledIndex` is out of range.

#### `bool setLEDEffect(const String& name, LEDEffect effect, unsigned long speed = 1000)`

Name-based overload.

#### `bool enableLED(size_t ledIndex, bool enabled = true)`

Enables or disables an LED. Disabled LEDs are immediately forced off (`LEDColor::Off()`, brightness 0) and skipped during effect updates. Returns `false` if out of range.

#### `bool enableLED(const String& name, bool enabled = true)`

Name-based overload.

### Query Methods

#### `size_t getLEDCount() const`

Returns the number of configured LEDs (single + RGB entries combined).

#### `std::vector<String> getLEDNames() const`

Returns a vector of friendly names for all configured LEDs, in registration order.

#### `String getLEDStatus(size_t ledIndex) const`

Returns a human-readable string describing the LED state, for example:
```
LED 'MainRGB': Enabled, Color: RGB(0,0,255), Brightness: 200, Effect: Breathing
```

#### `String getEffectName(LEDEffect effect) const`

Maps an `LEDEffect` enum value to its display string (`"Solid"`, `"Blink"`, `"Fade"`, `"Pulse"`, `"Rainbow"`, `"Breathing"`, or `"Unknown"`).

### Private Helper Methods

These methods are internal implementation details but are documented here for completeness.

#### `bool validateLEDPins()`

Iterates over all `ledConfigs` and verifies that single-color LEDs have `pin >= 0` and RGB LEDs have all three channel pins `>= 0`. Logs an error via `DLOG_E` and returns `false` on the first invalid entry.

#### `void initializePins()`

Sets all configured GPIO pins to `OUTPUT` mode via `HAL::pinMode()` and writes an initial PWM value of `0` (respecting `invertLogic`).

#### `void setPWMOutput(int pin, uint8_t value, bool invert)`

Low-level helper that writes a single PWM value to a pin. If `invert` is `true`, the output is `255 - value`. Guards against `pin < 0`.

#### `void setLEDOutput(size_t ledIndex, const LEDColor& color, uint8_t brightness)`

Applies the two-stage brightness scaling pipeline (see PWM Control Details below), then writes the computed PWM values to the appropriate pins.

#### `void updateEffects()`

Called by `loop()` at 20 Hz. For each enabled LED with a non-Solid effect, advances `effectPhase` based on elapsed time and `effectSpeed`, computes the output brightness/color per the effect formula, and calls `setLEDOutput()`. Solid-effect LEDs are written once per tick without phase advancement.

---

## Test Coverage

Two test suites exist:

| Test File | Scope |
|-----------|-------|
| `test/test_led_types/test_led_types.cpp` | Pure data-type tests: `LEDColor` constructors and predefined colors, `LEDEffect` enum values, `LEDConfig` default values and field assignment, `LEDState` defaults. 17 test cases. |
| `test/test_led_component/test_led_component.cpp` | Component integration tests: verifies `metadata.name` is `"LED"` and that `Core::getComponent<LEDComponent>("LED")` succeeds after registration. 2 test cases. |

Tests run on the `native` platform using the Unity framework (`platformio.ini` at the component root).

---

## PWM Control Details

### Brightness Scaling

All brightness values pass through a two-stage scaling pipeline:

1. **Max brightness clamp**: `scaledBrightness = map(brightness, 0, 255, 0, config.maxBrightness)`
2. **Per-channel scaling** (RGB only): `channelValue = map(color.channel, 0, 255, 0, scaledBrightness)`

For single-color LEDs, any non-zero color channel results in `scaledBrightness` being written; all-zero color channels produce 0.

### Inverted Logic (Common Anode)

When `LEDConfig::invertLogic` is `true`, the actual PWM output value is `255 - computedValue`. This supports common-anode RGB LEDs where the anode is tied to VCC and pulling a pin LOW turns the channel ON.

### HAL Abstraction

All GPIO operations go through the platform HAL:
- `HAL::pinMode(pin, OUTPUT)` -- configures the pin direction.
- `HAL::analogWrite(pin, value)` -- writes the 8-bit PWM duty cycle.
- `HAL::getMillis()` -- provides the monotonic clock for effect timing.
- `HAL::map(value, fromLow, fromHigh, toLow, toHigh)` -- linear interpolation.

---

## LEDWebUI Provider

```cpp
class LEDWebUI : public CachingWebUIProvider { ... };
```

Declared in `<DomoticsCore/LEDWebUI.h>`. Provides a browser-based control panel through the DomoticsCore WebUI system.

### Constructor

```cpp
explicit LEDWebUI(LEDComponent* comp);
```

Takes a **non-owning** pointer to the `LEDComponent`. The provider does not manage the component lifetime.

### WebUI Contexts

The provider registers two contexts:

| Context ID | Type | Description |
|------------|------|-------------|
| `led_status` | Status Badge | Shows ON/OFF state with a bulb icon. Real-time update at 1000 ms. |
| `led_dashboard` | Dashboard | LED selector, enable toggle, brightness slider, effect dropdown. Real-time update at 1000 ms. |

### Dashboard Fields

| Field ID | Type | Default | Description |
|----------|------|---------|-------------|
| `led_select` | Select | First LED name | Dropdown of all configured LED names |
| `enabled_toggle` | Boolean | `false` | Enables or disables the selected LED |
| `brightness` | Slider (0-255) | `128` | PWM brightness level |
| `effect` | Select | `"Solid"` | Effect picker: Solid, Blink, Fade, Pulse, Rainbow, Breathing |

### Overridden Methods

#### `void buildContexts(std::vector<WebUIContext>& contexts)` (protected)

Registers the two WebUI contexts (`led_status` and `led_dashboard`) with their field definitions and real-time update intervals. Calls `ensureInitialized()` to push the initial state to hardware on first invocation.

#### `String getWebUIName() const`

Returns `metadata.name` from the linked `LEDComponent` (`"LED"`).

#### `String getWebUIVersion() const`

Returns `metadata.version` from the linked `LEDComponent` (typically `"1.4.0"`). Falls back to `"1.3.0"` if the component pointer is null.

#### `String getWebUIData(const String& contextId)`

Returns a JSON object with the current values for the requested context. Used by the WebSocket real-time update loop.

#### `String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params)`

Processes POST requests from the dashboard. Expects `field` and `value` in `params`. Updates internal mirrored state and delegates to `LEDComponent` methods. Returns `{"success":true}` or `{"success":false}`.

### Internal State

The WebUI maintains a mirrored state to synchronize the UI with hardware:
- `selected` -- index of the currently selected LED.
- `enabled` -- whether the selected LED is on (starts `false`).
- `brightness` -- current brightness (starts `128`).
- `effect` -- current effect (starts `Solid`).
- `initialApplied` -- flag to ensure initial state is pushed to hardware once.

LED name lookups are cached via `getCachedNames()` to avoid repeated vector allocations.

### Private Helpers

#### `static String effectToString(LEDEffect e)`

Maps an `LEDEffect` enum to its display string. Used by `getWebUIData()` to serialize the current effect into JSON. Returns `"Solid"` for unknown values (unlike `LEDComponent::getEffectName()` which returns `"Unknown"`).

#### `static LEDEffect stringToEffect(const String& s)`

Parses a display string back into an `LEDEffect` enum. Used by `handleWebUIRequest()` to deserialize the effect field from POST params. Defaults to `LEDEffect::Solid` for unrecognized strings.

#### `void ensureInitialized()`

Called once (guarded by `initialApplied` flag) during the first `buildContexts()` invocation. Applies the initial mirrored state to the hardware: if `enabled` is true, sets the LED to white at the current brightness with the current effect; otherwise forces the LED off.

#### `const std::vector<String>& getCachedNames() const`

Lazy-loads and caches LED names from `LEDComponent::getLEDNames()`. If the component returns an empty list, generates synthetic names (`"LED_0"`, `"LED_1"`, ...). The cache is populated once and never invalidated, which means LEDs added after the first WebUI context build will not appear.

---

## See Also

- [README (Overview)](README.md)
- [Project Context (AI Agent)](project-context.md)
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md)
