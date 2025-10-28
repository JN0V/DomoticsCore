# DomoticsCore Architecture

**Version:** 1.0.0  
**Status:** Production Ready

---

## Overview

DomoticsCore is a modular IoT framework for ESP32 with a clean component architecture and "batteries included" System component for rapid development.

---

## Header-Only Design

**Rationale:** Most components are header-only (`.h` files only) for simplicity and optimization.

### Why Header-Only?

✅ **Advantages:**
1. **No Linking Issues** - PlatformIO automatically includes headers, no manual library configuration
2. **Better Optimization** - Compiler can inline everything, reducing overhead
3. **Simpler Build** - No precompiled libraries (.a files), no linking errors
4. **Zero Overhead** - Unused functions are eliminated at compile-time
5. **ESP32 Friendly** - Embedded systems benefit from aggressive inlining

❌ **Exceptions (have .cpp files):**
- **DomoticsCore-Core** - Complex logic, reduce compilation time
- **DomoticsCore-OTA** - Large implementation, better in .cpp

### Implementation Pattern

Most components use a clean header-only approach:

```cpp
// LED.h (typical header-only component)
class LEDComponent : public IComponent {
    void begin() override {
        // Implementation directly in header
    }
};
```

**MQTT Pattern (header-only with separation):**

Some components separate declaration/implementation while staying header-only:

```cpp
// MQTT.h (declaration)
class MQTTComponent : public IComponent {
    void begin() override;  // Declared
};

#include "MQTT_impl.h"  // ← Include implementation at end

// MQTT_impl.h (implementation)
inline void MQTTComponent::begin() {
    // Implementation here
}
```

**Why `MQTT_impl.h` instead of `MQTT.cpp`?**
- Keeps it header-only (no compilation unit)
- Separates interface from implementation (readability)
- Still gets inlining benefits
- Avoids `.cpp` linking in PlatformIO

**This is NOT ESP32-specific** - it's a C++ pattern commonly used in template-heavy or embedded libraries (Boost, Eigen, etc.).

---

## Current Architecture

```
ComponentRegistry (Core)
    ↓ (Coordinates: dependency resolution, lifecycle)
    ↓
  System Component
    ↓ (Provides: WiFi, LED, Console, State Management)
    ↓ (Optional: WebUI, MQTT, HA, NTP, OTA, Storage)
    ↓
  Developer
    ↓ (Focus on application logic only)
```

**Clear Responsibilities:**
- **ComponentRegistry**: Coordinates components (dependencies, init order, lifecycle)
- **System**: High-level API, automatic setup, state management
- **Developer**: Application logic only

---

## Component List

### Core Components (12 total)

```
DomoticsCore/
├── DomoticsCore-Core/              ← Foundation (ComponentRegistry)
├── DomoticsCore-System/            ← All-in-one (Recommended)
├── DomoticsCore-LED/               ← LED effects & patterns
├── DomoticsCore-RemoteConsole/     ← Telnet debugging
├── DomoticsCore-Wifi/              ← WiFi with AP fallback
├── DomoticsCore-MQTT/              ← Message broker
├── DomoticsCore-WebUI/             ← Web interface
├── DomoticsCore-HomeAssistant/     ← HA auto-discovery
├── DomoticsCore-NTP/               ← Time sync
├── DomoticsCore-OTA/               ← Remote updates
├── DomoticsCore-Storage/           ← Persistent config
└── DomoticsCore-SystemInfo/        ← System monitoring
```

---

## System Component (Batteries Included)

### Preset Configurations

**Minimal** (WiFi, LED, Console):
```cpp
SystemConfig config = SystemConfig::minimal();
config.deviceName = "MyDevice";
System domotics(config);
domotics.begin();  // Everything automatic!
```

**Standard** (+ WebUI, NTP, Storage):
```cpp
SystemConfig config = SystemConfig::standard();
```

**FullStack** (+ MQTT, HA, OTA):
```cpp
SystemConfig config = SystemConfig::fullStack();
config.mqttBroker = "192.168.1.100";
config.otaPassword = "secure123";
```

### Built-in State Management

```cpp
enum class SystemState {
    BOOTING,           // Initial boot
    WIFI_CONNECTING,   // Connecting to WiFi
    WIFI_CONNECTED,    // WiFi established
    SERVICES_STARTING, // Starting services
    READY,             // All services operational
    ERROR,             // Critical error
    OTA_UPDATE,        // Firmware update
    SHUTDOWN           // Graceful shutdown
};
```

**Automatic LED Patterns:**
- BOOTING → Fast blink (200ms)
- WIFI_CONNECTING → Slow blink (1s)
- WIFI_CONNECTED → Heartbeat (2s)
- READY → Breathing (3s)
- ERROR → Fast blink (300ms)

### Developer Experience

**Before (100+ lines):**
```cpp
// Manual LED setup
auto led = std::make_unique<LEDComponent>();
led->addSingleLED(2, "status");
core.addComponent(std::move(led));

// Manual WiFi
WiFi.begin(ssid, password);
// ... connection logic ...

// Manual console
auto console = std::make_unique<RemoteConsoleComponent>(config);
core.addComponent(std::move(console));

core.begin();
```

**After (10 lines):**
```cpp
SystemConfig config = SystemConfig::minimal();
config.deviceName = "MyDevice";
System domotics(config);
domotics.begin();  // Everything automatic!
```

---

## 1. Repository Structure

### Current: Monorepo ✅ RECOMMENDED

**Structure:**
```
DomoticsCore/
├── library.json (meta-package)
├── DomoticsCore-Core/
├── DomoticsCore-System/          ← NEW: All-in-one component
├── DomoticsCore-LED/
├── DomoticsCore-RemoteConsole/
├── DomoticsCore-Wifi/
├── DomoticsCore-MQTT/
├── DomoticsCore-WebUI/
├── DomoticsCore-HomeAssistant/
├── DomoticsCore-NTP/
├── DomoticsCore-OTA/
├── DomoticsCore-Storage/
└── DomoticsCore-SystemInfo/
```

### Monorepo Structure

**Rationale:**

✅ **Advantages:**
1. **Atomic Changes** - Update multiple components in a single commit
2. **Easier Testing** - Test inter-component dependencies immediately
3. **Simplified Versioning** - Coordinated releases across all components
4. **Developer Experience** - Clone once, get everything
5. **Cross-component Refactoring** - Make breaking changes across components safely
6. **Shared CI/CD** - Single build pipeline for all components
7. **Documentation Consistency** - Central docs generation

✅ **Current Success:**
- Examples work seamlessly across components
- Build system optimized for monorepo
- No dependency version conflicts

❌ **Multi-repo Drawbacks:**
- Version hell (Component A v1.2 needs MQTT v2.1 but Core v0.3...)
- Coordination overhead (breaking changes require PRs across repos)
- Testing complexity (integration tests scattered)
- Documentation fragmentation
- Release coordination nightmares

### Top-Level library.json Analysis

**Current Status:** ✅ **CORRECT - Keep It**

```json
{
  "name": "DomoticsCore",
  "version": "1.0.0",
  "description": "ESP32 domotics framework...",
  "dependencies": []
}
```

**Purpose:**
1. **Meta-package** - Represents the entire framework
2. **PlatformIO Registry** - Searchable package name
3. **Version Coordination** - Single version for the framework
4. **Documentation Landing** - Entry point for users

**Recommendation:** 
- ✅ Keep the top-level `library.json`
- ✅ It acts as the "umbrella" package
- ✅ Individual components can still be used independently
- ✅ Users can reference either:
  - `DomoticsCore` (gets everything, if we add dependencies)
  - `DomoticsCore-Core` (just the core)
  - `DomoticsCore-MQTT` (specific component)

**Enhancement:**
Consider adding dependencies to make it a true meta-package:

```json
{
  "name": "DomoticsCore",
  "version": "1.0.0",
  "description": "ESP32 domotics framework...",
  "dependencies": [
    { "name": "DomoticsCore-Core", "version": "^1.0.0" },
    { "name": "DomoticsCore-MQTT", "version": "^1.0.0" },
    { "name": "DomoticsCore-WebUI", "version": "^1.0.0" }
  ]
}
```

This would allow users to install the entire suite with one dependency.

---

## 2. Alternative: Multi-repo Consideration

### When to Split (Future):

**Only if:**
1. **Independent Release Cycles** - Components evolve completely independently
2. **External Adoption** - Core becomes library for non-IoT ESP32 projects
3. **Team Size >10** - Separate teams own different components
4. **Licensing Differences** - Some components need different licenses

**Current Recommendation:** **DON'T SPLIT**

Your project is:
- ✅ Cohesive ecosystem
- ✅ Designed to work together
- ✅ Manageable size (<20 components)
- ✅ Single maintainer/small team

---

## 3. Publishing Strategy

### PlatformIO Library Registry

**Individual Publishing:**
Each component is independently publishable:

```bash
# Publish individually
pio pkg publish DomoticsCore-Core
pio pkg publish DomoticsCore-MQTT
pio pkg publish DomoticsCore-HomeAssistant
```

**Benefits:**
- ✅ Users can cherry-pick components
- ✅ Each component has its own version
- ✅ Dependencies managed per component
- ✅ Still in one repo for development

---

## 4. Versioning Strategy

**Recommendation:** Synchronized major/minor, independent patches

```
DomoticsCore          v1.0.0
├── Core              v1.0.0
├── MQTT              v1.0.0
├── WebUI             v1.0.0
├── HomeAssistant     v1.0.0
└── NTP               v1.0.0
```

**Rules:**
1. **Major versions** synchronized across all components (currently v1.0.0)
2. **Minor versions** synchronized for breaking changes
3. **Patch versions** can diverge for bug fixes
4. Use semantic versioning strictly

---

## 3. System Component Design

### "Batteries Included" Approach

**Rationale:**

The `System` component provides a high-level API that handles common setup automatically.

**Design Principles:**

1. **Preset Configurations**
   - `minimal()` - WiFi, LED, Console
   - `standard()` - + WebUI, NTP, Storage
   - `fullStack()` - + MQTT, HA, OTA

2. **Automatic Setup**
   - State management built-in
   - LED patterns automatic
   - WiFi with AP fallback
   - Component initialization

3. **Progressive Enhancement**
   - Start simple (minimal)
   - Add features as needed (standard)
   - Full stack when required (fullStack)

**Benefits:**
- ✅ 10x less boilerplate code
- ✅ Consistent developer experience
- ✅ Production-ready defaults
- ✅ Still extensible for advanced users

---

## Summary

| Aspect | Decision | Status |
|--------|----------|--------|
| Repository Structure | Monorepo | ✅ Keep |
| Top-level library.json | Meta-package | ✅ Keep |
| Component Independence | Separate library.json per component | ✅ Current |
| Publishing | Individual + Meta-package | ✅ Implement |
| Versioning | Synchronized major/minor | ✅ Adopt |
| Coordinator Component | Removed, merged into System | ✅ Done |
| System Component | Batteries included approach | ✅ Implemented |

**Component Count:**
- **Before:** 13 components (including Coordinator)
- **After:** 12 components (Coordinator removed, System added)

**Next Steps:**
1. ✅ Keep current monorepo structure
2. ✅ Optionally enhance top-level library.json with dependencies
3. ✅ Publish individual components to registry
4. ✅ Synchronize all components to v1.0.0
4. ✅ Document component interdependencies clearly
5. ✅ Maintain CHANGELOG.md at root and per-component
