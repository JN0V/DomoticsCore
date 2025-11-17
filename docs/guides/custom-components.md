# Creating Custom Components

This guide shows how to create custom components for DomoticsCore, including best practices for ESP32 hardware interaction, dependency management, and storage patterns.

## Table of Contents
- [Basic Component Structure](#basic-component-structure)
- [Accessing Other Components](#accessing-other-components)
- [Understanding Dependencies](#understanding-dependencies)
- [ISR Best Practices (ESP32 Specific)](#isr-best-practices-esp32-specific)
- [Storage Patterns](#storage-patterns)
- [Event Bus Communication](#event-bus-communication)
- [Non-Blocking Timers](#non-blocking-timers)

---

## Basic Component Structure

Every custom component must inherit from `IComponent` and implement the required methods:

```cpp
#include <DomoticsCore/IComponent.h>

class MyComponent : public DomoticsCore::Components::IComponent {
public:
    ComponentStatus begin() override {
        // Initialize your hardware/state here
        pinMode(LED_PIN, OUTPUT);
        
        DLOG_I("MyComponent", "Initialized successfully");
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Your periodic logic here
        // This is called continuously in the main loop
    }
    
    ComponentStatus shutdown() override {
        // Cleanup resources here
        digitalWrite(LED_PIN, LOW);
        
        DLOG_I("MyComponent", "Shutdown complete");
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return "MyComponent";
    }
};
```

**Usage in main.cpp:**
```cpp
#include <DomoticsCore/System.h>
#include "MyComponent.h"

using namespace DomoticsCore;

System* domotics = nullptr;
MyComponent* myComp = nullptr;

void setup() {
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "MyDevice";
    
    domotics = new System(config);
    
    // Add custom component
    myComp = new MyComponent();
    domotics->getCore().addComponent(std::unique_ptr<MyComponent>(myComp));
    
    domotics->begin();
}

void loop() {
    domotics->loop();
}
```

---

## Accessing Other Components

Components can access other registered components through the automatically injected `Core` reference:

```cpp
class MyComponent : public IComponent {
private:
    // Component dependencies cached after initialization
    StorageComponent* storage_ = nullptr;
    NTPComponent* ntp_ = nullptr;
    
public:
    ComponentStatus begin() override {
        // Access Core automatically injected by framework
        Core* core = getCore();
        
        if (!core) {
            DLOG_E("MyComponent", "Core not available!");
            return ComponentStatus::DependencyError;
        }
        
        // Get other components
        storage_ = core->getComponent<StorageComponent>("Storage");
        ntp_ = core->getComponent<NTPComponent>("NTP");
        
        if (!storage_) {
            DLOG_W("MyComponent", "Storage component not available");
        }
        
        if (storage_) {
            // Use storage component
            int value = storage_->getInt("my_counter", 0);
            DLOG_I("MyComponent", "Loaded counter: %d", value);
        }
        
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (storage_) {
            // Use cached component reference
            storage_->putInt("my_counter", millis() / 1000);
        }
    }
    
    String getName() const override {
        return "MyComponent";
    }
    
    // Declare dependencies for automatic initialization ordering
    std::vector<String> getDependencies() const override {
        return {"Storage", "NTP"};
    }
};
```

**Key Points:**
- Use `getCore()` to access the automatically injected Core instance
- Cache component pointers in `begin()` for efficiency
- Check for `nullptr` - components may not be registered
- Use `getDependencies()` carefully (see next section)

---

## Understanding Dependencies

### Optional vs Required Dependencies

`getDependencies()` supports both required and optional dependencies:

```cpp
class MyComponent : public IComponent {
private:
    StorageComponent* storage_ = nullptr;
    MQTTComponent* mqtt_ = nullptr;
    
public:
    // Declare dependencies with optional flag
    std::vector<Dependency> getDependencies() const override {
        return {
            {"Storage", false},  // Optional - won't fail if missing
            {"MQTT", false},     // Optional
            {"MyCustomComp", true}  // Required - init fails if missing
        };
    }
    
    ComponentStatus begin() override {
        // Internal initialization only
        pinMode(PIN, INPUT);
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        // Get components - framework already logged if optional missing
        storage_ = getCore()->getComponent<StorageComponent>("Storage");
        mqtt_ = getCore()->getComponent<MQTTComponent>("MQTT");
        
        // Load from storage if available
        if (storage_) {
            uint64_t count = storage_->getULong64("pulse_count", 0);
            DLOG_I("MyComponent", "Loaded count: %llu", count);
        }
    }
    
    void loop() override {
        // Always check before using optional components
        if (storage_) {
            storage_->putULong64("pulse_count", currentCount);
        }
        
        if (mqtt_ && mqtt_->isConnected()) {
            mqtt_->publish("sensor/data", payload);
        }
    }
};
```

### Simple Dependencies (All Required)

For simple cases where all dependencies are required, use implicit conversion:

```cpp
class ComponentB : public IComponent {
    std::vector<Dependency> getDependencies() const override {
        return {"ComponentA"};  // Implicit required=true
    }
    
    void afterAllComponentsReady() override {
        auto* compA = getCore()->getComponent<ComponentA>("ComponentA");
        // compA is guaranteed to exist (framework fails if missing)
    }
};
```

### When to Use `getDependencies()`

**✅ Use for:**
- Custom → custom component dependencies (required or optional)
- Built-in components (Storage, MQTT, etc.) as optional dependencies
- Explicit declaration of what your component needs

**💡 Tips:**
- Use `required=false` for built-in components (they might not be configured)
- Use `required=true` for critical custom dependencies
- Framework logs INFO when optional dep missing, ERROR for required
- Access components in `afterAllComponentsReady()` for guaranteed availability

### Component Initialization Timeline

```
1. setup() {
   2. Create System
   3. Register custom components
   4. begin() {
      5. Register built-in components
      6. Resolve dependencies (optional deps OK if missing)
      7. initializeAll() in dependency order
         → begin() called on each component
         → afterAllComponentsReady() called on each component
      }
   }
}
```

---

## ISR Best Practices (ESP32 Specific)

ESP32 has a specific limitation: **C++ class static members cannot be placed in IRAM**, which is required for interrupt service routines (ISRs).

### ❌ DON'T: Static ISR in class (causes linker errors)
```cpp
class MyComponent : public IComponent {
private:
    static volatile uint64_t count_;
    
    static void IRAM_ATTR myISR() {
        count_++;  // Error: "dangerous relocation: l32r"
    }
};
```

### ✅ DO: Global ISR outside class
```cpp
namespace {
    // Use anonymous namespace for file-local globals
    volatile uint64_t g_pulseCount = 0;
    volatile bool g_newPulse = false;
}

// ISR must be a global function to be placed in IRAM
void IRAM_ATTR pulseISR() {
    g_pulseCount++;
    g_newPulse = true;
}

class MyComponent : public IComponent {
private:
    const int PULSE_PIN = 4;
    
public:
    ComponentStatus begin() override {
        pinMode(PULSE_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseISR, FALLING);
        
        DLOG_I("MyComponent", "ISR attached to pin %d", PULSE_PIN);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (g_newPulse) {
            DLOG_D("MyComponent", "Pulse detected! Count: %llu", g_pulseCount);
            g_newPulse = false;
            
            // Process the pulse event
            handlePulse();
        }
    }
    
    void handlePulse() {
        // Your pulse handling logic
        // Maybe emit an event or update storage
    }
    
    ComponentStatus shutdown() override {
        detachInterrupt(digitalPinToInterrupt(PULSE_PIN));
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return "MyComponent";
    }
};
```

**Why?** ESP32 toolchain limitation with C++ member functions in IRAM.  
**Solution:** ISR must be a global function, use anonymous namespace for variables.

---

## Storage Patterns

### Storing Simple Types

```cpp
void loop() override {
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (!storage) return;
    
    // Store different types
    storage->putInt("counter", 42);
    storage->putString("device_name", "MyDevice");
    storage->putFloat("temperature", 23.5);
    storage->putBool("enabled", true);
    
    // Retrieve with defaults
    int counter = storage->getInt("counter", 0);
    String name = storage->getString("device_name", "Unknown");
    float temp = storage->getFloat("temperature", 0.0);
    bool enabled = storage->getBool("enabled", false);
}
```

### Storing uint64_t (Native Support)

```cpp
ComponentStatus begin() override {
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (!storage) return ComponentStatus::DependencyError;
    
    // Load uint64_t value
    uint64_t pulseCount = storage->getULong64("pulse_count", 0);
    DLOG_I("MyComponent", "Loaded pulse count: %llu", pulseCount);
    
    return ComponentStatus::Success;
}

void savePulseCount(uint64_t count) {
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (storage) {
        storage->putULong64("pulse_count", count);
        DLOG_D("MyComponent", "Saved pulse count: %llu", count);
    }
}
```

### Storing Binary Data (Structs)

```cpp
struct DeviceConfig {
    int sensorPin;
    float calibration;
    bool enabled;
};

void saveConfig(const DeviceConfig& cfg) {
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (storage) {
        storage->putBlob("device_config", (const uint8_t*)&cfg, sizeof(DeviceConfig));
        DLOG_I("MyComponent", "Config saved");
    }
}

bool loadConfig(DeviceConfig& cfg) {
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (!storage) return false;
    
    uint8_t buffer[sizeof(DeviceConfig)];
    size_t read = storage->getBlob("device_config", buffer, sizeof(buffer));
    
    if (read == sizeof(DeviceConfig)) {
        memcpy(&cfg, buffer, sizeof(DeviceConfig));
        DLOG_I("MyComponent", "Config loaded");
        return true;
    }
    
    return false;
}
```

---

## Component Configuration and Persistence

### The `setConfig()` Pattern

**Critical Rule:** If your component has a configuration structure and you want to load it from Storage AFTER component creation, you **MUST** provide a `setConfig()` method.

**Why?** Components are created and initialized (`begin()` called) BEFORE the Storage component is available. To support loading configuration from Storage, components need a way to update their configuration after initialization.

### Configuration Structure

```cpp
// Define your component's configuration
struct MyComponentConfig {
    bool enabled = true;
    int updateInterval = 5000;
    String serverUrl = "";
    float threshold = 23.5;
};

class MyComponent : public IComponent {
private:
    MyComponentConfig config;
    
public:
    // Constructor takes initial config (may have defaults)
    explicit MyComponent(const MyComponentConfig& cfg = MyComponentConfig())
        : config(cfg) {
        metadata.name = "MyComponent";
        metadata.version = "1.0.0";
    }
    
    ComponentStatus begin() override {
        // Component initialized with constructor config
        DLOG_I("MyComponent", "Init with interval: %d ms", config.updateInterval);
        return ComponentStatus::Success;
    }
    
    /**
     * @brief Update configuration after component creation
     * @param cfg New configuration
     * 
     * Allows updating settings loaded from Storage component.
     * Call this after core.begin() when loading config from Storage.
     */
    void setConfig(const MyComponentConfig& cfg) {
        bool needsRestart = (cfg.updateInterval != config.updateInterval);
        
        config = cfg;
        
        // Apply changes that require restart
        if (needsRestart) {
            // Reinitialize with new settings
            restart();
        }
        
        DLOG_I("MyComponent", "Config updated: interval=%d ms", config.updateInterval);
    }
    
    /**
     * @brief Get current configuration (for saving to Storage)
     */
    const MyComponentConfig& getConfig() const {
        return config;
    }
    
    String getName() const override { return "MyComponent"; }
};
```

### Loading Config from Storage (in System or main.cpp)

```cpp
// After core.begin(), components and Storage are initialized
auto* storageComp = core.getComponent<StorageComponent>("Storage");
auto* myComp = core.getComponent<MyComponent>("MyComponent");

if (storageComp && myComp) {
    // Load configuration from Storage
    MyComponentConfig cfg;
    cfg.enabled = storageComp->getBool("my_enabled", true);
    cfg.updateInterval = storageComp->getInt("my_interval", 5000);
    cfg.serverUrl = storageComp->getString("my_server", "");
    cfg.threshold = storageComp->getFloat("my_threshold", 23.5);
    
    DLOG_I(LOG_SYSTEM, "Loading MyComponent config from Storage");
    
    // Apply loaded configuration
    myComp->setConfig(cfg);
}
```

### Saving Config to Storage (in WebUI or component)

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
}
```

### Components That Need `setConfig()`

✅ **Have `setConfig()`:**
- `NTPComponent` - timezone, servers, sync interval
- `WebUIComponent` - theme, device name
- `MQTTComponent` - broker, port, credentials
- `HomeAssistantComponent` - node ID, discovery prefix

❌ **Don't need `setConfig()`:**
- `StorageComponent` - initialized first, config at construction only
- `CoreComponent` - no persistent config
- Simple hardware components without configuration

### Best Practices

1. **Always provide `getConfig()`** - Allows reading current configuration
2. **Handle config changes gracefully** - Check what changed, only restart if needed
3. **Log configuration updates** - Use `DLOG_I()` for visibility
4. **Validate new config** - Check ranges, required fields before applying
5. **Support partial updates** - Start from current config, update only changed fields

### Complete Example with WebUI Integration

```cpp
struct SensorConfig {
    bool enabled = true;
    int readInterval = 10000;
    float calibrationOffset = 0.0;
    String mqttTopic = "sensor/data";
};

class SensorComponent : public IComponent {
private:
    SensorConfig config;
    Utils::NonBlockingDelay readTimer;
    
public:
    explicit SensorComponent(const SensorConfig& cfg = SensorConfig())
        : config(cfg), readTimer(cfg.readInterval) {
        metadata.name = "Sensor";
    }
    
    ComponentStatus begin() override {
        pinMode(SENSOR_PIN, INPUT);
        return ComponentStatus::Success;
    }
    
    void setConfig(const SensorConfig& cfg) {
        bool intervalChanged = (cfg.readInterval != config.readInterval);
        
        config = cfg;
        
        if (intervalChanged) {
            readTimer.setInterval(config.readInterval);
        }
        
        DLOG_I("Sensor", "Config updated: interval=%d, offset=%.2f", 
               config.readInterval, config.calibrationOffset);
    }
    
    const SensorConfig& getConfig() const { return config; }
    
    void loop() override {
        if (!config.enabled) return;
        
        if (readTimer.isReady()) {
            float raw = analogRead(SENSOR_PIN);
            float value = raw + config.calibrationOffset;
            
            emit("sensor/reading", value, true);
            
            auto* mqtt = getCore()->getComponent<MQTTComponent>("MQTT");
            if (mqtt && mqtt->isConnected()) {
                mqtt->publish(config.mqttTopic, String(value));
            }
        }
    }
    
    String getName() const override { return "Sensor"; }
};
```

---

## Event Bus Communication

The Event Bus allows components to communicate without direct coupling.

### Publishing Events

```cpp
struct SensorData {
    float temperature;
    int humidity;
    uint32_t timestamp;
};

void loop() override {
    // Read sensor
    SensorData data;
    data.temperature = readTemperature();
    data.humidity = readHumidity();
    data.timestamp = millis();
    
    // Publish to other components
    emit("sensor.data", data, false);  // Non-sticky
    
    // Or publish sticky (last value cached for late subscribers)
    emit("sensor.temperature", data.temperature, true);
}
```

### Subscribing to Events

```cpp
ComponentStatus begin() override {
    // Subscribe to sensor data
    on<SensorData>("sensor.data", [](const SensorData& data) {
        DLOG_I("MyComponent", "Temp: %.1f°C, Humidity: %d%%", 
               data.temperature, data.humidity);
    });
    
    // Subscribe to temperature only
    on<float>("sensor.temperature", [](const float& temp) {
        DLOG_D("MyComponent", "Temperature updated: %.1f°C", temp);
    }, true);  // Replay last value immediately
    
    return ComponentStatus::Success;
}
```

### System Events

DomoticsCore components emit standard events that you can subscribe to:

**WiFi Component:**
- `wifi/sta/connected` (bool) - Station connected/disconnected
- `wifi/ap/enabled` (bool) - Access Point enabled/disabled

**MQTT Component:**
- `mqtt/connected` (bool) - MQTT broker connected
- `mqtt/disconnected` (bool) - MQTT broker disconnected

**NTP Component:**
- `ntp/synced` (bool) - Time synchronized successfully
- `ntp/sync_failed` (bool) - Time synchronization failed

**HomeAssistant Component:**
- `ha/discovery_published` (int) - Discovery published (entity count)
- `ha/entity_added` (String) - Entity added (entity ID)

### Subscribing to System Events

```cpp
void setup() {
    System system(config);
    system.begin();
    
    // React to MQTT connection
    system.getCore().getEventBus().subscribe("mqtt/connected", 
        [](const void*) {
            DLOG_I(LOG_APP, "MQTT is now available!");
            // Start publishing data, enable features, etc.
        }
    );
    
    // Monitor NTP sync
    system.getCore().getEventBus().subscribe("ntp/synced", 
        [](const void*) {
            DLOG_I(LOG_APP, "Time synchronized!");
        }
    );
    
    // Monitor Home Assistant entity additions
    system.getCore().getEventBus().subscribe("ha/entity_added", 
        [](const void* payload) {
            String id = payload ? *static_cast<const String*>(payload) : "";
            DLOG_I(LOG_APP, "New HA entity: %s", id.c_str());
        }
    );
}
```

### System.h Orchestration

The `System` class uses the Event Bus for intelligent orchestration of components:

```cpp
// WiFi → MQTT: Trigger connection when network available
core.getEventBus().subscribe("wifi/sta/connected", [mqttComp](const void* payload) {
    bool connected = payload ? *static_cast<const bool*>(payload) : false;
    if (connected) {
        mqttComp->connect();
    }
});

// MQTT → HomeAssistant: Trigger discovery when MQTT connects
core.getEventBus().subscribe("mqtt/connected", [haComp](const void*) {
    if (haComp->getStatistics().entityCount > 0) {
        haComp->publishDiscovery();
    }
});
```

This ensures proper timing: WiFi must be up before MQTT connects, and MQTT must be connected before Home Assistant discovery is published.

---

## Non-Blocking Timers

Use `NonBlockingDelay` for periodic tasks without blocking the main loop:

```cpp
#include <DomoticsCore/Timer.h>

class MyComponent : public IComponent {
private:
    Utils::NonBlockingDelay statusTimer{5000};    // 5 seconds
    Utils::NonBlockingDelay saveTimer{60000};     // 1 minute
    Utils::NonBlockingDelay publishTimer{10000};  // 10 seconds
    
public:
    void loop() override {
        // Check status every 5 seconds
        if (statusTimer.isReady()) {
            printStatus();
            // Timer auto-resets after isReady() returns true
        }
        
        // Save to storage every minute
        if (saveTimer.isReady()) {
            saveToStorage();
        }
        
        // Publish to MQTT every 10 seconds
        if (publishTimer.isReady()) {
            publishData();
        }
    }
    
    void printStatus() {
        DLOG_I("MyComponent", "Uptime: %lu seconds", millis() / 1000);
    }
    
    void saveToStorage() {
        auto* storage = getCore()->getComponent<StorageComponent>("Storage");
        if (storage) {
            storage->putInt("last_save", millis());
        }
    }
    
    void publishData() {
        // Your publish logic
    }
    
    String getName() const override {
        return "MyComponent";
    }
};
```

**Key Points:**
- No `delay()` calls - keeps the main loop responsive
- Timers automatically reset after `isReady()` returns true
- Multiple timers can run independently
- Very low overhead (just one `millis()` check per timer)

---

## Complete Example: Water Meter Component

Here's a real-world example combining all the patterns:

```cpp
#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Timer.h>

namespace {
    // ISR globals (ESP32 requirement)
    volatile uint64_t g_pulseCount = 0;
    volatile bool g_newPulse = false;
}

void IRAM_ATTR pulseISR() {
    g_pulseCount++;
    g_newPulse = true;
}

class WaterMeterComponent : public DomoticsCore::Components::IComponent {
private:
    const int PULSE_PIN = 4;
    const float LITERS_PER_PULSE = 1.0;
    
    Utils::NonBlockingDelay saveTimer{60000};     // Save every minute
    Utils::NonBlockingDelay publishTimer{10000};   // Publish every 10 seconds
    
    uint64_t lastSavedCount_ = 0;
    
public:
    ComponentStatus begin() override {
        // Load saved count from storage
        auto* storage = getCore()->getComponent<StorageComponent>("Storage");
        if (storage) {
            g_pulseCount = storage->getULong64("pulse_count", 0);
            lastSavedCount_ = g_pulseCount;
            DLOG_I("WaterMeter", "Loaded count: %llu pulses", g_pulseCount);
        }
        
        // Setup hardware
        pinMode(PULSE_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseISR, FALLING);
        
        DLOG_I("WaterMeter", "Initialized on pin %d", PULSE_PIN);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Handle new pulses
        if (g_newPulse) {
            g_newPulse = false;
            float liters = g_pulseCount * LITERS_PER_PULSE;
            DLOG_D("WaterMeter", "Pulse! Total: %.1f L", liters);
        }
        
        // Auto-save periodically
        if (saveTimer.isReady() && g_pulseCount != lastSavedCount_) {
            saveCount();
        }
        
        // Publish to MQTT
        if (publishTimer.isReady()) {
            publishUsage();
        }
    }
    
    void saveCount() {
        auto* storage = getCore()->getComponent<StorageComponent>("Storage");
        if (storage) {
            storage->putULong64("pulse_count", g_pulseCount);
            lastSavedCount_ = g_pulseCount;
            DLOG_I("WaterMeter", "Saved count: %llu", g_pulseCount);
        }
    }
    
    void publishUsage() {
        float liters = g_pulseCount * LITERS_PER_PULSE;
        
        struct UsageData {
            uint64_t pulses;
            float liters;
        };
        
        UsageData data = {g_pulseCount, liters};
        emit("watermeter.usage", data, true);  // Sticky for late subscribers
    }
    
    ComponentStatus shutdown() override {
        detachInterrupt(digitalPinToInterrupt(PULSE_PIN));
        saveCount();  // Save before shutdown
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return "WaterMeter";
    }
    
    std::vector<String> getDependencies() const override {
        return {"Storage"};
    }
};
```

---

## Summary

**Component Lifecycle:**
1. Constructor - Set metadata
2. `begin()` - Initialize hardware, load storage, get dependencies
3. `loop()` - Process events, handle timers, update state
4. `shutdown()` - Cleanup, save state

**Best Practices:**
- ✅ Use `getCore()` to access other components (automatically injected)
- ✅ ISRs must be global functions (ESP32 limitation)
- ✅ Use `NonBlockingDelay` instead of `delay()`
- ✅ Cache component references in `begin()`
- ✅ Check for `nullptr` when accessing components
- ✅ Use Event Bus for decoupled communication
- ✅ Declare dependencies with `getDependencies()`
- ✅ Use native uint64_t storage methods when available
- ❌ Don't use `delay()` in `loop()`
- ❌ Don't make ISRs class static members

**For More Examples:**
- See `examples/` directory for complete working projects
- Check component headers in `DomoticsCore-*/include/` for API reference
- WaterMeter project demonstrates all patterns in production use
