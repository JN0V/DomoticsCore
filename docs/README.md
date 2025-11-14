# DomoticsCore Documentation

## Overview

Welcome to the DomoticsCore documentation. This directory contains comprehensive guides for developing with the DomoticsCore modular component framework for ESP32.

---

## 📚 Documentation Index

### **Getting Started**
- **[Getting Started Guide](getting-started.md)** - Complete tutorial from installation to first project
- **[Architecture Guide](architecture.md)** - Framework design, patterns, and best practices

### **Migration Guides**
- **[Migration to v1.2.0](migration/v1.2.0.md)** - Upgrading from v1.1.x (EventBus changes)

### **Developer Guides**
- **[Custom Components](guides/custom-components.md)** - Creating your own components
- **[WebUI Development](guides/webui-developer.md)** - Adding web interfaces to components
- **[WebUI State Tracking](guides/webui-state-tracking.md)** - Efficient state management with LazyState

### **Reference Documentation**
- **[EventBus Architecture](reference/eventbus-architecture.md)** - Complete EventBus API and patterns

---

## Component Documentation

Each component library contains its own documentation:

### **WiFi Component** (`DomoticsCore-Wifi/`)
- `README.md` - WiFi component overview and API
- `examples/BasicWifi/` - Simple WiFi connection example
- `examples/WifiWithWebUI/` - WiFi with web interface example

### **NTP Component** (`DomoticsCore-NTP/`)
- `README.md` - NTP time synchronization API
- `SPECIFICATIONS.md` - Detailed NTP implementation specs
- `examples/NTPWithWebUI/` - NTP with web interface example

### **MQTT Component** (`DomoticsCore-MQTT/`)
- `README.md` - MQTT client API and usage
- `examples/MQTTWithWebUI/` - MQTT with web interface example

### **System Component** (`DomoticsCore-System/`)
- `README.md` - System component integration guide
- `examples/Standard/` - Standard system setup example

---

## Getting Started

### **New to DomoticsCore?**

1. **Start with a component README** - Each component has its own README with quick start instructions
2. **Try the examples** - Run example applications to see components in action
3. **Read the WebUI Developer Guide** - Learn how to add web interfaces to your components

### **Creating a New Component?**

1. **Review existing components** - Look at `DomoticsCore-Wifi` or `DomoticsCore-NTP` for structure
2. **Follow the component pattern** - Implement `IComponent` interface
3. **Add WebUI (optional)** - Follow the **[WebUI-Developer-Guide.md](WebUI-Developer-Guide.md)**

### **Adding Web Interface to Existing Component?**

1. **Read [WebUI-Developer-Guide.md](WebUI-Developer-Guide.md)** - Complete step-by-step guide
2. **Check [WebUI-State-Tracking.md](WebUI-State-Tracking.md)** - For LazyState quick reference
3. **Study WifiWebUI.h or NTPWebUI.h** - Real-world reference implementations

---

## Architecture Overview

```
DomoticsCore Framework
├── Core                      # Component registry and lifecycle
├── Components
│   ├── WiFi                 # Network connectivity
│   ├── WebUI                # Web interface framework
│   ├── NTP                  # Time synchronization
│   ├── MQTT                 # MQTT client
│   ├── HomeAssistant        # HA integration
│   ├── OTA                  # Over-the-air updates
│   ├── LED                  # Status LED control
│   ├── Storage              # Persistent storage
│   ├── SystemInfo           # System information
│   └── System               # Complete system integration
│
└── Utils                    # Utilities (Timer, Logger, etc.)
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

## API Documentation

### **Doxygen Documentation**

Generate full API documentation using Doxygen:

```bash
cd docs
doxygen Doxyfile
```

This generates HTML documentation in `docs/html/` with:
- Full API reference for all classes
- Component interfaces
- Utility functions
- Detailed method documentation

---

## Contributing

### **Documentation Guidelines**

- Use Markdown for all documentation
- Include code examples
- Keep examples minimal and focused
- Update documentation when changing APIs
- Cross-reference related documentation

### **Code Documentation**

- Use Doxygen-style comments for public APIs
- Document parameters and return values
- Include usage examples in header comments
- Explain non-obvious behavior

---

## Support and Resources

### **Example Applications**

Each component library includes example applications demonstrating:
- Basic usage
- WebUI integration
- Configuration options
- Common patterns

### **Reference Implementations**

- **WifiWebUI.h** - Complete WebUI provider with all features
- **NTPWebUI.h** - Time-based real-time updates
- **SystemComponent** - Full system integration

### **Common Issues**

See individual component READMEs for component-specific troubleshooting.

For WebUI issues, see:
- [WebUI-Developer-Guide.md](WebUI-Developer-Guide.md) - Troubleshooting section
- [WebUI-State-Tracking.md](WebUI-State-Tracking.md) - State tracking patterns

---

## Quick Links

- **Main Repository**: [GitHub URL] _(add your repo URL)_
- **Issues**: [GitHub Issues] _(add your issues URL)_
- **Examples**: See `examples/` directory in each component library
- **API Reference**: Generate with `doxygen Doxyfile`

---

## Documentation Structure

```
docs/
├── README.md                          # This file - documentation index
├── getting-started.md                 # Complete tutorial
├── architecture.md                    # Framework design
├── migration/
│   └── v1.2.0.md                     # Migration guides
├── guides/
│   ├── custom-components.md          # Component development
│   ├── webui-developer.md            # WebUI integration
│   └── webui-state-tracking.md       # State management
├── reference/
│   └── eventbus-architecture.md      # EventBus API reference
├── Doxyfile                          # Doxygen configuration
└── html/                             # Generated API docs (git-ignored)

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

**DomoticsCore Version:** 2.0+  
**Documentation Last Updated:** 2025-10-05

---

## License

See main repository LICENSE file for licensing information.
