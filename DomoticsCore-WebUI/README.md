# DomoticsCore-WebUI

Web interface component for DomoticsCore. Serves a modern dashboard (HTML/CSS/JS), exposes a WebSocket data channel, and allows components to publish UI via the `IWebUIProvider` interface.

## Features

- Automatic static UI serving (HTML/CSS/JS) with gzip embedding
- WebSocket real-time updates (system + provider contexts)
- `IWebUIProvider` for multi-context UI per component
- Header status badges, dashboard cards, settings sections, component detail views
- Enable/disable providers at runtime (server-side)

## Installation & Dependencies

- Include headers from `DomoticsCore-WebUI` with prefix `DomoticsCore/` (e.g. `#include <DomoticsCore/WebUI.h>`)
- PlatformIO dependencies (brought by your meta project or installed globally):
  - `ESP32Async/ESPAsyncWebServer @ ^3.8.1`
  - `ESP32Async/AsyncTCP @ ^3.4.8`

## Usage (direct provider via inheritance)

```cpp
#include <DomoticsCore/WebUI.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;

WebUIConfig cfg; cfg.deviceName = "My Device"; cfg.wsUpdateInterval = 2000;
core.addComponent(std::make_unique<WebUIComponent>(cfg));

// Discover providers automatically when components are ready
// Any component implementing IWebUIProvider will be registered.
```

## Provide UI from a component (inheritance pattern)

```cpp
class MyComponent : public IComponent, public virtual IWebUIProvider {
  // ...
  std::vector<WebUIContext> getWebUIContexts() override {
    return { WebUIContext::settings("my_settings", "My Settings")
               .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true")) };
  }
  String handleWebUIRequest(const String& ctx, const String&, const String& method,
                            const std::map<String,String>& params) override {
    // Handle field changes sent via WebSocket
    return "{\"success\":true}";
  }
  String getWebUIName() const override { return "MyComponent"; }
  String getWebUIVersion() const override { return "1.0.0"; }
};
```

## Provide UI for a component without modifying it (composition pattern)

If you prefer to keep library components free of WebUI code (recommended), create a separate provider class and register it with the component instance using `registerProviderWithComponent`:

```cpp
// MyComponentWebUI.h
class MyComponentWebUI : public virtual IWebUIProvider {
 public:
  explicit MyComponentWebUI(MyComponent* c) : comp(c) {}
  std::vector<WebUIContext> getWebUIContexts() override { /* ... */ }
  String handleWebUIRequest(const String&, const String&, const String& method,
                            const std::map<String,String>& params) override { /* ... */ }
  String getWebUIName() const override { return "MyComponent"; }
  String getWebUIVersion() const override { return "1.0.0"; }
 private:
  MyComponent* comp; // non-owning
};

// Registration (after both components exist)
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* my    = core.getComponent<MyComponent>("MyComponent");
if (webui && my) {
  webui->registerProviderWithComponent(new MyComponentWebUI(my), my);
}
```

This pattern keeps library code clean and lets examples/apps decide how to expose UI.

## Build notes

- Assets are embedded via `DomoticsCore-WebUI/embed_webui.py` (pre-script). Rebuild examples to refresh embedded files.
- No extra configuration is required to serve the dashboard.

## Main behaviors

- **Auto discovery**: any component implementing `IWebUIProvider` (or registered via `registerProviderWithComponent()`) is detected during `onComponentsReady()`.
- **Schema + data**: providers define contexts (`WebUIContext`) while real-time values come from `getWebUIData()`.
- **WebSocket updates**: system summary plus provider contexts are pushed every `wsUpdateInterval` milliseconds.
- **Lifecycle awareness**: providers tied to components via `registerProviderWithComponent()` can be enabled/disabled and trigger component `begin()/shutdown()`.

## Examples

See the examples in each package (e.g. `DomoticsCore-Wifi` → `WifiWithWebUI`) for concrete usage of providers and composed UIs.
- `DomoticsCore-WebUI/examples/WebUIOnly` – minimal demo showing WebUI without other components.
- `DomoticsCore-Wifi/examples/WifiWithWebUI` – WiFi component with a dedicated WebUI provider wrapper.

## License

MIT
