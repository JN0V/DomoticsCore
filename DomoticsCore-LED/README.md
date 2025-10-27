# DomoticsCore-LED

Simple LED control component for DomoticsCore with optional WebUI provider.

## Features

- Configure GPIO for LED output
- On/Off and brightness (PWM) support
- WebUI provider (optional): toggle and slider controls, live status

## Installation & Dependencies

- Requires Arduino-ESP32 core for `analogWrite`/GPIO primitives.
- Include headers with prefix `DomoticsCore/`:

```cpp
#include <DomoticsCore/LED.h>
```

## Usage

```cpp
#include <DomoticsCore/LED.h>
using namespace DomoticsCore::Components;

core.addComponent(std::make_unique<LEDComponent>(/* pin */ 2));
```

### With WebUI

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/LEDWebUI.h>

core.addComponent(std::make_unique<WebUIComponent>());
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* led   = core.getComponent<LEDComponent>("LED");
if (webui && led) {
  webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::LEDWebUI(led), led);
}
```

## Main behaviors

- Supports single-color or RGB LEDs via `addSingleLED` / `addRGBLED`.
- PWM brightness is scaled by `maxBrightness` and respects `invertLogic` for common-anode hardware.
- Built-in effects (`LEDEffect`) update using a 20 Hz timer (`Utils::NonBlockingDelay`).
- Available effects:
  - `Solid` – constant brightness.
  - `Blink` – on/off toggle at `effectSpeed` interval.
  - `Fade` – smooth sine-wave fade in/out.
  - `Pulse` – heartbeat-style double pulse then rest.
  - `Rainbow` – RGB color cycle (for RGB LEDs).
  - `Breathing` – smooth inhale/exhale curve.

## Examples

- `DomoticsCore-LED/examples/LEDNoWebUI` – toggles LEDs and demonstrates effects via serial.
- `DomoticsCore-LED/examples/LEDWithWebUI` – wraps the component with WebUI controls.

## License

MIT
