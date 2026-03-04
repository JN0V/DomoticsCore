# DomoticsCore-LED

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What is DomoticsCore-LED?

DomoticsCore-LED is the hardware LED management component for the DomoticsCore IoT framework. It provides PWM-based brightness control, named LED management, and a built-in effects engine for both single-color and RGB LEDs on ESP32 and ESP8266 platforms.

The component is header-only, registers as `"LEDComponent"` in the Core component registry (note: this name is inconsistent with other components that use short names like `"MQTT"`, `"LED"` would be expected -- see technical reference), and follows the standard `IComponent` lifecycle (`begin` / `loop` / `shutdown`).

## Supported Effects

| Effect | Enum Value | Description |
|--------|------------|-------------|
| Solid | `LEDEffect::Solid` | Constant brightness, no animation |
| Blink | `LEDEffect::Blink` | On/off toggle at a configurable interval |
| Fade | `LEDEffect::Fade` | Smooth sine-wave fade in and out |
| Pulse | `LEDEffect::Pulse` | Heartbeat-style double pulse followed by rest |
| Rainbow | `LEDEffect::Rainbow` | Continuous hue rotation (RGB LEDs only) |
| Breathing | `LEDEffect::Breathing` | Smooth cosine-based inhale/exhale curve |

Effects are driven by a non-blocking 20 Hz timer (`Utils::NonBlockingDelay` at 50 ms). The `effectSpeed` parameter controls the full cycle duration in milliseconds.

## Quick Start

```cpp
#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

void setup() {
    Serial.begin(115200);

    auto led = std::make_unique<LEDComponent>();
    led->addSingleLED(LED_BUILTIN, "Status", 255, HAL::isInternalLEDInverted());
    led->addRGBLED(18, 19, 21, "MainRGB");
    core.addComponent(std::move(led));

    CoreConfig cfg;
    cfg.deviceName = "LEDDevice";
    core.begin(cfg);

    // Set a solid color
    // NOTE: metadata.name is currently "LEDComponent" (should be "LED")
    auto* ledComp = core.getComponent<LEDComponent>("LEDComponent");
    if (ledComp) {
        ledComp->setLED("Status", LEDColor::White(), 128);
        ledComp->setLED("MainRGB", LEDColor::Blue(), 200);
        ledComp->setLEDEffect("MainRGB", LEDEffect::Breathing, 3000);
    }
}

void loop() {
    core.loop();
}
```

## Optional WebUI Integration

Pair the component with `LEDWebUI` to expose a browser-based control panel:

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/LEDWebUI.h>

// After core.addComponent for both WebUIComponent and LEDComponent:
auto* webui = core.getComponent<WebUIComponent>("WebUI");
// NOTE: metadata.name is currently "LEDComponent" (should be "LED")
auto* ledComp = core.getComponent<LEDComponent>("LEDComponent");
if (webui && ledComp) {
    webui->registerProviderWithComponent(new LEDWebUI(ledComp), ledComp);
}
```

The WebUI provider offers LED selection, enable/disable toggle, brightness slider, and effect picker with real-time WebSocket updates.

## Further Reading

- [Technical Reference](technical-reference.md) -- full API documentation, struct definitions, and PWM details.
- [Project Context (AI Agent)](project-context.md) -- file inventory, dependencies, conventions, and constitution compliance notes.
- [BasicLED Example](../../../DomoticsCore-LED/examples/BasicLED/) -- standalone demo cycling through all six effects.
- [LEDWithWebUI Example](../../../DomoticsCore-LED/examples/LEDWithWebUI/) -- browser-controlled LED via WiFi AP.

## License

MIT
