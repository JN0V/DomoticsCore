# DomoticsCore-LED

LED management component for DomoticsCore with PWM brightness control, a built-in effects engine, and an optional WebUI provider.

## Features

- Single-color and RGB LED support via `addSingleLED` / `addRGBLED`
- PWM brightness control scaled by `maxBrightness`, with `invertLogic` for common-anode hardware
- Six built-in effects: Solid, Blink, Fade, Pulse, Rainbow, Breathing
- Non-blocking 20 Hz effect engine (`Utils::NonBlockingDelay`)
- Optional WebUI provider (`LEDWebUI`) with live dashboard controls
- Header-only implementation

## Installation & Dependencies

- Requires `DomoticsCore-Core ^1.3.0`.
- Requires Arduino-ESP32 or ESP8266 core for `analogWrite`/GPIO primitives.
- Include headers with prefix `DomoticsCore/`:

```cpp
#include <DomoticsCore/LED.h>
```

## Usage

```cpp
#include <DomoticsCore/LED.h>
using namespace DomoticsCore::Components;

auto led = std::make_unique<LEDComponent>();
led->addSingleLED(LED_BUILTIN, "Status", 255, HAL::isInternalLEDInverted());
led->addRGBLED(18, 19, 21, "MainRGB");
core.addComponent(std::move(led));
```

### With WebUI

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/LEDWebUI.h>

auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* led   = core.getComponent<LEDComponent>("LED");
if (webui && led) {
    webui->registerProviderWithComponent(new LEDWebUI(led), led);
}
```

## Available Effects

| Effect | Enum | Description |
|--------|------|-------------|
| Solid | `LEDEffect::Solid` | Constant brightness, no animation |
| Blink | `LEDEffect::Blink` | On/off toggle at configurable interval |
| Fade | `LEDEffect::Fade` | Smooth sine-wave fade in/out |
| Pulse | `LEDEffect::Pulse` | Heartbeat-style double pulse then rest |
| Rainbow | `LEDEffect::Rainbow` | Continuous hue rotation (RGB LEDs only) |
| Breathing | `LEDEffect::Breathing` | Smooth cosine-based inhale/exhale curve |

## Examples

- [`examples/BasicLED`](examples/BasicLED/) -- standalone demo cycling through all six effects with single and RGB LEDs.
- [`examples/LEDWithWebUI`](examples/LEDWithWebUI/) -- browser-controlled LED via WiFi AP at `http://192.168.4.1`.

## License

MIT
