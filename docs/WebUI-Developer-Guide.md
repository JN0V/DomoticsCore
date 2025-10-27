# WebUI Developer Guide

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Creating a WebUI Provider](#creating-a-webui-provider)
4. [State Tracking with LazyState](#state-tracking-with-lazystate)
5. [UI Contexts](#ui-contexts)
6. [WebSocket Updates](#websocket-updates)
7. [Best Practices](#best-practices)
8. [Complete Examples](#complete-examples)

---

## Overview

The DomoticsCore WebUI system provides a modular, component-based web interface for ESP32 applications. Each component can expose its functionality through a **WebUI Provider** that defines how the component appears in different parts of the UI.

### Key Features

- **Multi-context support**: Components can display in Dashboard, Settings, Status badges, etc.
- **Real-time updates**: WebSocket-based live data streaming
- **Optimized bandwidth**: State tracking prevents unnecessary updates
- **Type-safe**: Template-based LazyState helper for change detection
- **Modular**: Each component provides its own UI independently

---

## Architecture

### Component Hierarchy

```
WebUIComponent (Core)
    ├── IWebUIProvider (Interface)
    │   ├── WifiWebUI
    │   ├── NTPWebUI
    │   ├── MQTTWebUI
    │   └── [Your Custom WebUI]
    │
    └── WebUI Server (AsyncWebServer)
        ├── HTTP Routes
        ├── WebSocket Handler
        └── Update Scheduler
```

### Data Flow

```
Component State Change
    ↓
hasDataChanged() called (LazyState check)
    ↓
if changed → getWebUIData() called
    ↓
JSON serialization
    ↓
WebSocket broadcast to clients
    ↓
Browser updates UI
```

---

## Creating a WebUI Provider

### Step 1: Define the Provider Class

```cpp
#include <DomoticsCore/IWebUIProvider.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

class MyComponentWebUI : public IWebUIProvider {
private:
    MyComponent* component;  // Non-owning pointer
    
    // State tracking (optional, for optimization)
    LazyState<bool> enabledState;
    LazyState<String> statusState;
    
public:
    explicit MyComponentWebUI(MyComponent* comp) : component(comp) {}
    
    // Required interface methods
    String getWebUIName() const override;
    String getWebUIVersion() const override;
    std::vector<WebUIContext> getWebUIContexts() override;
    String getWebUIData(const String& contextId) override;
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override;
    
    // Optional: for bandwidth optimization
    bool hasDataChanged(const String& contextId) override;
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
```

### Step 2: Implement Required Methods

#### **getWebUIName() & getWebUIVersion()**

```cpp
String getWebUIName() const override {
    return component ? component->getName() : String("MyComponent");
}

String getWebUIVersion() const override {
    return "1.0.0";
}
```

#### **getWebUIContexts()**

Define where and how your component appears in the UI:

```cpp
std::vector<WebUIContext> getWebUIContexts() override {
    std::vector<WebUIContext> contexts;
    if (!component) return contexts;
    
    // 1. Status badge (top-right corner)
    contexts.push_back(
        WebUIContext::statusBadge("mycomp_status", "MyComp", "dc-myicon")
            .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean,
                                 component->isEnabled() ? "true" : "false"))
            .withAPI("/api/mycomp/status")
            .withRealTime(5000)  // Update every 5 seconds
    );
    
    // 2. Dashboard card
    WebUIContext dashboard = WebUIContext::dashboard("mycomp_dash", "My Component", "dc-myicon");
    dashboard.withField(WebUIField("status", "Status", WebUIFieldType::Display, 
                                   component->getStatus(), "", true))
             .withField(WebUIField("value", "Current Value", WebUIFieldType::Display,
                                   String(component->getValue()), "", true))
             .withRealTime(1000)
             .withPriority(50);
    contexts.push_back(dashboard);
    
    // 3. Settings panel
    WebUIContext settings = WebUIContext::settings("mycomp_settings", "My Component Settings", "dc-settings");
    settings.withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean,
                                  component->isEnabled() ? "true" : "false"))
            .withField(WebUIField("threshold", "Threshold", WebUIFieldType::Number,
                                  String(component->getThreshold())))
            .withAPI("/api/mycomp/settings");
    contexts.push_back(settings);
    
    return contexts;
}
```

#### **getWebUIData()**

Provide real-time data for contexts:

```cpp
String getWebUIData(const String& contextId) override {
    if (!component) return "{}";
    
    JsonDocument doc;
    
    if (contextId == "mycomp_status") {
        doc["enabled"] = component->isEnabled();
        doc["status"] = component->getStatus();
    }
    else if (contextId == "mycomp_dash") {
        doc["status"] = component->getStatus();
        doc["value"] = component->getValue();
        doc["lastUpdate"] = component->getLastUpdateTime();
    }
    else if (contextId == "mycomp_settings") {
        doc["enabled"] = component->isEnabled();
        doc["threshold"] = component->getThreshold();
    }
    
    String json;
    serializeJson(doc, json);
    return json;
}
```

#### **handleWebUIRequest()**

Handle user interactions (button clicks, setting changes):

```cpp
String handleWebUIRequest(const String& contextId, const String& endpoint,
                         const String& method, const std::map<String, String>& params) override {
    if (!component) {
        return "{\"success\":false,\"error\":\"Component not available\"}";
    }
    
    if (contextId == "mycomp_settings" && method == "POST") {
        auto fieldIt = params.find("field");
        auto valueIt = params.find("value");
        
        if (fieldIt != params.end() && valueIt != params.end()) {
            const String& field = fieldIt->second;
            const String& value = valueIt->second;
            
            if (field == "enabled") {
                component->setEnabled(value == "true" || value == "1");
                return "{\"success\":true}";
            }
            else if (field == "threshold") {
                component->setThreshold(value.toInt());
                return "{\"success\":true}";
            }
        }
    }
    
    return "{\"success\":false,\"error\":\"Unknown request\"}";
}
```

### Step 3: Register the Provider

In your application:

```cpp
void setup() {
    // Add components
    core.addComponent(std::make_unique<WebUIComponent>());
    core.addComponent(std::make_unique<MyComponent>());
    
    // Initialize
    core.begin();
    
    // Register WebUI provider
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* mycomp = core.getComponent<MyComponent>("MyComponent");
    
    if (webui && mycomp) {
        webui->registerProviderWithComponent(new MyComponentWebUI(mycomp), mycomp);
    }
}
```

---

## State Tracking with LazyState

### Why State Tracking?

WebSocket updates are sent periodically (every 1-5 seconds). Without state tracking, you send the same data repeatedly even when nothing changed, wasting bandwidth and CPU.

### The LazyState Solution

`LazyState<T>` provides automatic change detection with zero boilerplate:

```cpp
template<typename T>
struct LazyState {
    bool hasChanged(const T& current);     // Check if value changed
    const T& getValue() const;             // Get stored value
    bool isInitialized() const;            // Check initialization
    void reset();                          // Reset state
};
```

### Pattern 1: Simple State

```cpp
class MyWebUI : public IWebUIProvider {
private:
    LazyState<bool> enabledState;
    LazyState<int> valueState;
    LazyState<String> statusState;
    
public:
    bool hasDataChanged(const String& contextId) override {
        if (!component) return false;
        
        if (contextId == "status") {
            return enabledState.hasChanged(component->isEnabled());
        }
        else if (contextId == "dashboard") {
            // Multiple states - return true if ANY changed
            bool changed = false;
            changed |= valueState.hasChanged(component->getValue());
            changed |= statusState.hasChanged(component->getStatus());
            return changed;
        }
        
        return false;  // Unknown context, don't send
    }
};
```

### Pattern 2: Composite State

For tracking multiple related values together:

```cpp
class WifiWebUI : public IWebUIProvider {
private:
    struct ConnectionState {
        bool connected;
        String ssid;
        String ip;
        
        bool operator==(const ConnectionState& other) const {
            return connected == other.connected && 
                   ssid == other.ssid && 
                   ip == other.ip;
        }
        bool operator!=(const ConnectionState& other) const {
            return !(*this == other);
        }
    };
    
    LazyState<ConnectionState> connState;
    
public:
    bool hasDataChanged(const String& contextId) override {
        if (contextId == "wifi_info") {
            ConnectionState current = {
                wifi->isConnected(),
                wifi->getSSID(),
                wifi->getLocalIP()
            };
            return connState.hasChanged(current);
        }
        return false;
    }
};
```

**Key Points:**
- ✅ First call returns `false` (no false positives)
- ✅ Works regardless of initialization order
- ✅ No manual `initialized` flag needed
- ✅ Type-safe with compile-time checking

---

## UI Contexts

### Available Locations

```cpp
enum class WebUILocation {
    Dashboard,          // Main dashboard cards
    ComponentDetail,    // Detailed component view
    HeaderStatus,       // Top-right status badges
    QuickControls,      // Sidebar quick actions
    Settings,           // Settings/configuration panels
    HeaderInfo          // Main header info zone (time, uptime, etc.)
};
```

### Factory Methods

#### Status Badge
```cpp
WebUIContext::statusBadge("context_id", "Label", "icon-name")
    .withField(WebUIField("status", "Status", WebUIFieldType::Boolean, "true"))
    .withRealTime(5000)
    .withAPI("/api/component/status");
```

#### Dashboard Card
```cpp
WebUIContext::dashboard("context_id", "Title", "icon-name")
    .withField(WebUIField("value", "Value", WebUIFieldType::Display, "123"))
    .withRealTime(1000)
    .withPriority(100);  // Higher = appears first
```

#### Settings Panel
```cpp
WebUIContext::settings("context_id", "Settings Title", "icon-name")
    .withField(WebUIField("option", "Option", WebUIFieldType::Boolean, "false"))
    .withAPI("/api/component/settings");
```

#### Header Info
```cpp
WebUIContext::headerInfo("context_id", "Label", "icon-name")
    .withField(WebUIField("time", "Time", WebUIFieldType::Display, "12:34:56", "", true))
    .withRealTime(1000);
```

### Field Types

```cpp
enum class WebUIFieldType {
    Text,              // Text input/display
    Number,            // Number input/display
    Float,             // Float input/display
    Boolean,           // Checkbox/toggle
    Select,            // Dropdown selection
    Slider,            // Range slider
    Color,             // Color picker
    Button,            // Action button
    Display,           // Read-only display (most common)
    Chart,             // Chart data
    Status,            // Status indicator
    Progress           // Progress value
};
```

---

## WebSocket Updates

### Update Flow

1. **WebUIComponent** calls `hasDataChanged()` on schedule (per context interval)
2. If `true`, calls `getWebUIData()` to get fresh JSON
3. Broadcasts JSON to all connected WebSocket clients
4. Browser JavaScript updates the UI dynamically

### Update Intervals

```cpp
// Fast updates (1 second) - for time, sensors
context.withRealTime(1000);

// Medium updates (5 seconds) - for status, connection info
context.withRealTime(5000);

// Slow updates (30 seconds) - for statistics, configuration
context.withRealTime(30000);
```

### Optimization Tips

1. **Use LazyState** - Only send when data actually changes
2. **Choose appropriate intervals** - Don't update static data every second
3. **Minimize JSON size** - Only send necessary fields
4. **Batch related data** - Combine related values in one context

---

## Best Practices

### ✅ **DO:**

- **Use LazyState for all state tracking** - Prevents false positives and saves bandwidth
- **Define equality operators** - For composite state structs
- **Choose appropriate update intervals** - Match data volatility
- **Return minimal JSON** - Only include changed/relevant fields
- **Use non-owning pointers** - WebUI providers don't own components
- **Register after core.begin()** - Ensures component is initialized (if using constructor init)
- **Handle null components** - Always check `if (!component) return ...`

### ❌ **DON'T:**

- **Don't initialize state in constructor** - Use LazyState (unless provider created after init)
- **Don't track initialized flags manually** - LazyState handles it
- **Don't send updates too frequently** - Wastes bandwidth and CPU
- **Don't use pointers as state types** - Use values for LazyState
- **Don't call getValue() before isInitialized()** - In validation code
- **Don't return huge JSON documents** - Keep WebSocket messages small

---

## Complete Examples

### Example 1: Simple Sensor WebUI

```cpp
class SensorWebUI : public IWebUIProvider {
private:
    SensorComponent* sensor;
    LazyState<float> temperatureState;
    LazyState<float> humidityState;
    
public:
    explicit SensorWebUI(SensorComponent* comp) : sensor(comp) {}
    
    String getWebUIName() const override {
        return "Sensor";
    }
    
    String getWebUIVersion() const override {
        return "1.0.0";
    }
    
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!sensor) return contexts;
        
        // Dashboard card with live sensor readings
        WebUIContext dashboard = WebUIContext::dashboard("sensor_dash", "Environment", "dc-temp");
        dashboard.withField(WebUIField("temperature", "Temperature", WebUIFieldType::Display,
                                       String(sensor->getTemperature(), 1) + "°C", "", true))
                 .withField(WebUIField("humidity", "Humidity", WebUIFieldType::Display,
                                       String(sensor->getHumidity(), 1) + "%", "", true))
                 .withRealTime(2000)  // Update every 2 seconds
                 .withPriority(90);
        
        contexts.push_back(dashboard);
        return contexts;
    }
    
    String getWebUIData(const String& contextId) override {
        if (!sensor) return "{}";
        
        if (contextId == "sensor_dash") {
            JsonDocument doc;
            doc["temperature"] = String(sensor->getTemperature(), 1) + "°C";
            doc["humidity"] = String(sensor->getHumidity(), 1) + "%";
            
            String json;
            serializeJson(doc, json);
            return json;
        }
        
        return "{}";
    }
    
    bool hasDataChanged(const String& contextId) override {
        if (!sensor) return false;
        
        if (contextId == "sensor_dash") {
            bool changed = false;
            changed |= temperatureState.hasChanged(sensor->getTemperature());
            changed |= humidityState.hasChanged(sensor->getHumidity());
            return changed;
        }
        
        return false;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                             const String& method, const std::map<String, String>& params) override {
        return "{\"success\":false,\"error\":\"No actions available\"}";
    }
};
```

### Example 2: Configuration WebUI with Settings

```cpp
class ConfigWebUI : public IWebUIProvider {
private:
    ConfigComponent* config;
    
    struct ConfigState {
        bool enabled;
        String mode;
        int interval;
        
        bool operator==(const ConfigState& other) const {
            return enabled == other.enabled && 
                   mode == other.mode && 
                   interval == other.interval;
        }
        bool operator!=(const ConfigState& other) const { return !(*this == other); }
    };
    LazyState<ConfigState> configState;
    
public:
    explicit ConfigWebUI(ConfigComponent* comp) : config(comp) {}
    
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!config) return contexts;
        
        // Settings panel
        WebUIContext settings = WebUIContext::settings("config_settings", "Configuration", "dc-settings");
        settings.withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean,
                                      config->isEnabled() ? "true" : "false"))
                .withField(WebUIField("mode", "Mode", WebUIFieldType::Select, config->getMode())
                          .choices({"auto", "manual", "scheduled"}))
                .withField(WebUIField("interval", "Interval (sec)", WebUIFieldType::Number,
                                      String(config->getInterval()))
                          .range(1, 3600))
                .withAPI("/api/config/settings");
        
        contexts.push_back(settings);
        return contexts;
    }
    
    String getWebUIData(const String& contextId) override {
        if (!config) return "{}";
        
        if (contextId == "config_settings") {
            JsonDocument doc;
            doc["enabled"] = config->isEnabled() ? "true" : "false";
            doc["mode"] = config->getMode();
            doc["interval"] = String(config->getInterval());
            
            String json;
            serializeJson(doc, json);
            return json;
        }
        
        return "{}";
    }
    
    bool hasDataChanged(const String& contextId) override {
        if (!config) return false;
        
        if (contextId == "config_settings") {
            ConfigState current = {
                config->isEnabled(),
                config->getMode(),
                config->getInterval()
            };
            return configState.hasChanged(current);
        }
        
        return false;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                             const String& method, const std::map<String, String>& params) override {
        if (!config) {
            return "{\"success\":false,\"error\":\"Component not available\"}";
        }
        
        if (contextId == "config_settings" && method == "POST") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");
            
            if (fieldIt != params.end() && valueIt != params.end()) {
                const String& field = fieldIt->second;
                const String& value = valueIt->second;
                
                if (field == "enabled") {
                    config->setEnabled(value == "true" || value == "1");
                    return "{\"success\":true}";
                }
                else if (field == "mode") {
                    config->setMode(value);
                    return "{\"success\":true}";
                }
                else if (field == "interval") {
                    config->setInterval(value.toInt());
                    return "{\"success\":true}";
                }
            }
        }
        
        return "{\"success\":false,\"error\":\"Unknown request\"}";
    }
};
```

---

## API Reference

### IWebUIProvider Interface

```cpp
class IWebUIProvider {
public:
    // Required methods
    virtual std::vector<WebUIContext> getWebUIContexts() = 0;
    virtual String getWebUIData(const String& contextId) = 0;
    virtual String handleWebUIRequest(const String& contextId, const String& endpoint,
                                     const String& method, 
                                     const std::map<String, String>& params) = 0;
    virtual String getWebUIName() const = 0;
    virtual String getWebUIVersion() const = 0;
    
    // Optional optimization
    virtual bool hasDataChanged(const String& contextId) { return true; }
    
    // Helper methods
    virtual WebUIContext getWebUIContext(const String& contextId);
    virtual bool isWebUIEnabled() { return true; }
};
```

### LazyState<T> API

```cpp
template<typename T>
struct LazyState {
    // Main method - check if value changed, update internal state
    bool hasChanged(const T& current);
    
    // Get current stored value (undefined if not initialized)
    const T& getValue() const;
    
    // Check if state has been initialized
    bool isInitialized() const;
    
    // Initialize/get value with lazy loading
    T& get(std::function<T()> initializer);
    
    // Reset to uninitialized state
    void reset();
};
```

---

## Troubleshooting

### Issue: WebSocket updates not appearing

**Solution:**
1. Check `hasDataChanged()` returns `true` when data changes
2. Verify `getWebUIData()` returns valid JSON
3. Check WebSocket connection in browser console
4. Verify update interval is appropriate

### Issue: False-positive updates (always sends data)

**Solution:**
1. Implement `hasDataChanged()` with LazyState
2. Don't return `true` for unknown contexts
3. Ensure equality operators are correctly implemented for composite states

### Issue: State mismatch errors

**Solution:**
1. Use LazyState instead of manual initialization
2. Don't initialize state in constructor if provider created before component init
3. Check that state types match component return types

### Issue: Settings changes not persisting

**Solution:**
1. Implement `handleWebUIRequest()` properly
2. Save settings to Preferences/EEPROM in the handler
3. Return proper success/failure JSON response

---

## Related Documentation

- **IWebUIProvider.h**: Base interface definition
- **WifiWebUI.h**: Complete reference implementation
- **NTPWebUI.h**: Time-based data example
- **WebUIComponent**: Core WebUI server implementation

---

**Version:** DomoticsCore v2.0+  
**Last Updated:** 2025-10-05
