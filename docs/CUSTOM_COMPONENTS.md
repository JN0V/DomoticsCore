# Creating Custom Components

This guide shows how to create custom components for DomoticsCore, including best practices for ESP32 hardware interaction, dependency management, and storage patterns.

## ⚠️ CRITICAL: Component Registration Order

**Components MUST be registered BEFORE calling `begin()`, or your application will crash!**

```cpp
// ❌ WRONG - WILL CRASH
domotics = new System(config);
domotics->begin();                    // Core injection happens here
myComp = new MyComponent();
domotics->getCore().addComponent(...); // TOO LATE! Crashes with nullptr

// ✅ CORRECT - Works properly
domotics = new System(config);
myComp = new MyComponent();
domotics->getCore().addComponent(...); // BEFORE begin()
domotics->begin();                     // Now Core is injected correctly
```

**Why this matters:**
- The framework injects `Core` reference during `begin()` → `initializeAll()`
- Components added after `begin()` never receive the Core pointer
- Calling `getCore()` returns `nullptr` → **Guru Meditation Error (LoadProhibited)**

**Always follow this order:**
1. Create System instance
2. Register ALL custom components
3. Call `begin()`

---

## Table of Contents
- [Critical Registration Order](#️-critical-component-registration-order)
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

### ⚠️ Important: `getDependencies()` Limitations

**`getDependencies()` only works for custom → custom component dependencies!**

```cpp
// ✅ WORKS: Custom component depending on another custom component
class ComponentB : public IComponent {
    std::vector<String> getDependencies() const override {
        return {"ComponentA"};  // ComponentA is custom, registered before begin()
    }
    
    ComponentStatus begin() override {
        auto* compA = getCore()->getComponent<ComponentA>("ComponentA");
        // compA is guaranteed to exist and be initialized
    }
};

// ❌ FAILS: Custom component depending on built-in component
class MyComponent : public IComponent {
    std::vector<String> getDependencies() const override {
        return {"Storage", "MQTT"};  // ERROR: Not yet registered!
    }
};
```

**Why this limitation exists:**
- Custom components are registered in `setup()` **before** `begin()`
- Built-in components (Storage, WiFi, MQTT, NTP, etc.) are registered **during** `begin()`
- Dependency resolution happens at registration time
- Built-ins don't exist yet when custom components are registered

### Correct Pattern for Built-in Dependencies

Always use defensive checks when accessing built-in components:

```cpp
class MyComponent : public IComponent {
private:
    StorageComponent* storage_ = nullptr;
    MQTTComponent* mqtt_ = nullptr;
    
public:
    std::vector<String> getDependencies() const override {
        // Don't declare built-in dependencies here
        return {};
    }
    
    ComponentStatus begin() override {
        // Get built-in components with null checks
        storage_ = getCore()->getComponent<StorageComponent>("Storage");
        mqtt_ = getCore()->getComponent<MQTTComponent>("MQTT");
        
        if (!storage_) {
            DLOG_W("MyComponent", "Storage unavailable, using defaults");
            // Continue with fallback behavior
        }
        
        if (!mqtt_) {
            DLOG_W("MyComponent", "MQTT unavailable, data will not be published");
        }
        
        // Load from storage if available
        if (storage_) {
            uint64_t count = storage_->getULong64("pulse_count", 0);
            DLOG_I("MyComponent", "Loaded count: %llu", count);
        }
        
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Always check before using
        if (storage_) {
            storage_->putULong64("pulse_count", currentCount);
        }
        
        if (mqtt_ && mqtt_->isConnected()) {
            mqtt_->publish("sensor/data", payload);
        }
    }
};
```

### When to Use `getDependencies()`

**✅ Use it when:**
- Custom component depends on another custom component
- You want guaranteed initialization order between your components
- You need to fail fast if dependency is missing

**❌ Don't use it for:**
- Built-in framework components (Storage, WiFi, MQTT, NTP, OTA, etc.)
- Optional dependencies (use null checks instead)
- Components that might not be configured

### Component Initialization Timeline

```
1. setup() {
   2. Create System
   3. Register custom components ← getDependencies() checked here
   4. begin() {
      5. Register built-in components
      6. initializeAll() in dependency order
         7. Core injection happens
         8. begin() called on each component
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

### Publishing to MQTT via Event Bus

```cpp
void publishWaterUsage(uint64_t liters) {
    struct MQTTMessage {
        String topic;
        String payload;
        bool retained;
    };
    
    MQTTMessage msg;
    msg.topic = "watermeter/usage";
    msg.payload = String(liters);
    msg.retained = true;
    
    // MQTT component will pick this up and publish
    emit("mqtt.publish", msg, false);
}
```

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
