# Component Configuration Pattern

## Standard Pattern for All Components

Every component with persistent configuration MUST follow this pattern for consistency and maintainability.

---

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

---

## 3. Storage Integration (in System.h)

### Pattern: Get → Override → Set

```cpp
// ====================================================================
// 6.X. Load MyComponent configuration from Storage
// ====================================================================
#if __has_include(<DomoticsCore/MyComponent.h>) && __has_include(<DomoticsCore/Storage.h>)
if (config.enableMyComponent && config.enableStorage) {
    auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
    auto* myComp = core.getComponent<Components::MyComponent>("MyComponent");
    if (storageComp && myComp) {
        // 1. GET current config (preserves defaults)
        Components::MyComponentConfig cfg = myComp->getConfig();
        
        // 2. OVERRIDE only fields from Storage (keeps defaults if key missing)
        cfg.enabled = storageComp->getBool("my_enabled", cfg.enabled);
        cfg.updateInterval = storageComp->getInt("my_interval", cfg.updateInterval);
        cfg.serverUrl = storageComp->getString("my_server", cfg.serverUrl);
        cfg.threshold = storageComp->getFloat("my_threshold", cfg.threshold);
        
        DLOG_I(LOG_SYSTEM, "Loading MyComponent config from Storage: interval=%d", 
               cfg.updateInterval);
        
        // 3. SET the merged config
        myComp->setConfig(cfg);
    }
}
#endif
```

**Critical Steps:**
1. **GET** current config → Preserves constructor defaults and non-persisted fields
2. **OVERRIDE** from Storage → Uses Storage value OR falls back to current value
3. **SET** merged config → Applies changes intelligently

**Why This Pattern?**
- ✅ Preserves non-persisted fields (e.g., `port`, `maxClients`)
- ✅ Graceful degradation if Storage empty/missing
- ✅ Constructor defaults always available
- ✅ No data loss on partial updates

---

## 4. WebUI Integration

Save config when user modifies settings:

```cpp
void onConfigChanged(const MyComponentConfig& cfg) {
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (storage) {
        storage->putBool("my_enabled", cfg.enabled);
        storage->putInt("my_interval", cfg.updateInterval);
        storage->putString("my_server", cfg.serverUrl);
        storage->putFloat("my_threshold", cfg.threshold);
        
        DLOG_I("MyComponent", "Config saved to Storage");
    }
    
    // Apply immediately
    setConfig(cfg);
}
```

---

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
    
    String getName() const override { return "MyComponent"; }
};
```

---

## 6. Current Component Status

### ✅ Components with Uniform Pattern

| Component | Config Struct | getConfig() | setConfig() | Storage Integration |
|-----------|---------------|-------------|-------------|---------------------|
| **WiFi** | `WifiConfig` | ✅ | ✅ | ✅ Section 6.1 |
| **WebUI** | `WebUIConfig` | ✅ | ✅ | ✅ Section 6.2 |
| **NTP** | `NTPConfig` | ✅ | ✅ | ✅ Section 6.3 |
| **MQTT** | `MQTTConfig` | ✅ | ✅ | ✅ Section 6.4 |
| **HomeAssistant** | `HAConfig` | ✅ | ✅ | ✅ Section 6.5 |
| **SystemInfo** | `SystemInfoConfig` | ✅ | ⚠️ | ⚠️ Read-only (populated from SystemConfig) |

### ⚠️ Components Needing Full Pattern

| Component | Status | Note |
|-----------|--------|------|
| **OTA** | Has config struct | Needs `getConfig()` + `setConfig()` + Storage |
| **RemoteConsole** | Simple config | Currently static (port 23) |
| **LED** | Has config struct | Needs Storage integration |

---

## 7. Benefits of This Pattern

1. **Consistency**: All components work the same way
2. **Safety**: Defaults always preserved
3. **Flexibility**: Partial config updates supported
4. **Testability**: Easy to mock and test
5. **User-Friendly**: Graceful degradation if Storage empty
6. **Performance**: Only restarts when necessary

---

## 8. Anti-Patterns to Avoid

### ❌ Creating Empty Config

```cpp
// BAD: Loses all defaults
MyComponentConfig cfg;
cfg.field1 = storage->getString("field1", "");
myComp->setConfig(cfg);  // All other fields now have wrong values!
```

### ✅ Get → Override → Set

```cpp
// GOOD: Preserves defaults
MyComponentConfig cfg = myComp->getConfig();
cfg.field1 = storage->getString("field1", cfg.field1);
myComp->setConfig(cfg);  // All fields correct
```

---

## 9. Migration Checklist

For each component needing update:

- [ ] Add `getXxxConfig()` method
- [ ] Verify `setConfig()` is smart (detects changes, minimal restart)
- [ ] Add Storage integration section in `System.h`
- [ ] Test with empty Storage (should use defaults)
- [ ] Test with partial Storage (should merge correctly)
- [ ] Test with full Storage (should override everything)
- [ ] Update component documentation

---

## 10. References

- **Implementation Example**: `DomoticsCore-NTP/include/DomoticsCore/NTP.h`
- **Storage Integration**: `DomoticsCore-System/include/DomoticsCore/System.h` (sections 6.2-6.5)
- **Custom Components Guide**: `docs/guides/custom-components.md`
