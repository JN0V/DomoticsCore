# Changelog

All notable changes to DomoticsCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.3] - 2025-12-12

### 🐛 Bug Fixes

- PlatformIO Registry: include WebUI build script (`DomoticsCore-WebUI/embed_webui.py`) and WebUI sources (`DomoticsCore-WebUI/webui_src`) in the exported package

## [1.3.2] - 2025-12-03

### ⚡ Performance

- WebUI: use a static 8KB buffer for WebSocket updates

### ✨ Improvements

- WebUI: add JS/CSS/HTML minification before gzip embedding

### 🐛 Bug Fixes

- MQTT: fix enabled persistence and storage key registration

### 📝 Notes

- Release history around 1.3.x was rebuilt during stabilization; `v1.3.3` is the authoritative tag for the current `main` lineage.

## [1.3.0] - 2025-11-30

### 🏗️ Architecture: Hardware Abstraction Layer (HAL)

Complete HAL refactoring for ESP8266 portability preparation.

#### HAL Files Structure
- **Platform_HAL.h**: Platform detection macros, `getChipId()`, `restart()`, `getFreeHeap()`, `getChipModel()`, `getChipRevision()`, `getCpuFreqMHz()`, `SHA256` class
- **Wifi_HAL.h**: WiFi functions + `NetworkClient`, `SecureNetworkClient` type aliases
- **Storage_HAL.h**: `PlatformStorage` abstraction (Preferences/LittleFS)
- **SystemInfo_HAL.h**: System metrics abstraction
- **NTP_HAL.h**: Time synchronization abstraction

#### Architectural Improvements
- **All platform conditionals** (`#if DOMOTICS_PLATFORM_*`) now only in `*_HAL.h` files
- **MQTT storage removed**: Config persistence now handled by SystemPersistence (consistent with other components)
- **No direct ESP/WiFi calls** outside HAL files
- **Component dependencies updated**: MQTT and RemoteConsole now depend on Wifi component

### 🔧 Components Updated (all to v1.3.0)
- Core, MQTT, OTA, WiFi, Storage, System, WebUI, RemoteConsole, SystemInfo, NTP

### 📦 Dependency Changes
- `DomoticsCore-MQTT` now requires `DomoticsCore-Wifi`
- `DomoticsCore-RemoteConsole` now requires `DomoticsCore-Wifi`

### 🐛 Bug Fixes
- Fixed cross-component HAL dependencies in examples
- Fixed `getChipRevision()` missing from Platform_HAL

---

## [1.2.1] - 2025-11-14

### 🚨 Breaking Changes

**EventBus-Only Communication** - MQTT and HomeAssistant components now use EventBus exclusively

#### MQTT Component
- **REMOVED:** Direct callback methods (`onConnect`, `onDisconnect`, `onMessage`)
- **NEW:** EventBus-based communication via topics
  - Emits: `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`
  - Listens: `mqtt/publish`, `mqtt/subscribe`
- **Migration:** Use `core.on<>()` and `core.emit()` for event handling
- **See:** `MIGRATING_TO_V1.2.md` for detailed migration guide

#### HomeAssistant Component
- **REMOVED:** `MQTTComponent*` parameter from constructor
- **NEW:** EventBus-based MQTT communication (no direct dependency)
- **Before:** `HomeAssistantComponent(mqttPtr, haConfig)`
- **After:** `HomeAssistantComponent(haConfig)`
- **Impact:** Fully decoupled from MQTT component

### ✨ New Features

#### Core EventBus Helpers
- **NEW:** `Core::on<>()` and `Core::emit()` methods for cleaner API
- **Before:** `core.getEventBus().subscribe()` / `core.getEventBus().publish()`
- **After:** `core.on<bool>("topic", callback)` / `core.emit("topic", data)`
- **Impact:** Consistent API between Core and IComponent

### 🐛 Critical Bug Fixes

#### Bug #1: EventBus Listener Registration
- **Problem:** EventBus listeners registered AFTER configuration checks
- **Impact:** Discovery/subscriptions failed after WebUI configuration (reboot required)
- **Fix:** Listeners now registered BEFORE configuration checks
- **Result:** Discovery works immediately after WebUI config (no reboot needed)

#### Bug #2: PubSubClient Callback Registration  
- **Problem:** `mqttClient.setCallback()` called AFTER configuration checks
- **Impact:** Incoming MQTT messages silently dropped (switch commands not received)
- **Fix:** Callback registered BEFORE configuration checks
- **Result:** All MQTT messages properly received

### ✨ Improvements

- **Architecture:** Complete EventBus decoupling for inter-component communication
- **API Consistency:** Same `on<>()` / `emit()` API in Core and IComponent
- **Modularity:** Components can be tested independently
- **Maintainability:** Reduced tight coupling and component dependencies
- **Examples:** Updated all MQTT/HomeAssistant examples to use EventBus
- **Documentation:** Added comprehensive EventBus architecture guide

### 📚 Documentation

- **NEW:** `docs/reference/eventbus-architecture.md` - Complete EventBus reference
- **NEW:** `docs/migration/v1.2.0.md` - Migration guide from v1.1.x
- **REORGANIZED:** Documentation structure with guides/, migration/, reference/ folders
- **UPDATED:** All component examples with EventBus usage
- **UPDATED:** Component README files with new APIs

### 🔄 Component Versions

- **DomoticsCore-MQTT:** 1.0.1 → 1.2.0
- **DomoticsCore-HomeAssistant:** 1.0.2 → 1.2.0  
- **DomoticsCore-System:** 1.0.2 → 1.2.0
- **DomoticsCore-Core:** 1.1.4 → 1.2.0

### 📦 Migration Required

This is a breaking release. See [Migration Guide](docs/migration/v1.2.1.md) for step-by-step migration instructions.

---

## [1.1.4] - 2025-11-07

### 🔧 Architecture Improvements

**Unified Component Metadata System** - Simplified component identification

#### Changes
Refactored component metadata initialization for consistency and proper dependency resolution.

**Before (inconsistent):**
- Some components: `getName()` override returning hardcoded string
- Other components: `metadata.name` in `begin()` (too late for dependency resolution)
- Mixed approach created confusion and potential bugs

**After (unified):**
```cpp
// All components now initialize metadata in constructor
ComponentName() {
    metadata.name = "ComponentName";
    metadata.version = "1.0.0";
    metadata.author = "DomoticsCore";
    metadata.description = "Component description";
}
```

#### Benefits
- ✅ **Consistency:** All components use same pattern
- ✅ **Early availability:** Metadata ready before dependency resolution
- ✅ **Cleaner API:** Removed redundant `getName()` and `getVersion()` overrides
- ✅ **Direct access:** `component->metadata.name` instead of `component->getName()`
- ✅ **Simpler:** One source of truth (metadata struct)

#### Components Updated
All components now follow unified pattern:
- LED, Storage, Wifi, NTP, WebUI, OTA, MQTT, HomeAssistant
- SystemInfo, RemoteConsole, System

#### Technical Details
- Made `metadata` and `config` public in `IComponent` for external access
- Removed `getName()` and `getVersion()` virtual methods
- All metadata initialization moved to constructors
- Updated all WebUI providers to use `metadata.name` and `metadata.version`
- Added `eventBus()` helper method to `IComponent` for direct EventBus API access

**Result:** Cleaner, more maintainable codebase with proper early metadata initialization.

#### Examples Tested
All examples compile and work correctly:
- ✅ FullStack (1.38MB flash, 88.2%)
- ✅ WebUIOnly (1.04MB flash, 79.6%)
- ✅ CoreWithDummyComponent (320KB flash, 24.4%)
- ✅ 03-EventBusBasics (313KB flash, 23.9%)
- ✅ 04-EventBusCoordinators (315KB flash, 24.0%)
- ✅ 05-EventBusTests (315KB flash, 24.1%)

---

## [1.1.3] - 2025-11-01

### ✨ New Features

**Storage Namespace Configuration** - Project isolation for NVS storage

#### Feature
Configurable storage namespace for better project isolation on shared devices.

**System-level:**
```cpp
SystemConfig sysConfig = SystemConfig::standard();
sysConfig.storageNamespace = "watermeter";  // Custom namespace instead of "domotics"
System system(sysConfig);
```

**Component-level:**
```cpp
StorageConfig config;
config.namespace_name = "myproject";
auto storage = new StorageComponent(config);
```

#### Benefits
- ✅ **Isolation:** Multiple projects on same device without key conflicts
- ✅ **Clarity:** Each project has its own namespace
- ✅ **Development:** Easier to test multiple firmwares
- ✅ **Backward compatible:** Defaults to "domotics"

#### Testing
New unit test: `DomoticsCore-Storage/tests/namespace-isolation/`
- Tests namespace isolation with two Storage instances
- Validates same keys store different values in different namespaces
- Tests all data types (String, Int, Bool, Float, ULong64)
- Validates clear() per namespace
- Automated pass/fail with assertion macros

**Test Results:**
- ✅ All namespace isolation tests pass
- ✅ FullStack example compiles with namespace config
- ✅ Backward compatible (defaults to "domotics")

---

## [1.1.2] - 2025-11-01

### 🐛 Bug Fixes

**Fixed:** WifiComponent AP mode broken after v1.1.1 refactoring

#### Problem
- AP configured and logs showed "AP started: FullStackDevice-ac89 (IP: 192.168.4.1)"
- But AP not visible/accessible, system showed IP: 0.0.0.0
- FullStack example affected (any config with AP + empty STA credentials)

#### Root Cause
In `Wifi.h::begin()`, the code always set `WiFi.mode(WIFI_STA)` at the beginning, overriding the AP mode that was configured earlier via `enableAP()`. When SSID was empty, `begin()` returned early without restoring the mode, leaving WiFi in inconsistent state.

#### Solution
Added AP mode detection in `begin()`:
```cpp
// If AP already enabled, don't override mode
if (ssid.isEmpty() && apEnabled) {
    DLOG_I(LOG_WIFI, "No STA credentials - AP-only mode");
    return ComponentStatus::Success;  // Keep AP mode
}
```

**Result:** AP mode now works correctly with empty STA credentials.

#### Testing
- ✅ FullStack example: AP visible at 192.168.4.1
- ✅ WebUI accessible at http://192.168.4.1:80
- ✅ Telnet accessible at 192.168.4.1:23

---

## [1.1.1] - 2025-10-31

### 🎯 Eliminated Early-Init Requirement

**Major Improvement:** Refactored WifiComponent and System to eliminate Storage early-init pattern.

#### WifiComponent Enhancements

**New Features:**
```cpp
// Constructor now accepts empty credentials
WifiComponent wifi;  // Create without credentials
wifi.setCredentials("MySSID", "password");  // Configure later

// Automatic connection in afterAllComponentsReady()
void afterAllComponentsReady() override {
    // Connects to WiFi if credentials were set via setCredentials()
}
```

**Changes:**
- Constructor parameter `ssid` is now optional (defaults to empty)
- Added `setCredentials(ssid, password)` method for late configuration
- `begin()` skips connection if SSID is empty (waits for setCredentials)
- `afterAllComponentsReady()` connects if credentials set after begin()

#### System Improvements

**Before (v1.1.0):** Storage early-init required
```cpp
// Storage initialized BEFORE core.begin() to load WiFi credentials
storage->begin();  // Early-init
storage->setActive(true);
config.wifiSSID = storage->getString("wifi_ssid", "");
auto wifi = new WifiComponent(config.wifiSSID, config.wifiPassword);
core.begin();
```

**After (v1.1.1):** Clean component initialization
```cpp
// All components registered normally
core.addComponent(std::make_unique<StorageComponent>());
core.addComponent(std::make_unique<WifiComponent>());  // Empty credentials OK
core.begin();  // All components initialized in dependency order

// Load credentials AFTER core.begin() (Storage is ready)
auto* storage = core.getComponent<StorageComponent>("Storage");
auto* wifi = core.getComponent<WifiComponent>("Wifi");
wifi->setCredentials(storage->getString("wifi_ssid", ""), 
                     storage->getString("wifi_password", ""));
```

**Result:**
- ✅ Storage early-init eliminated
- ✅ Only LED early-init remains (justified for boot error visualization)
- ✅ Cleaner code, easier to understand
- ✅ Better separation of concerns

#### Documentation Updates

- Roadmap: Marked early-init elimination as implemented
- Technical notes: Updated to reflect new pattern
- Examples: All compile successfully with new pattern

#### Testing

- ✅ Minimal example: 860KB flash, 45KB RAM
- ✅ Backward compatible: Existing code with credentials in constructor still works
- ✅ New pattern validated: Empty credentials + setCredentials() works

---

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
