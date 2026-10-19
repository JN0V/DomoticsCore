<!-- workline
sources: [DomoticsCore-NTP/include/DomoticsCore/NTP.h]
judged: 500862d
verified: agent:documentalist
-->
# Component Configuration Pattern -- Configuration Structure and Component Implementation

Part of the [Component Configuration Pattern](component-configuration-pattern.md).

## 1. Configuration Structure

Define a dedicated configuration structure:

```cpp
struct MyComponentConfig {
    bool enabled = true;
    int updateInterval = 5000;
    String serverUrl = "default.server.com";
    float threshold = 23.5;
};
```

**Requirements:**
- ✅ All fields have sensible defaults
- ✅ Struct is in component's namespace
- ✅ Named `XxxConfig` where Xxx is component name

---

## 2. Component Implementation

Every component with configuration MUST provide:

### 2.1. Private Member

```cpp
class MyComponent : public IComponent {
private:
    MyComponentConfig config;
    // ...
};
```

### 2.2. Constructor with Config

```cpp
public:
    explicit MyComponent(const MyComponentConfig& cfg = MyComponentConfig())
        : config(cfg) {
        metadata.name = "MyComponent";
        metadata.version = "1.0.0";
    }
```

### 2.3. Getter Method

```cpp
/**
 * @brief Get current component configuration
 * @return Current MyComponentConfig
 */
MyComponentConfig getConfig() const {
    return config;
}
```

**Uniform Naming Convention:**
- All components use `getConfig()` for consistency
- Returns config struct by value (safe copy)
- No `getMyComponentConfig()` - always just `getConfig()`

### 2.4. Setter Method (Smart Override)

```cpp
/**
 * @brief Update configuration after component creation
 * @param cfg New configuration
 * 
 * Allows updating settings loaded from Storage component.
 * Call this after core.begin() when loading config from Storage.
 */
void setConfig(const MyComponentConfig& cfg) {
    // 1. Detect what changed
    bool needsRestart = (cfg.updateInterval != config.updateInterval);
    bool urlChanged = (cfg.serverUrl != config.serverUrl);
    
    // 2. Apply new config
    config = cfg;
    
    // 3. Take action based on changes
    if (urlChanged) {
        reconnect();  // Immediate action
    }
    
    if (needsRestart && config.enabled) {
        restart();  // Full restart if needed
    }
    
    DLOG_I("MyComponent", "Config updated: interval=%d, url=%s", 
           config.updateInterval, config.serverUrl.c_str());
}
```

**setConfig() Requirements:**
- ✅ Compare old vs new config
- ✅ Only restart/reinit if necessary
- ✅ Log configuration changes
- ✅ Handle partial updates gracefully


## 5. Complete Example

```cpp
// MyComponent.h
struct MyComponentConfig {
    bool enabled = true;
    int pin = 4;
    float sensitivity = 1.0;
};

class MyComponent : public IComponent {
private:
    MyComponentConfig config;
    
public:
    explicit MyComponent(const MyComponentConfig& cfg = MyComponentConfig())
        : config(cfg) {
        metadata.name = "MyComponent";
    }
    
    ComponentStatus begin() override {
        pinMode(config.pin, INPUT);
        return ComponentStatus::Success;
    }
    
    MyComponentConfig getConfig() const {
        return config;
    }
    
    void setConfig(const MyComponentConfig& cfg) {
        bool pinChanged = (cfg.pin != config.pin);
        
        config = cfg;
        
        if (pinChanged) {
            pinMode(config.pin, INPUT);
            DLOG_I("MyComponent", "Pin changed to %d", config.pin);
        }
        
        DLOG_I("MyComponent", "Config updated: sensitivity=%.2f", config.sensitivity);
    }
    
};
```

