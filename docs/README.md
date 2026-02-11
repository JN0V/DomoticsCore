# DomoticsCore Documentation

## Overview

Welcome to the DomoticsCore documentation. This directory contains comprehensive guides for developing with the DomoticsCore modular component framework for ESP32, ESP32-C3, and ESP8266.

---

## Documentation Index

### **Getting Started**
- **[Getting Started Guide](getting-started.md)** - Complete tutorial from installation to first project
- **[Architecture Guide](architecture.md)** - Framework design, patterns, and best practices

### **Architecture**
- **[HAL Architecture](architecture/hal-architecture.md)** - Hardware Abstraction Layer routing pattern
- **[Component Lifecycle](architecture/component-lifecycle.md)** - Lifecycle states and methods
- **[EventBus Patterns](architecture/eventbus-patterns.md)** - Publish/subscribe and sticky events
- **[Component Configuration](architecture/component-configuration-pattern.md)** - Configuration patterns

### **Developer Guides**
- **[Custom Components](guides/custom-components.md)** - Creating your own components
- **[WebUI Development](guides/webui-developer.md)** - Adding web interfaces to components
- **[WebUI State Tracking](guides/webui-state-tracking.md)** - Efficient state management with LazyState

### **Reference Documentation**
- **[EventBus Architecture](reference/eventbus-architecture.md)** - Complete EventBus API and patterns

### **Reliability**
- **[Storage & Boot Diagnostics](reliability/storage-verbosity-and-boot-diagnostics.md)** - Persistent boot diagnostics and storage verbosity

---

## Component Documentation

Each component library contains its own documentation:

### **Core** (`DomoticsCore-Core/`)
- `README.md` - Framework, registry, EventBus, MemoryManager, HeapTracker
- `LOGGING.md` - Comprehensive logging guide
- `examples/` - CoreOnly, DummyComponent, EventBus basics and coordinators

### **System** (`DomoticsCore-System/`)
- `README.md` - System component integration guide
- `examples/Minimal/` - Minimal system setup
- `examples/Standard/` - Standard system setup
- `examples/FullStack/` - Full-featured system

### **WiFi** (`DomoticsCore-Wifi/`)
- `README.md` - WiFi component overview and API
- `examples/BasicWifi/` - Simple WiFi connection example
- `examples/WifiWithWebUI/` - WiFi with web interface

### **WebUI** (`DomoticsCore-WebUI/`)
- `README.md` - Web interface with WebSocket + SSE dual-mode
- `examples/HeadlessAPI/` - API-only mode without web frontend
- `examples/WebUIOnly/` - Standalone web interface

### **MQTT** (`DomoticsCore-MQTT/`)
- `README.md` - MQTT client API and usage
- `SPECIFICATIONS.md` - Detailed MQTT implementation specs
- `STATE_MACHINE.md` - MQTT state machine diagram
- `examples/BasicMQTT/`, `MQTTWithWebUI/`, `MQTTWifiWithWebUI/`

### **NTP** (`DomoticsCore-NTP/`)
- `README.md` - NTP time synchronization API
- `SPECIFICATIONS.md` - Detailed NTP implementation specs
- `examples/BasicNTP/`, `NTPWithWebUI/`

### **HomeAssistant** (`DomoticsCore-HomeAssistant/`)
- `README.md` - Home Assistant MQTT Discovery integration
- `SPECIFICATIONS.md` - HA integration specs
- `examples/BasicHA/`, `HAWithWebUI/`

### **OTA** (`DomoticsCore-OTA/`)
- `README.md` - Over-the-air firmware updates
- `examples/BasicOTA/`, `OTAWithWebUI/`

### **Storage** (`DomoticsCore-Storage/`)
- `README.md` - NVS / LittleFS persistent data
- `examples/BasicStorage/`, `NamespaceDemo/`, `StorageWithWebUI/`

### **LED** (`DomoticsCore-LED/`)
- `README.md` - Visual status indicators with 6 effects
- `examples/BasicLED/`, `LEDWithWebUI/`

### **RemoteConsole** (`DomoticsCore-RemoteConsole/`)
- `README.md` - Telnet debugging console with WebUI integration
- `examples/BasicRemoteConsole/`, `RemoteConsoleWithWebUI/`

### **SystemInfo** (`DomoticsCore-SystemInfo/`)
- `README.md` - System metrics and boot diagnostics
- `examples/BasicSystemInfo/`, `SystemInfoWithWebUI/`

---

## Getting Started

### **New to DomoticsCore?**

1. **Start with a component README** - Each component has its own README with quick start instructions
2. **Try the examples** - Run example applications to see components in action
3. **Read the WebUI Developer Guide** - Learn how to add web interfaces to your components

### **Creating a New Component?**

1. **Review existing components** - Look at `DomoticsCore-Wifi` or `DomoticsCore-NTP` for structure
2. **Follow the component pattern** - Implement `IComponent` interface
3. **Add WebUI (optional)** - Follow the **[WebUI Developer Guide](guides/webui-developer.md)**

### **Adding Web Interface to Existing Component?**

1. **Read [WebUI Developer Guide](guides/webui-developer.md)** - Complete step-by-step guide
2. **Check [WebUI State Tracking](guides/webui-state-tracking.md)** - For LazyState quick reference
3. **Study WifiWebUI.h or NTPWebUI.h** - Real-world reference implementations

---

## Architecture Overview

```
DomoticsCore Framework
├── Core                      # Component registry, lifecycle, MemoryManager
├── Components
│   ├── WiFi                 # Network connectivity
│   ├── WebUI                # Web interface (WebSocket + SSE)
│   ├── NTP                  # Time synchronization
│   ├── MQTT                 # MQTT client
│   ├── HomeAssistant        # HA integration
│   ├── OTA                  # Over-the-air updates
│   ├── LED                  # Status LED control
│   ├── Storage              # Persistent storage (NVS / LittleFS)
│   ├── RemoteConsole        # Telnet console with WebUI
│   ├── SystemInfo           # System information + boot diagnostics
│   └── System               # Complete system integration
│
├── HAL                      # Hardware Abstraction Layer
│   ├── Platform_HAL.h       # Platform detection and system functions
│   ├── Wifi_HAL.h           # Unified WiFi interface
│   ├── Storage_HAL.h        # Key-value storage abstraction
│   ├── NTP_HAL.h            # Time synchronization
│   └── SystemInfo_HAL.h     # System metrics
│
├── Testing                  # Test infrastructure
│   └── HeapTracker          # Memory leak detection (native + hardware)
│
└── Utils                    # Utilities (Timer, Logger, EventBus, etc.)
```

---

## Component Development Workflow

1. **Define your component class** - Implement `IComponent` interface
2. **Add configuration** - Define config struct for your component
3. **Implement lifecycle methods** - `begin()`, `loop()`, `shutdown()`
4. **Create WebUI provider (optional)** - Implement `IWebUIProvider` for web interface
5. **Write examples** - Demonstrate component usage
6. **Document** - Add README and inline documentation

---

## Key Concepts

### **Components**
Self-contained modules that provide specific functionality (WiFi, MQTT, etc.)

### **Component Registry**
Manages component lifecycle, dependency resolution, and initialization order

### **WebUI Providers**
Define how components appear in the web interface (dashboards, settings, status)

### **LazyState**
Helper for efficient state tracking in WebUI providers to optimize WebSocket updates

### **Contexts**
Different UI locations where component data can appear (Dashboard, Settings, Status badges)

### **MemoryManager**
Device-agnostic memory adaptation for different platforms (ESP32 vs ESP8266)

### **HeapTracker**
Memory leak detection tool for testing on native platform and hardware

---

## Common Patterns

### **Creating a Simple Component**

```cpp
class MyComponent : public IComponent {
public:
    ComponentStatus begin() override {
        // Initialize
        return ComponentStatus::Success;
    }

    void loop() override {
        // Main logic
    }

    ComponentStatus shutdown() override {
        // Cleanup
        return ComponentStatus::Success;
    }
};
```

### **Adding to Application**

```cpp
void setup() {
    Core core;

    // Add components
    core.addComponent(std::make_unique<MyComponent>());

    // Initialize all components
    core.begin();
}

void loop() {
    core.loop();
}
```

### **Adding WebUI**

```cpp
// 1. Create WebUI provider
class MyComponentWebUI : public IWebUIProvider {
    // Implement interface methods
};

// 2. Register after initialization
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* mycomp = core.getComponent<MyComponent>("MyComponent");
webui->registerProviderWithComponent(new MyComponentWebUI(mycomp), mycomp);
```

---

## Quick Links

- **Main Repository**: [GitHub](https://github.com/JN0V/DomoticsCore)
- **Issues**: [GitHub Issues](https://github.com/JN0V/DomoticsCore/issues)
- **PlatformIO Registry**: [jn0v/DomoticsCore](https://registry.platformio.org/libraries/jn0v/DomoticsCore)
- **Examples**: See `examples/` directory in each component library
- **Changelog**: [CHANGELOG.md](../CHANGELOG.md)

---

## Documentation Structure

```
docs/
├── README.md                          # This file - documentation index
├── getting-started.md                 # Complete tutorial
├── architecture.md                    # Framework design
├── architecture/
│   ├── hal-architecture.md            # HAL routing pattern
│   ├── component-lifecycle.md         # Component lifecycle states
│   ├── eventbus-patterns.md           # EventBus patterns
│   └── component-configuration-pattern.md  # Config patterns
├── guides/
│   ├── custom-components.md           # Component development
│   ├── webui-developer.md             # WebUI integration
│   └── webui-state-tracking.md        # State management
├── reference/
│   └── eventbus-architecture.md       # EventBus API reference
└── reliability/
    └── storage-verbosity-and-boot-diagnostics.md  # Boot diagnostics

Each component library:
└── DomoticsCore-{Component}/
    ├── README.md                      # Component overview and API
    ├── include/                       # Header files
    ├── src/                           # Implementation (if needed)
    └── examples/                      # Example applications
        └── {ExampleName}/
            ├── src/main.cpp           # Example code
            └── platformio.ini         # Build configuration
```

---

## Version Information

**DomoticsCore Version:** 1.6.0
**Documentation Last Updated:** 2026-02-11

---

## License

See main repository LICENSE file for licensing information.
