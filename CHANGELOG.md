# Changelog

All notable changes to DomoticsCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.2] - 2025-10-30

### 📚 Critical Documentation Updates

**From Real-World Production Use (WaterMeter Project)**

#### ⚠️ Component Registration Order (CRITICAL)

Added prominent warnings across documentation about component registration order:

**The Issue:**
```cpp
// ❌ WRONG - WILL CRASH (Guru Meditation Error: LoadProhibited)
domotics = new System(config);
domotics->begin();                    // Core injection happens here
myComp = new MyComponent();
domotics->getCore().addComponent(...); // TOO LATE! nullptr crash

// ✅ CORRECT
domotics = new System(config);
myComp = new MyComponent();
domotics->getCore().addComponent(...); // BEFORE begin()
domotics->begin();                     // Now Core is injected correctly
```

**Why This Matters:**
- Core reference injection happens during `begin()` → `initializeAll()`
- Components added after `begin()` never receive the Core pointer
- Calling `getCore()` returns `nullptr` → **Crash with EXCVADDR: 0x00000000**

**Documentation Updated:**
- `docs/CUSTOM_COMPONENTS.md` - Added critical warning section at top
- `GETTING_STARTED.md` - Added "Custom Components - CRITICAL" section
- `README.md` - Added warning box after minimal core example

#### Understanding `getDependencies()` Limitations

Added comprehensive explanation of dependency system limitations:

**Key Discovery:**
- `getDependencies()` **only works** for custom → custom component dependencies
- **Does NOT work** for custom → built-in dependencies (Storage, MQTT, WiFi, etc.)
- Built-in components are registered **during** `begin()`, not before
- Custom components are registered **before** `begin()`

**Correct Pattern:**
```cpp
class MyComponent : public IComponent {
    std::vector<String> getDependencies() const override {
        return {};  // Don't declare built-in dependencies
    }
    
    ComponentStatus begin() override {
        // Always use defensive null checks for built-ins
        auto* storage = getCore()->getComponent<StorageComponent>("Storage");
        if (!storage) {
            DLOG_W("MyComponent", "Storage unavailable, using defaults");
        }
        return ComponentStatus::Success;
    }
};
```

**Documentation Added:**
- Complete section in `CUSTOM_COMPONENTS.md` explaining when to use `getDependencies()`
- Component initialization timeline diagram
- Examples of correct and incorrect patterns
- Defensive programming patterns for built-in components

### 🎓 Learning

These findings come from debugging production crashes in the WaterMeter project. The framework works excellently once these undocumented behaviors are understood.

**Real-World Validation:**
- ✅ ESP32 WaterMeter v0.5.1 running in production
- ✅ All patterns tested and verified
- ✅ Complete working implementation available

### 📖 Reference

See `docs/CUSTOM_COMPONENTS.md` for the complete guide with:
- Critical registration order warning
- getDependencies() limitations explained  
- Component initialization timeline
- ESP32 ISR best practices
- Complete real-world examples

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
