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

## Base UI Components

The library provides reusable HTML generators in `BaseWebUIComponents.h` for consistent UI elements:

```cpp
#include <DomoticsCore/BaseWebUIComponents.h>
using namespace DomoticsCore::Components::WebUI;

// Progress bars
html += BaseWebUIComponents::progressBar("myProgress", "Progress", true);

// Toggle switches  
html += BaseWebUIComponents::toggleSwitch("enabled", "Enabled", true);

// Buttons
html += BaseWebUIComponents::button("submitBtn", "Submit", true); // primary style

// Text inputs
html += BaseWebUIComponents::textInput("url", "URL", "https://...");

// Radio groups
String options[] = {"opt1|Option 1", "opt2|Option 2"};
html += BaseWebUIComponents::radioGroup("myRadio", "Select", options, 2, 0);

// Range sliders
html += BaseWebUIComponents::rangeSlider("brightness", "Brightness", 0, 255, 128);

// Select dropdowns
String opts[] = {"wifi|WiFi", "eth|Ethernet"};
html += BaseWebUIComponents::selectDropdown("mode", "Mode", opts, 2, 0);

// Field rows (label + value display)
html += BaseWebUIComponents::fieldRow("Status", "statusValue", "Ready");

// File inputs with button
html += BaseWebUIComponents::fileInput("fileInput", "selectBtn", "fileName", "Select File");

// Button rows (multiple buttons)
html += BaseWebUIComponents::buttonRow(
    BaseWebUIComponents::button("btn1", "Cancel", false) +
    BaseWebUIComponents::button("btn2", "OK", true)
);

// Line charts for real-time data
WebUIContext chart = BaseWebUIComponents::createLineChart(
    "cpu_chart", "CPU Usage", "cpuCanvas", "cpuValue", "#007acc", "%"
);
```

**Benefits:**
- Consistent styling across all components
- Theme-aware (dark/light mode via CSS variables)
- Automatic colon after labels (CSS `::after`)
- Clean, maintainable code

## Build notes

- Assets are embedded via `DomoticsCore-WebUI/embed_webui.py` (pre-script). Rebuild examples to refresh embedded files.
- No extra configuration is required to serve the dashboard.

## Main behaviors

- **Auto discovery**: any component implementing `IWebUIProvider` (or registered via `registerProviderWithComponent()`) is detected during `onComponentsReady()`.
- **Schema + data**: providers define contexts (`WebUIContext`) while real-time values come from `getWebUIData()`.
- **WebSocket updates**: system summary plus provider contexts are pushed every `wsUpdateInterval` milliseconds.
- **Lifecycle awareness**: providers tied to components via `registerProviderWithComponent()` can be enabled/disabled and trigger component `begin()/shutdown()`.

## API-Only Usage (No Web Interface)

The WebUI component can be used as a pure REST API server without serving any web interface. This is useful for headless IoT devices, mobile app backends, or custom integrations.

### Configuration

```cpp
WebUIConfig config;
config.port = 80;
config.useFileSystem = false;  // No UI assets needed
config.enableAuth = false;     // Use custom API authentication
config.enableCORS = true;      // Enable CORS for cross-origin requests

auto webui = std::make_unique<WebUIComponent>(config);
auto* webuiPtr = webui.get();
```

**CORS Support:**
- Set `config.enableCORS = true` to enable Cross-Origin Resource Sharing
- Automatically adds CORS headers to all responses
- Handles OPTIONS preflight requests
- Default headers: `Access-Control-Allow-Origin: *`
- Allows methods: GET, POST, PUT, DELETE, OPTIONS
- Allows headers: Content-Type, X-API-Key, Authorization

### Register API Endpoints

```cpp
// Register endpoints using registerApiRoute()
webuiPtr->registerApiRoute("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["value"] = getSensorValue();
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
});

webuiPtr->registerApiRoute("/api/control", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("state", true)) {
        return request->send(400, "application/json", 
            "{\"error\":\"Missing state parameter\"}");
    }
    
    String state = request->getParam("state", true)->value();
    setDeviceState(state);
    
    request->send(200, "application/json", "{\"success\":true}");
});
```

### Features

- ✅ **Pure REST API** - No web interface overhead
- ✅ **Custom Authentication** - API keys, bearer tokens, etc.
- ✅ **CORS Support** - Cross-origin requests
- ✅ **JSON Responses** - Standard API format
- ✅ **Lightweight** - Minimal memory usage (~350KB flash, ~35KB RAM)

### Use Cases

- Headless sensor nodes
- Mobile app backends
- Custom dashboards (separate frontend)
- Home Assistant REST sensors
- Node-RED integration
- Machine-to-machine communication

See `DomoticsCore-WebUI/examples/HeadlessAPI` for a complete working example.

---

## Examples

See the examples in each package (e.g. `DomoticsCore-Wifi` → `WifiWithWebUI`) for concrete usage of providers and composed UIs.
- `DomoticsCore-WebUI/examples/WebUIOnly` – minimal demo showing WebUI without other components.
- `DomoticsCore-WebUI/examples/HeadlessAPI` – pure REST API server without web interface.
- `DomoticsCore-Wifi/examples/WifiWithWebUI` – WiFi component with a dedicated WebUI provider wrapper.

## License

MIT
