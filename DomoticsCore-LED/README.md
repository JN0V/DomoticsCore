# DomoticsCore-LED

Simple LED control component for DomoticsCore with optional WebUI provider.

## Features

- Configure GPIO for LED output
- On/Off and brightness (PWM) support
- WebUI provider (optional): toggle and slider controls, live status

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

## License

MIT
