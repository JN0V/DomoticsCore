# Quickstart: ESP8266 Full Support

**Feature**: 002-esp8266-full-support  
**Date**: 2025-12-18  
**Purpose**: Getting started guide for ESP8266 validation

## Prerequisites

- **Hardware**: Wemos D1 Mini (or compatible ESP8266 with 4MB flash)
- **Software**: PlatformIO IDE or CLI
- **Network**: WiFi network for testing (Phases 4+)

## Environment Setup

### 1. PlatformIO Configuration

Add ESP8266 environment to `platformio.ini`:

```ini
[env:esp8266]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200

; Build options - match existing project settings
build_flags = 
    -std=gnu++14
    -DCORE_DEBUG_LEVEL=3

; Libraries - only WebUI needs additional deps for ESP8266
; PubSubClient and ArduinoJson already declared in library.json
lib_deps = 
    esphome/ESPAsyncWebServer-esphome@^3.0.0  ; Only for WebUI phase
    esphome/ESPAsyncTCP-esphome@^2.0.0        ; Only for WebUI phase
```

**Note**: `PubSubClient ^2.8` and `ArduinoJson ^7.0.0` are already dependencies in `DomoticsCore-MQTT/library.json` - no need to add them again.

### 2. Board Connection

1. Connect Wemos D1 Mini via USB
2. Note the serial port (e.g., `/dev/ttyUSB0` or `COM3`)
3. Open Serial Monitor at 115200 baud

## Phase-by-Phase Validation

### Phase 1: Core

```cpp
#include <DomoticsCore/Core.h>

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Phase 1: Core ===");
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    
    // Test EventBus
    DomoticsCore::EventBus::init();
    Serial.println("EventBus initialized");
    
    // Test Timer
    DomoticsCore::Timer::NonBlockingDelay timer(1000);
    Serial.println("Timer created");
    
    Serial.printf("Heap after init: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Keep loop fast
}
```

**Expected Output**:
```
=== Phase 1: Core ===
Free Heap: ~50000 bytes
EventBus initialized
Timer created
Heap after init: ~48000 bytes
```

---

### Phase 2: LED

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>

DomoticsCore::Components::LEDComponent led;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Phase 2: LED ===");
    Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
    
    led.begin();
    led.setPattern(DomoticsCore::Components::LEDPattern::Blink);
    
    Serial.printf("Heap after: %d\n", ESP.getFreeHeap());
}

void loop() {
    led.loop();
}
```

**Expected**: LED blinks, heap increase ≤1KB

---

### Phase 3: Storage

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

DomoticsCore::Components::StorageComponent storage;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Phase 3: Storage ===");
    Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
    
    storage.begin();
    
    // Write test
    storage.putString("test", "key1", "Hello ESP8266!");
    
    // Read test
    String value = storage.getString("test", "key1", "default");
    Serial.printf("Read back: %s\n", value.c_str());
    
    Serial.printf("Heap after: %d\n", ESP.getFreeHeap());
}

void loop() {}
```

**Expected**: Value persists across reboots, heap increase ≤2KB

---

### Phase 4: WiFi

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>

DomoticsCore::Components::WifiComponent wifi;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Phase 4: WiFi ===");
    Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
    
    DomoticsCore::Components::WifiConfig cfg;
    cfg.ssid = "YOUR_SSID";
    cfg.password = "YOUR_PASSWORD";
    wifi.setConfig(cfg);
    
    wifi.begin();
    Serial.printf("Heap after begin: %d\n", ESP.getFreeHeap());
}

void loop() {
    wifi.loop();
    
    static bool printed = false;
    if (wifi.isConnected() && !printed) {
        Serial.printf("Connected! IP: %s\n", wifi.getIP().toString().c_str());
        Serial.printf("Heap connected: %d\n", ESP.getFreeHeap());
        printed = true;
    }
}
```

**Expected**: Connects to WiFi, prints IP, heap increase ≤5KB

---

### Phase 11: WebUI

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/WebUI.h>

DomoticsCore::Components::WifiComponent wifi;
DomoticsCore::Components::StorageComponent storage;
DomoticsCore::Components::WebUIComponent webui;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Phase 11: WebUI ===");
    Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
    
    // Initialize dependencies
    storage.begin();
    
    DomoticsCore::Components::WifiConfig wifiCfg;
    wifiCfg.ssid = "YOUR_SSID";
    wifiCfg.password = "YOUR_PASSWORD";
    wifi.setConfig(wifiCfg);
    wifi.begin();
    
    // Wait for WiFi
    while (!wifi.isConnected()) {
        wifi.loop();
        delay(100);
    }
    
    // Initialize WebUI
    webui.begin();
    
    Serial.printf("Heap after WebUI: %d\n", ESP.getFreeHeap());
    Serial.printf("WebUI at: http://%s/\n", wifi.getIP().toString().c_str());
}

void loop() {
    wifi.loop();
    webui.loop();
}
```

**Expected**: Dashboard accessible at device IP, heap ≥20KB free

---

## Validation Checklist

After each phase, record:

1. **Compiles**: `pio run -e esp8266` succeeds
2. **Functions**: Expected behavior observed
3. **Heap Before**: `ESP.getFreeHeap()` before component init
4. **Heap After**: `ESP.getFreeHeap()` after component init
5. **Heap Delta**: Difference (must be ≤ budget)

## Troubleshooting

### Compilation Errors

| Error | Solution |
|-------|----------|
| `ESPAsyncWebServer.h not found` | Add `esphome/ESPAsyncWebServer-esphome` to lib_deps |
| `LittleFS.h not found` | Update ESP8266 Arduino Core to >= 3.0.0 |
| `WiFi.h ambiguous` | Use `ESP8266WiFi.h` explicitly in HAL |

### Runtime Issues

| Issue | Solution |
|-------|----------|
| Crash on boot | Check heap usage, reduce JSON size |
| WiFi won't connect | Verify credentials, check `WiFi.persistent(false)` |
| WebUI slow | Check heap fragmentation, reduce concurrent connections |
| OTA fails | Ensure enough flash space, check partition |

## Next Steps

After all 12 phases pass:

1. Run `/speckit.tasks` to generate implementation tasks
2. Create `examples/ESP8266/` directory with validated examples
3. Update component READMEs with ESP8266 notes
4. Run 24-hour stability test on physical device
