# DomoticsCore-WebUI

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What It Is

DomoticsCore-WebUI is the web interface component for the DomoticsCore IoT framework. It serves a modern, responsive dashboard over HTTP and provides real-time data updates to connected browsers via **Server-Sent Events (SSE)** with automatic **polling fallback** for memory-constrained devices. Components expose their UI through the `IWebUIProvider` interface, and WebUI aggregates and renders them dynamically.

## Key Features

- **Real-time updates** -- SSE push on ESP32 (heap >= 20KB), automatic polling fallback on ESP8266
- **Provider pattern** -- Any component implementing `IWebUIProvider` is auto-discovered and rendered
- **Reusable UI components** -- `BaseWebUIComponents` provides progress bars, toggle switches, buttons, sliders, charts, and more
- **Streaming serialization** -- `StreamingContextSerializer` writes JSON directly to chunked HTTP responses without intermediate heap allocation
- **CORS support** -- Configurable cross-origin headers for external frontend or API consumers
- **Headless / API-only mode** -- Use as a pure REST API server without serving any web assets
- **Embedded assets** -- HTML/CSS/JS are gzip-compressed into PROGMEM via `embed_webui.py`; no filesystem required
- **Authentication** -- Optional HTTP basic auth for all endpoints
- **Platform HAL** -- Buffer sizes and client limits adapt automatically to ESP32, ESP8266, or native/test targets

## Quick Start

```cpp
#include <DomoticsCore/WebUI.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// 1. Configure
WebUIConfig cfg;
cfg.setDeviceName("My Device");
cfg.wsUpdateInterval = 2000;

// 2. Register with the core
core.addComponent(std::make_unique<WebUIComponent>(cfg));

// 3. Any component implementing IWebUIProvider is auto-discovered
//    when onComponentsReady() fires.
```

### Providing UI from a Component (Inheritance)

```cpp
class MyComponent : public IComponent, public CachingWebUIProvider {
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        contexts.push_back(
            WebUIContext::settings("my_settings", "My Settings")
                .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true"))
        );
    }
public:
    String getWebUIName() const override { return "MyComponent"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    IWebUIProvider* getWebUIProvider() override { return this; }
    String handleWebUIRequest(const String& ctx, const String&,
                              const String& method,
                              const std::map<String,String>& params) override {
        return "{\"success\":true}";
    }
};
```

### Providing UI via Composition (Recommended)

```cpp
class MyComponentWebUI : public CachingWebUIProvider {
public:
    explicit MyComponentWebUI(MyComponent* c) : comp(c) {}
    // Override buildContexts(), getWebUIName(), handleWebUIRequest()...
private:
    MyComponent* comp;
};

// Register externally
webui->registerProviderWithComponent(new MyComponentWebUI(my), my);
```

### Headless API Mode

```cpp
WebUIConfig config;
config.enableCORS = true;

auto webui = std::make_unique<WebUIComponent>(config);
auto* ptr = webui.get();

ptr->registerApiRoute("/api/sensor", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"value\":42}");
});
```

## REST API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/api/ui/schema` | GET | Full UI schema (chunked streaming) |
| `/api/ui/updates` | GET | Polling updates; add `?schema=1` for schema |
| `/api/ui/action` | GET | Client-to-server UI action (query params) |
| `/api/ui/context?id=X` | GET | Single context schema by ID |
| `/api/ui/events` | SSE | Server-Sent Events stream (ESP32) |
| `/api/components` | GET | List registered providers |
| `/api/components/enable` | POST | Enable/disable a provider at runtime |
| `/api/system/info` | GET | Uptime, heap, and client count |

## Known Issues

### SSE Broadcast WARNING Log Noise

The `sendWebSocketUpdates()` method logs every routine SSE broadcast at WARNING level (`DLOG_W`), producing ~1560 bytes of log output approximately every 5.4 seconds when at least one SSE client is connected. This is a log-severity mismatch -- the message should use DEBUG level. See the [Technical Reference](./technical-reference.md#known-issue-sse-broadcast-log-severity) for details and workarounds.

## Test Suites

| Test | Path | Scope |
|------|------|-------|
| `test_streaming_serializer` | `test/test_streaming_serializer/` | Unit tests for `StreamingContextSerializer` pause/resume and JSON correctness |
| `test_webui_component` | `test/test_webui_component/` | Integration tests for `WebUIComponent` API routes, provider registration, and config |
| `test_schema_memory` | `test/test_schema_memory/` | Memory-profiling tests for schema generation on constrained targets |
| `test_heap_esp8266` | `test/test_heap_esp8266/` | Heap fragmentation and low-memory behavior on ESP8266 |

## Further Reading

- [Technical Reference](./technical-reference.md) -- Full API documentation for all classes and methods
- [Project Context (AI)](./project-context.md) -- AI-oriented context file with conventions and constraints
- [Component README](../../../DomoticsCore-WebUI/README.md) -- Original library README with examples
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md) -- Governing development principles

## License

MIT
