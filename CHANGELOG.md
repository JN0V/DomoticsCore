# Changelog

All notable changes to DomoticsCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-10-31

### ✨ New Features

#### Lifecycle Callback: afterAllComponentsReady()

**Problem Solved:** Need clean separation between internal init and dependency setup.

**New Lifecycle Hook:**
```cpp
class MyComponent : public IComponent {
    std::vector<Dependency> getDependencies() const override {
        return {{"Storage", false}, {"MQTT", false}};  // Optional dependencies
    }
    
    ComponentStatus begin() override {
        // Internal initialization only (GPIO, state)
        pinMode(PIN, INPUT);
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        // ALL components guaranteed available here!
        storage_ = getCore()->getComponent<StorageComponent>("Storage");
        mqtt_ = getCore()->getComponent<MQTTComponent>("MQTT");
        // Framework logs if optional dep missing
    }
};
```

**Benefits:**
- Clear separation: `begin()` = internal init, `afterAllComponentsReady()` = dependency setup
- Framework guarantees all components (including built-ins) are ready
- More intuitive than defensive null checks everywhere
- Non-breaking: virtual with default empty implementation

**Lifecycle Order:**
1. `begin()` - Internal initialization (all components)
2. `afterAllComponentsReady()` - Dependency setup (all components)
3. `loop()` - Normal operation

**Files Changed:**
- `IComponent.h`: Added `afterAllComponentsReady()` virtual method
- `ComponentRegistry.h`: Call hook after all components initialized

---

## [1.0.3] - 2025-10-31

### ✨ New Features

#### Optional Dependencies Support

**Problem Solved:** Custom components couldn't declare built-in dependencies without workarounds.

**Enhanced `getDependencies()` API:**
```cpp
// Simple case - all required (implicit)
std::vector<Dependency> getDependencies() const override {
    return {"ComponentA", "ComponentB"};  // Implicit required=true
}

// Advanced case - mix of required and optional
std::vector<Dependency> getDependencies() const override {
    return {
        {"Storage", false},      // Optional - won't fail if missing
        {"MQTT", false},         // Optional  
        {"MyCustomComp", true}   // Required - init fails if missing
    };
}
```

**Benefits:**
- Explicit intent (required vs optional)
- Framework logs informative messages for missing optional deps
- Implicit conversion from `String` for backward compatibility
- Better than defensive null checks everywhere

**Implementation:**
- Added `Dependency` struct with `name` and `required` flag
- Implicit conversion from `String` (defaults to required=true)
- ComponentRegistry checks optional deps but doesn't fail
- Logs INFO when optional dep missing, ERROR for required

**Files Changed:**
- `IComponent.h`: Enhanced `getDependencies()` to return `Dependency` objects
- `ComponentRegistry.h`: Updated dependency resolution to support optional deps

---

## [1.0.2] - 2025-10-30

### ✨ Lazy Core Injection - Enhanced Flexibility

**Problem Solved:** Components can now be registered at any time, even after `begin()`!

#### 🎯 Technical Solution

Implemented **lazy Core injection** to eliminate registration order constraints:

**Before (rigid order required):**
```cpp
// Had to register before begin()
core.addComponent(...);  // MUST be here
core.begin();            // Or crash!
```

**After (flexible - works anytime):**
```cpp
// Can register anytime!
core.begin();
// ... later ...
core.addComponent(...);  // Works perfectly! Core injected on first getCore() call
```

**How it works:**
1. `ComponentRegistry` is injected immediately when component is registered
2. `getCore()` uses lazy injection - fetches Core from registry on first access
3. Zero overhead: one `if` check per `getCore()` call
4. Memory cost: +4 bytes per component for registry pointer

**Files Changed:**
- `DomoticsCore-Core/include/DomoticsCore/IComponent.h` - Added `__dc_registry` member
- `DomoticsCore-Core/src/IComponent.cpp` - Lazy `getCore()` implementation
- `DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h` - Inject registry on registration

#### 📚 Documentation

Updated `docs/CUSTOM_COMPONENTS.md`:
- Enhanced `getDependencies()` limitations section
- Component initialization timeline diagram
- Defensive programming patterns for built-in dependencies
- Removed registration order warnings (no longer needed!)

**Key Points:**
- `getDependencies()` works for custom → custom dependencies
- Use null checks for built-in components (Storage, MQTT, WiFi, etc.)
- Built-ins are registered during `begin()`, customs before

### 🎓 Real-World Validation

**From Production Use (WaterMeter Project):**
- ✅ ESP32 WaterMeter v0.5.1 running in production
- ✅ Lazy injection tested and verified
- ✅ Zero crashes, perfect stability
- ✅ Flexibility without complexity

### 🚀 Benefits

- **Zero crashes**: No more `nullptr` access errors
- **Developer friendly**: Register components in any order
- **Minimal overhead**: Single `if` check per `getCore()` call
- **Backward compatible**: Existing code works without changes
- **Production proven**: Real-world tested

---

## [1.0.1] - 2025-10-30

### ✨ New Features

#### Core Access in Components (HIGH Priority)
- **Automatic Core Injection**: Components now receive automatic Core reference via `getCore()`
- **No More Boilerplate**: Eliminates manual `setCore()` calls in user projects
- **Consistent Pattern**: Same injection mechanism as EventBus (`__dc_core`)
- **Backward Compatible**: Fully additive, no breaking changes

**Before (Manual):**
```cpp
class MyComponent : public IComponent {
private:
    Core* core_ = nullptr;
public:
    void setCore(Core* c) { core_ = c; }
};
// In main.cpp - manual injection required
myComponent->setCore(&domotics->getCore());
```

**After (Automatic):**
```cpp
class MyComponent : public IComponent {
public:
    ComponentStatus begin() override {
        auto* storage = getCore()->getComponent<StorageComponent>("Storage");
        // Core automatically injected by framework!
    }
};
```

#### Storage uint64_t Support (MEDIUM Priority)
- **Native uint64_t Methods**: Added `putULong64()` and `getULong64()`
- **Cleaner API**: No more blob workarounds for counters
- **Use Cases**: Pulse counters, timestamps, large values

**Before (Blob Workaround):**
```cpp
uint8_t buffer[8];
memcpy(buffer, &value, 8);
storage->putBlob("count", buffer, 8);
```

**After (Native Support):**
```cpp
storage->putULong64("pulse_count", g_pulseCount);
uint64_t count = storage->getULong64("pulse_count", 0);
```

### 📚 Documentation

- **New Guide**: `docs/CUSTOM_COMPONENTS.md` - Comprehensive guide for creating custom components
  - Basic component structure
  - Accessing other components with `getCore()`
  - ESP32 ISR best practices (IRAM requirements)
  - Storage patterns (simple types, uint64_t, binary data)
  - Event Bus communication
  - Non-blocking timers
  - Complete real-world example (Water Meter Component)

### 🔧 Improvements

- **ComponentRegistry**: Enhanced to inject Core reference during initialization
- **IComponent**: Added protected `__dc_core` member and public `getCore()` helper
- **Build System**: Improved error messages in `embed_webui.py`

### 📦 Installation

GitHub installation remains simple:
```ini
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.1

build_unflags = -std=gnu++11
build_flags = -std=gnu++14
board_build.partitions = min_spiffs.csv
```

### 🙏 Acknowledgments

These enhancements come from real-world production use in the WaterMeter project, demonstrating DomoticsCore's readiness for IoT applications.

---

## [1.0.0] - 2025-10-27

### 🎉 First Stable Release

This is the first stable release of DomoticsCore with a complete modular architecture.

### ✨ Features

#### Core Architecture
- **Modular Component System**: Header-only components with zero overhead for unused features
- **Dependency Resolution**: Automatic topological sorting for component initialization
- **Component Registry**: Clean lifecycle management (begin/loop/shutdown)
- **Event Bus**: Inter-component communication with sticky events
- **Status Monitoring**: Component health tracking and diagnostics

#### Components Available

- **Core**: Essential framework with component registry and event bus
- **System**: High-level system orchestration with state management
- **WiFi**: Network connectivity with AP fallback mode
- **LED**: Visual status indication with multiple effects (blink, fade, pulse, breathing, rainbow)
- **Storage**: Persistent data management using NVS Preferences
- **RemoteConsole**: Telnet-based debugging console with log streaming
- **WebUI**: Modern web interface with real-time updates via WebSocket
- **NTP**: Network time synchronization
- **MQTT**: Message broker integration with auto-reconnection
- **OTA**: Secure over-the-air firmware updates
- **HomeAssistant**: Auto-discovery integration
- **SystemInfo**: Real-time system metrics and monitoring

#### System Features

- **LED Error Indicators**: Visual feedback for system states
  - BOOTING: Fast blink (200ms)
  - WIFI_CONNECTING: Slow blink (1000ms)
  - WIFI_CONNECTED: Pulse/heartbeat (2000ms)
  - SERVICES_STARTING: Fade (1500ms)
  - READY: Breathing (3000ms)
  - **ERROR: Fast blink (300ms)** ⚠️
  - OTA_UPDATE: Solid on
  - SHUTDOWN: Off

- **Error State Handling**: Components continue running even when initialization fails, allowing LED indicators and console access for debugging

- **Chunked Transfer Encoding**: WebUI schema endpoint supports large responses (>40KB) via chunked HTTP transfer

### 🏗️ Architecture

- **Examples Provided**:
  - FullStack: Complete system with all components
  - CoreOnly: Minimal setup
  - Component-specific examples for each module

### 🔧 Technical

- **ESP32 Platform**: Optimized for ESP32 microcontrollers
- **Build System**: PlatformIO with proper library dependency management
- **C++14**: Modern C++ features with template-based component system
- **Memory Efficient**: Components only consume resources when used

### 📦 Package Structure

```
DomoticsCore/
├── DomoticsCore-Core/           # Essential framework
├── DomoticsCore-System/         # System orchestration
├── DomoticsCore-WiFi/           # Network connectivity
├── DomoticsCore-LED/            # LED indicators
├── DomoticsCore-Storage/        # Persistent storage
├── DomoticsCore-RemoteConsole/  # Telnet console
├── DomoticsCore-WebUI/          # Web interface
├── DomoticsCore-NTP/            # Time sync
├── DomoticsCore-MQTT/           # MQTT client
├── DomoticsCore-OTA/            # OTA updates
├── DomoticsCore-HomeAssistant/  # HA integration
└── DomoticsCore-SystemInfo/     # System monitoring
```

### 🐛 Bug Fixes

- Fixed LED error indicator not animating when system initialization fails
- Fixed WebUI schema endpoint failing with large payloads (>40KB)
- Fixed component loop not running in ERROR state
- Fixed chunked transfer encoding for AsyncWebServer responses

### 📝 Documentation

- Comprehensive README for each component
- Example applications demonstrating usage patterns
- API documentation in header files
- Architecture diagrams and design patterns

---

## Release Notes

This 1.0.0 release represents a complete, production-ready framework for ESP32 IoT applications. The modular architecture allows you to include only the components you need, keeping your binary size small and your code maintainable.

**Upgrade Path**: This is the first stable release. Future versions will maintain backward compatibility within the 1.x series.

**Breaking Changes**: None (initial release)

[1.0.0]: https://github.com/JN0V/DomoticsCore/releases/tag/v1.0.0
