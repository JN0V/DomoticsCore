<!-- workline
sources: [DomoticsCore-System/include/DomoticsCore/System.h, DomoticsCore-System/include/DomoticsCore/SystemConfig.h, DomoticsCore-Core/include/DomoticsCore/Core.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h]
-->

# Quick Start and Key Features

### Option 1: Full System (Recommended for beginners)

Everything automatic: WiFi, LED status, remote console, error recovery!

```cpp
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    
    // Full-stack configuration
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "MyDevice";
    config.wifiSSID = "YOUR_WIFI";
    config.wifiPassword = "YOUR_PASSWORD";
    config.ledPin = 2;  // Visual status on GPIO 2
    
    domotics = new System(config);
    
    // Add custom console commands
    domotics->registerCommand("hello", [](const String& args) {
        return String("Hello from DomoticsCore!\n");
    });
    
    // Initialize - automatic WiFi, LED, Console, error handling
    if (!domotics->begin()) {
        DLOG_E(LOG_APP, "System initialization failed!");
        while (1) {
            domotics->loop();  // Keep LED error animation running
            yield();
        }
    }
    
    DLOG_I(LOG_APP, "System ready!");
}

void loop() {
    domotics->loop();  // Handles everything automatically
    
    // Your application code here
}
```

**That's it!** LED patterns show system state, telnet console on port 23, error recovery built-in.

**LED States:**
- 🔵 Fast blink (200ms): Booting
- 🟡 Slow blink (1000ms): WiFi connecting  
- 🟢 Pulse (2000ms): WiFi connected
- 🟢 Fade (1500ms): Services starting
- 🟢 Breathing (3000ms): System ready
- 🔴 **Fast blink (300ms): ERROR** (LED works even in error state!)

### Option 2: Minimal Core (Advanced users)

Use only what you need - build your own orchestration:

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>
#include <DomoticsCore/Wifi.h>

using namespace DomoticsCore;

Core core;

void setup() {
    // Add only the components you need
    core.addComponent(std::make_unique<Components::LEDComponent>());
    core.addComponent(std::make_unique<Components::WifiComponent>("SSID", "password"));
    
    // Initialize - automatic dependency resolution
    CoreConfig config;
    config.deviceName = "MinimalDevice";
    core.begin(config);
}

void loop() {
    core.loop();
}
```

Binary size: **~300KB** (vs 1MB+ for full system)

## 🔧 Key Features Deep Dive

### Error Recovery

System continues running even when components fail:

```cpp
if (!domotics->begin()) {
    DLOG_E(LOG_APP, "Init failed!");
    while (1) {
        domotics->loop();  // LED shows ERROR, console still accessible
        yield();
    }
}
```

LED fast-blinks (300ms) to indicate error state. Telnet console remains available for debugging.

### Automatic Dependency Resolution

Components declare dependencies, framework initializes in correct order:

```cpp
class MyComponent : public IComponent {
    std::vector<Dependency> getDependencies() const override {
        return {{"Storage", false}, {"Wifi", false}};  // Will init after these
    }
};
```

### Visual Status Indicators

LED shows system state without serial console:

- **BOOTING** → Fast blink (200ms)
- **WIFI_CONNECTING** → Slow blink (1000ms)
- **WIFI_CONNECTED** → Pulse (2000ms)
- **READY** → Breathing (3000ms)
- **ERROR** → Fast blink (300ms)
- **OTA_UPDATE** → Solid on

### Event Bus

Decouple components with topic-based messaging:

```cpp
// Publisher (from within a component)
struct TempData { float celsius; };
emit("sensor/temperature", TempData{22.5}, true);  // sticky

// Subscriber (typed API, from within a component)
on<TempData>("sensor/temperature", [](const TempData& temp) {
    Serial.printf("Temp: %.1f°C\n", temp.celsius);
}, true);  // replayLast = true
```

### Chunked HTTP Responses

WebUI handles large responses (>40KB) automatically:

```cpp
// Automatically uses chunked transfer encoding for large schemas
webUI->serveSchema();  // Works even with 50KB+ JSON
```
