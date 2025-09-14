# LED Demo Example

This example demonstrates the comprehensive LED management capabilities of the DomoticsCore LED component.

## Features Demonstrated

- **Multiple LED Types**: Single LEDs and RGB LEDs
- **PWM Control**: Smooth brightness control and color mixing
- **LED Effects**: 6 different visual effects with configurable timing
- **Hardware Abstraction**: Support for both common cathode and common anode RGB LEDs
- **Configuration Validation**: Pin conflict detection and hardware validation
- **Status Reporting**: Component status and error reporting

## Hardware Setup

### Required Connections

| Component | Pin(s) | Description |
|-----------|--------|-------------|
| Built-in LED | 2 | ESP32 built-in LED |
| Status LED | 4 | Single white/colored LED |
| Activity LED | 16 | Single white/colored LED |
| Error LED | 17 | Single red LED |
| Main RGB LED | 18, 19, 21 | Common cathode RGB LED (R, G, B) |
| Secondary RGB | 22, 23, 25 | Common anode RGB LED (R, G, B) |

### LED Wiring

**Single LEDs:**
```
ESP32 Pin → 220Ω Resistor → LED Anode → LED Cathode → GND
```

**Common Cathode RGB LED:**
```
ESP32 Pin 18 → 220Ω → Red Anode
ESP32 Pin 19 → 220Ω → Green Anode  } → Common Cathode → GND
ESP32 Pin 21 → 220Ω → Blue Anode
```

**Common Anode RGB LED:**
```
3.3V → Common Anode
ESP32 Pin 22 → 220Ω → Red Cathode
ESP32 Pin 23 → 220Ω → Green Cathode
ESP32 Pin 25 → 220Ω → Blue Cathode
```

## Demo Effects

The demo cycles through 6 different effects every 5 seconds:

1. **Solid Colors** - Static colors with different brightness levels
2. **Blinking Effects** - On/off blinking at various speeds
3. **Fade Effects** - Smooth sine wave fading
4. **Pulse Effects** - Heartbeat-like double pulse
5. **Rainbow Effects** - Color cycling (RGB LEDs only)
6. **Breathing Effects** - Smooth breathing pattern

## Building and Running

```bash
cd examples/LEDDemo
pio run
pio run --target upload
pio device monitor
```

## Expected Output

```
=== DomoticsCore LED Demo ===
Demonstrates comprehensive LED management with effects

[Setup] Adding LED demonstration component...
[LEDDemo] Initializing LED demonstration component...
[LED] Initializing LED component...
[LED] Initialized 6 LEDs successfully
[LEDDemo] Initialized with 6 LEDs
[LEDDemo] - LED: BuiltinLED
[LEDDemo] - LED: StatusLED
[LEDDemo] - LED: ActivityLED
[LEDDemo] - LED: ErrorLED
[LEDDemo] - LED: MainRGB
[LEDDemo] - LED: SecondaryRGB
[LEDDemo] Starting demo 1/6
[LEDDemo] Demo: Solid Colors
[Setup] LED Demo ready!
```

## Component Architecture

The LED component demonstrates:

- **Configuration System**: Type-safe LED configuration with validation
- **Status Reporting**: Detailed error reporting with ComponentStatus enum
- **Hardware Abstraction**: Support for different LED types and logic levels
- **Effect Engine**: Real-time effect calculation and PWM output
- **Memory Efficiency**: Header-only component with minimal overhead

## Customization

You can modify the LED configuration in `main.cpp`:

```cpp
// Add your own LEDs
ledManager->addSingleLED(yourPin, "YourLED", maxBrightness, invertLogic);
ledManager->addRGBLED(rPin, gPin, bPin, "YourRGB", maxBrightness, invertLogic);

// Control LEDs programmatically
ledManager->setLED("YourLED", LEDColor::Blue(), 200);
ledManager->setLEDEffect("YourLED", LEDEffect::Fade, 2000);
```

This example showcases the power and flexibility of the DomoticsCore component architecture for hardware control applications.
