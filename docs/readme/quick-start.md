<!-- workline
sources: [DomoticsCore-System/include/DomoticsCore/System.h, DomoticsCore-System/include/DomoticsCore/SystemConfig.h, DomoticsCore-Core/include/DomoticsCore/Core.h]
-->

# Quick Start

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
