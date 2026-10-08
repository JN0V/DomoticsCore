<!-- workline
sources: [DomoticsCore-System/include/DomoticsCore/System.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/, DomoticsCore-LED/, DomoticsCore-WebUI/]
-->

# Key Features Deep Dive

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
