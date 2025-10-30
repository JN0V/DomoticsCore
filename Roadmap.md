# DomoticsCore - Roadmap pour installation GitHub simplifiée

## Contexte
DomoticsCore v1.0 est actuellement un monorepo avec 12 composants modulaires. L'installation depuis GitHub via PlatformIO nécessite des configurations manuelles complexes (12 chemins `-I` dans build_flags, script embed_webui.py qui échoue, etc.).

**Objectif**: Rendre l'installation triviale avec juste:
```ini
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

## Solution retenue: Option B - Fixer le monorepo pour PlatformIO

### Tâche 1: Mettre à jour library.json racine

Modifier `/library.json` pour que PlatformIO détecte automatiquement tous les composants:

```json
{
  "name": "DomoticsCore",
  "version": "1.0.0",
  "description": "ESP32 domotics framework with WiFi, MQTT, web interface, Home Assistant integration, and persistent storage",
  "keywords": ["esp32", "domotics", "iot", "mqtt", "home-assistant", "wifi"],
  "repository": {
    "type": "git",
    "url": "https://github.com/JN0V/DomoticsCore.git"
  },
  "authors": [
    {
      "name": "JN0V",
      "maintainer": true
    }
  ],
  "license": "MIT",
  "homepage": "https://github.com/JN0V/DomoticsCore",
  "frameworks": ["arduino"],
  "platforms": ["espressif32"],
  "export": {
    "include": [
      "DomoticsCore-*/include"
    ]
  },
  "build": {
    "includeDir": ".",
    "flags": [
      "-IDomoticsCore-Core/include",
      "-IDomoticsCore-System/include",
      "-IDomoticsCore-Wifi/include",
      "-IDomoticsCore-LED/include",
      "-IDomoticsCore-Storage/include",
      "-IDomoticsCore-WebUI/include",
      "-IDomoticsCore-MQTT/include",
      "-IDomoticsCore-HomeAssistant/include",
      "-IDomoticsCore-OTA/include",
      "-IDomoticsCore-NTP/include",
      "-IDomoticsCore-RemoteConsole/include",
      "-IDomoticsCore-SystemInfo/include"
    ],
    "srcFilter": [
      "+<DomoticsCore-*/src/*>"
    ],
    "lib_archive": false
  },
  "dependencies": [
    {
      "owner": "bblanchon",
      "name": "ArduinoJson",
      "version": "^7.0.0"
    },
    {
      "owner": "ESP32Async",
      "name": "ESPAsyncWebServer",
      "version": "^3.8.0"
    },
    {
      "owner": "ESP32Async",
      "name": "AsyncTCP",
      "version": "^3.4.8"
    },
    {
      "owner": "knolleary",
      "name": "PubSubClient",
      "version": "^2.8"
    }
  ]
}
```

**Points clés:**
- `export.include`: Exporte tous les dossiers `include` des composants
- `build.flags`: Ajoute automatiquement tous les chemins include
- `build.srcFilter`: Compile tous les fichiers .cpp des composants
- `build.lib_archive: false`: Évite le cache problématique
- `dependencies`: Déclare les dépendances externes

### Tâche 2: Fixer embed_webui.py

Le script `DomoticsCore-WebUI/embed_webui.py` échoue quand installé via PlatformIO car il assume la structure du monorepo local.

**Solution: Rendre le script robuste pour détecter l'environnement**

Modifier `embed_webui.py` pour détecter automatiquement si on est en développement local ou installé via PlatformIO:

```python
Import("env")
import os
from pathlib import Path

def find_package_root():
    """Trouve la racine du package, que ce soit en dev local ou via PlatformIO"""
    try:
        # Essayer avec __file__ (marche en local et PlatformIO)
        script_path = Path(__file__).resolve()
        
        # Monorepo local: chercher le dossier 'packages' en remontant
        current = script_path.parent
        while current.parent != current:
            if (current / "packages").exists():
                # Dev local: retourner packages/
                return current / "packages"
            current = current.parent
        
        # Installation PlatformIO: pas de dossier 'packages' trouvé
        # Le script est dans DomoticsCore-WebUI/
        webui_root = script_path.parent
        
        # Chercher webui_src/ dans DomoticsCore-WebUI/
        webui_src = webui_root / "webui_src"
        if webui_src.exists():
            return webui_src
        
        # Fallback: peut-être dans le parent (structure alternative)
        parent_webui_src = webui_root.parent / "packages"
        if parent_webui_src.exists():
            return parent_webui_src
        
        raise RuntimeError(
            f"Could not locate WebUI sources.\n"
            f"Searched in:\n"
            f"  - {webui_src}\n"
            f"  - {parent_webui_src}\n"
            f"Please ensure 'webui_src/' or 'packages/' directory exists."
        )
        
    except NameError:
        # __file__ non disponible (environnement SCons très spécial)
        raise RuntimeError(
            "Unable to resolve paths: __file__ is not available.\n"
            "This is an unusual SCons environment."
        )

# Continuer avec la génération normale
package_root = find_package_root()
print(f"Using WebUI sources from: {package_root}")

# Suite du code de génération...
# ...
```

### Tâche 3: Ajouter un exemple d'installation GitHub

Créer `examples/GitHubInstallation/` avec un exemple minimal qui fonctionne:

**examples/GitHubInstallation/platformio.ini:**
```ini
; Example: Installing DomoticsCore from GitHub
; Just add the GitHub URL to lib_deps and it works!

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0

build_flags = 
    -DCORE_DEBUG_LEVEL=3
```

**examples/GitHubInstallation/src/main.cpp:**
```cpp
#include <Arduino.h>
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "GitHub-Install-Test";
    config.wifiSSID = "";  // AP mode
    config.wifiPassword = "";
    config.ledPin = 2;
    
    domotics = new System(config);
    
    if (!domotics->begin()) {
        Serial.println("System init failed!");
        while(1) delay(1000);
    }
    
    Serial.println("System ready! Access point or WebUI available.");
}

void loop() {
    domotics->loop();
}
```

**examples/GitHubInstallation/README.md:**
```markdown
# Installing DomoticsCore from GitHub

This example demonstrates how simple it is to use DomoticsCore directly from GitHub.

## Installation

1. Create your project with this `platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

2. Include and use:
```cpp
#include <DomoticsCore/System.h>
```

3. Build and upload:
```bash
pio run -t upload -t monitor
```

That's it! No manual configuration needed.

## What you get automatically
- All DomoticsCore components
- Correct include paths
- All dependencies (ArduinoJson, ESPAsyncWebServer, etc.)
- WebUI assets generated at compile time

## Troubleshooting
If compilation fails, ensure you're using DomoticsCore v1.0.0 or later:
```ini
lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```
```

### Tâche 4: Mettre à jour GETTING_STARTED.md

Ajouter une section "Installation from GitHub" au début de GETTING_STARTED.md:

```markdown
## 🚀 Quick Start

### Installation from GitHub (Recommended during development)

**Step 1: Add to platformio.ini**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

**Step 2: Write your code**
```cpp
#include <DomoticsCore/System.h>

using namespace DomoticsCore;
System* domotics = nullptr;

void setup() {
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "MyDevice";
    config.wifiSSID = "";  // Empty = AP mode
    
    domotics = new System(config);
    domotics->begin();
}

void loop() {
    domotics->loop();
}
```

**Step 3: Build**
```bash
pio run -t upload -t monitor
```

That's it! Everything is configured automatically.

---

### Installation from PlatformIO Registry (Coming soon)
Once published on the registry:
```ini
lib_deps = 
    DomoticsCore-System
```
```

### Tâche 5: Test de validation

Créer un test automatisé dans `.github/workflows/test-github-install.yml`:

```yaml
name: Test GitHub Installation

on: [push, pull_request]

jobs:
  test-install:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Install PlatformIO
        run: |
          pip install platformio
      
      - name: Create test project
        run: |
          mkdir test-install
          cd test-install
          cat > platformio.ini << EOF
          [env:esp32dev]
          platform = espressif32
          board = esp32dev
          framework = arduino
          lib_deps = 
              https://github.com/JN0V/DomoticsCore.git#${GITHUB_SHA}
          EOF
          
          mkdir src
          cat > src/main.cpp << 'EOF'
          #include <DomoticsCore/System.h>
          using namespace DomoticsCore;
          System* sys = nullptr;
          void setup() {
              SystemConfig config;
              config.deviceName = "Test";
              sys = new System(config);
              sys->begin();
          }
          void loop() { sys->loop(); }
          EOF
      
      - name: Test compilation
        run: |
          cd test-install
          pio run
```

## Ordre d'implémentation

1. ✅ **Mettre à jour library.json** racine avec les chemins include
2. ✅ **Modifier embed_webui.py** pour détecter l'environnement (local vs PlatformIO)
3. ✅ **Créer l'exemple GitHubInstallation/**
4. ✅ **Mettre à jour GETTING_STARTED.md**
5. ✅ **Ajouter le workflow de test** GitHub Actions
6. ✅ **Tester manuellement** avec un projet vierge
7. ⏳ **Publier v1.0.1** avec ces corrections

## Test manuel

```bash
# Dans un nouveau dossier vide
mkdir /tmp/test-domotics && cd /tmp/test-domotics

cat > platformio.ini << EOF
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.0
EOF

mkdir src
cat > src/main.cpp << 'EOF'
#include <DomoticsCore/System.h>
using namespace DomoticsCore;
System* sys = nullptr;
void setup() {
    SystemConfig config;
    sys = new System(config);
    sys->begin();
}
void loop() { sys->loop(); }
EOF

pio run  # DOIT compiler sans erreur ✅
```

## Résultat attendu

Après ces modifications, un utilisateur pourra simplement faire:

```ini
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

Et `#include <DomoticsCore/System.h>` fonctionnera immédiatement, sans:
- ❌ Chemins include manuels
- ❌ Scripts extra
- ❌ Configuration complexe
- ❌ Erreurs de compilation

## Notes supplémentaires

- Garder la structure modulaire actuelle intacte
- Ces changements sont rétro-compatibles avec le développement local
- Une fois stable, publier sur le registre PlatformIO pour `lib_deps = DomoticsCore-System`

---

# DomoticsCore v1.0 - API Enhancement Suggestions

**From:** WaterMeter Project (Real-world ESP32 IoT water meter implementation)  
**Date:** October 2025  
**Context:** Full production implementation using all DomoticsCore features

## Executive Summary

After developing a complete ESP32 water meter using DomoticsCore v1.0, here are practical enhancement suggestions based on real-world usage. The project successfully uses:
- Custom IComponent with ISR handling
- Storage for uint64_t counters
- NTP for time-based resets
- Event bus for MQTT publishing
- All fullStack() features

## 🎯 Enhancement Suggestions

### 1. **Core Access in IComponent** (Priority: **HIGH**)

**Current Issue:**
```cpp
// Manual boilerplate required in every project
class WaterMeterComponent : public IComponent {
private:
    Core* core_ = nullptr;  // Have to add this
    
public:
    void setCore(Core* c) { core_ = c; }  // And this
};

// In main.cpp
waterMeter = new WaterMeterComponent();
waterMeter->setCore(&domotics->getCore());  // Manual injection
domotics->getCore().addComponent(std::unique_ptr<WaterMeterComponent>(waterMeter));
```

**Proposed Solution:**
```cpp
class IComponent {
protected:
    Core* __dc_core = nullptr;  // Automatically injected by framework
    
public:
    void __dc_setCore(Core* c) { __dc_core = c; }
    
    // Helper for easy access
    Core* getCore() const { return __dc_core; }
};
```

**Benefits:**
- Consistent pattern with `__dc_eventBus` (already exists)
- No boilerplate code in user projects
- Easier for beginners
- Framework handles injection automatically

**Implementation Notes:**
- ComponentRegistry should call `__dc_setCore()` when registering
- Same lifecycle as `__dc_setEventBus()`
- Zero breaking changes (additive only)

---

### 2. **Storage uint64_t Support** (Priority: **MEDIUM**)

**Current Issue:**
```cpp
// Complex workaround needed for uint64_t
void saveToStorage() {
    auto* storage = core_->getComponent<StorageComponent>("Storage");
    uint8_t buffer[8];
    memcpy(buffer, (const void*)&g_pulseCount, 8);
    storage->putBlob("pulse_count", buffer, 8);
}

void loadFromStorage() {
    uint8_t buffer[8];
    if (storage->getBlob("pulse_count", buffer, 8) == 8) {
        memcpy((void*)&g_pulseCount, buffer, 8);
    }
}
```

**Proposed Solution:**
```cpp
class StorageComponent {
public:
    // Add native uint64_t support
    bool putULong64(const String& key, uint64_t value) {
        return preferences.putULong64(key.c_str(), value);
    }
    
    uint64_t getULong64(const String& key, uint64_t defaultValue = 0) {
        return preferences.getULong64(key.c_str(), defaultValue);
    }
};

// Usage becomes simple
storage->putULong64("pulse_count", g_pulseCount);
g_pulseCount = storage->getULong64("pulse_count", 0);
```

**Benefits:**
- Consistent API (putInt, putFloat, putString, putULong64)
- More readable, less error-prone
- ESP32 Preferences natively supports uint64_t
- No volatile pointer casting issues

**Use Case:**
- Counters (pulse counts, uptime seconds)
- Timestamps (milliseconds since epoch)
- Large values (total bytes transferred, etc.)

---

### 3. **Component Lifecycle Enhancement** (Priority: **LOW**)

**Current Issue:**
Components accessing dependencies in `begin()` must check availability:
```cpp
ComponentStatus begin() override {
    loadFromStorage();  // Storage might not be ready yet
    return ComponentStatus::Success;
}

void loadFromStorage() {
    if (!core_) return;  // Check 1
    auto* storage = core_->getComponent<StorageComponent>("Storage");
    if (!storage) {      // Check 2
        DLOG_W(LOG_WATER, "Storage not available");
        return;
    }
    // Actually use storage...
}
```

**Proposed Solution:**
```cpp
class IComponent {
    virtual ComponentStatus begin() = 0;           // Existing
    virtual void afterBegin() {}                   // NEW - Called after all components are ready
    virtual void loop() = 0;
    virtual ComponentStatus shutdown() = 0;
};

// Usage
ComponentStatus begin() override {
    // Internal initialization only
    pinMode(GPIO_PIN, INPUT);
    return ComponentStatus::Success;
}

void afterBegin() override {
    // Dependencies are guaranteed available here
    auto* storage = core_->getComponent<StorageComponent>("Storage");
    loadFromStorage();  // No null checks needed
}
```

**Benefits:**
- Clear separation: `begin()` = internal init, `afterBegin()` = dependency setup
- No defensive null checks everywhere
- Framework guarantees all dependencies are ready
- Cleaner, more maintainable code

**Implementation:**
- ComponentRegistry calls all `begin()` first
- Then calls all `afterBegin()` in topological order
- Non-breaking (virtual with default empty implementation)

---

### 4. **Documentation: Custom Components Guide** (Priority: **HIGH**)

**Currently Missing:**
- How to access Core from custom components
- Best practices for components with dependencies
- ISR handling patterns for ESP32
- Storage patterns for different data types
- Real-world examples beyond basic demos

**Proposed Documentation Structure:**

```markdown
## Creating Custom Components

### 1. Basic Component
```cpp
class MyComponent : public IComponent {
    ComponentStatus begin() override {
        // Initialize your hardware/state
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Your periodic logic
    }
    
    ComponentStatus shutdown() override {
        // Cleanup
        return ComponentStatus::Success;
    }
};
```

### 2. Accessing Other Components
```cpp
class MyComponent : public IComponent {
private:
    Core* core_ = nullptr;
    
public:
    void setCore(Core* c) { core_ = c; }
    
    ComponentStatus begin() override {
        auto* storage = core_->getComponent<StorageComponent>("Storage");
        if (storage) {
            // Use storage
        }
        return ComponentStatus::Success;
    }
};
```

### 3. ISR Best Practices (ESP32 Specific)
**Problem:** ESP32 linker cannot place C++ class static members in IRAM

```cpp
// ❌ DON'T: Static ISR in class (causes linker errors)
class MyComponent {
    static volatile uint64_t count;
    static void IRAM_ATTR isr() {
        count++;  // Error: "dangerous relocation: l32r"
    }
};

// ✅ DO: Global ISR outside class
namespace {
    volatile uint64_t g_count = 0;
    volatile bool g_newEvent = false;
}

void IRAM_ATTR myISR() {
    g_count++;
    g_newEvent = true;
}

class MyComponent : public IComponent {
    void begin() override {
        attachInterrupt(pin, myISR, FALLING);
    }
    
    void loop() override {
        if (g_newEvent) {
            handleEvent();
            g_newEvent = false;
        }
    }
};
```

**Why?** ESP32 toolchain limitation with C++ member functions in IRAM.
**Solution:** ISR must be global function, variables in anonymous namespace.

### 4. Storage Patterns

**Storing Simple Types:**
```cpp
storage->putInt("counter", 42);
storage->putString("name", "Device");
storage->putFloat("temperature", 23.5);
```

**Storing uint64_t (Current Workaround):**
```cpp
uint8_t buffer[8];
memcpy(buffer, &value, 8);
storage->putBlob("key", buffer, 8);

// Reading
if (storage->getBlob("key", buffer, 8) == 8) {
    memcpy(&value, buffer, 8);
}
```

**Storing Structs:**
```cpp
struct Config {
    int value1;
    float value2;
};
Config cfg = {42, 3.14};
storage->putBlob("config", (uint8_t*)&cfg, sizeof(Config));
```

### 5. Event Bus Communication
```cpp
// Emit data to other components
struct SensorData {
    float temperature;
    int humidity;
};
SensorData data = {23.5, 65};
emit("sensor.data", data, false);  // Non-sticky

// Subscribe in another component
on<SensorData>("sensor.data", [](const SensorData& d) {
    DLOG_I("MQTT", "Temp: %.1f, Humidity: %d", d.temperature, d.humidity);
});
```

### 6. Non-Blocking Timers
```cpp
class MyComponent : public IComponent {
private:
    Utils::NonBlockingDelay timer{5000};  // 5 seconds
    
    void loop() override {
        if (timer.isReady()) {
            doPeriodicTask();
            // timer auto-resets
        }
    }
};
```
```

---

### 5. **Dependency Injection Enhancement** (Priority: **LOW**)

**Current State:**
```cpp
std::vector<String> getDependencies() const override {
    return {"Storage", "NTP"};
}
// But no automatic injection - components must call getComponent() manually
```

**Potential Future Enhancement:**
```cpp
// Option A: Template-based automatic injection
template<typename... Deps>
class ComponentWithDeps : public IComponent {
protected:
    std::tuple<Deps*...> deps;
};

class MyComponent : public ComponentWithDeps<StorageComponent, NTPComponent> {
    void afterBegin() override {
        auto* storage = std::get<0>(deps);  // Automatically injected
        auto* ntp = std::get<1>(deps);
    }
};

// Option B: Callback pattern
class IComponent {
    virtual void onDependenciesReady() {}  // NEW
};

class MyComponent : public IComponent {
    void onDependenciesReady() override {
        // Framework guarantees all declared dependencies are available
        storage_ = core_->getComponent<StorageComponent>("Storage");
        // No null check needed
    }
};
```

**Note:** Lower priority - current manual approach works, but could be improved long-term.

---

### 6. **Storage Namespace Configuration** (Priority: **VERY LOW**)

**Observation:**
Storage always uses namespace `"domotics"` (hardcoded).

**Potential Enhancement:**
```cpp
StorageConfig config;
config.namespace_ = "watermeter";  // Per-project namespace

auto* storage = new StorageComponent(config);
```

**Benefits:**
- Isolation between projects on same device
- Avoid key conflicts
- Easier multi-project development

**Note:** Very low priority - current shared namespace works fine for most cases.

---

## 🐛 Minor Issues Found

### 1. Storage.h - Missing isOpen Check
Some methods check `isOpen` before accessing preferences, others don't:
```cpp
// Has check
String getString(...) {
    if (!isOpen) return defaultValue;  // ✅
}

// Missing check?
size_t getBlob(...) {
    // No isOpen check here
}
```

Should be consistent across all methods.

---

## 💡 Real-World Use Case Details

**Project:** ESP32 Water Meter  
**Description:** IoT water consumption monitoring with magnetic pulse sensor

**Features Implemented:**
- ✅ ISR pulse counting (IRAM pattern documented)
- ✅ NVS persistence (uint64_t via blob storage)
- ✅ NTP-based auto-resets (daily at midnight, yearly Jan 1st)
- ✅ Event bus → MQTT → Home Assistant
- ✅ WebUI with telnet console
- ✅ OTA updates
- ✅ Non-blocking timers (LED flash, auto-save, publish)

**Memory Usage:**
- RAM: 14.8% (48KB / 328KB)
- Flash: 70.8% (1.39MB / 1.97MB)

**Code Size:**
- `main.cpp`: 92 lines (mostly DomoticsCore setup)
- `WaterMeterComponent.h`: 290 lines (business logic)
- Clean, maintainable architecture

**Challenges Solved:**
1. ESP32 ISR IRAM issue → Global function pattern
2. uint64_t storage → Blob workaround
3. Core access → Manual injection pattern
4. Dependencies → Null-checking pattern

**What Worked Excellently:**
- Event bus - very clean and intuitive
- Component lifecycle - well thought out
- System::fullStack() - great for rapid start
- Storage with Preferences - simple and reliable
- Overall architecture - modular and extensible

**Project Available:**
Complete source code available at WaterMeter project for testing/examples if needed.

---

## 📊 Priority Summary

| Enhancement | Priority | Effort | Impact | Breaking? |
|------------|----------|--------|---------|-----------|
| Core Access in IComponent | HIGH | Low | High | No |
| Custom Components Documentation | HIGH | Medium | High | No |
| Storage uint64_t Support | MEDIUM | Low | Medium | No |
| Component Lifecycle (afterBegin) | LOW | Medium | Medium | No |
| Dependency Injection | LOW | High | Low | No |
| Storage Namespace Config | VERY LOW | Low | Low | No |

**Recommended Implementation Order:**
1. Documentation (high impact, non-breaking)
2. Core Access pattern (high impact, low effort)
3. Storage uint64_t (practical need, simple addition)
4. Others as time permits

---

## 🙏 Acknowledgments

Thank you for creating this framework! The modular architecture and component system are excellent. These suggestions come from real production use and aim to make DomoticsCore even better for the community.

The WaterMeter project demonstrates that DomoticsCore v1.0 is production-ready and provides a solid foundation for ESP32 IoT projects. These enhancements would make it even more developer-friendly.

**Contact:** Available for discussion, testing, or providing code examples if helpful.
