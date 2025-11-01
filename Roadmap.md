# DomoticsCore - Future Enhancements Roadmap

This file tracks potential future enhancements for DomoticsCore.

**Current Version:** v1.1.0

---

## Potential Future Enhancements

### Component Lifecycle Enhancement
**Priority:** LOW  
**Effort:** Medium  
**Status:** Deferred

**Proposal:** Add `afterBegin()` callback for dependency-safe initialization.

**Current Pattern (works well):**
```cpp
ComponentStatus begin() override {
    // Check for dependencies
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    if (!storage) {
        DLOG_W("MyComponent", "Storage not available");
        return ComponentStatus::DependencyError;
    }
    // Use storage...
}
```

**Proposed Pattern:**
```cpp
ComponentStatus begin() override {
    // Internal initialization only
    pinMode(GPIO_PIN, INPUT);
    return ComponentStatus::Success;
}

void afterBegin() override {
    // Dependencies guaranteed available here
    auto* storage = getCore()->getComponent<StorageComponent>("Storage");
    loadFromStorage();  // No null checks needed
}
```

**Benefits:**
- Clear separation: `begin()` = internal init, `afterBegin()` = dependency setup
- No defensive null checks everywhere
- Framework guarantees all dependencies are ready

**Implementation:**
- ComponentRegistry calls all `begin()` first
- Then calls all `afterBegin()` in topological order
- Non-breaking (virtual with default empty implementation)

**Decision:** Current pattern with `getCore()` and null checks works well. Defer until there's a strong use case.

---

### Dependency Injection Enhancement
**Priority:** LOW  
**Effort:** High  
**Status:** Not planned

**Proposal:** Template-based automatic dependency injection or callback patterns.

**Option A: Template-based:**
```cpp
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
```

**Option B: Callback pattern:**
```cpp
class IComponent {
    virtual void onDependenciesReady() {}  // NEW
};

class MyComponent : public IComponent {
    void onDependenciesReady() override {
        // Framework guarantees all declared dependencies are available
        storage_ = getCore()->getComponent<StorageComponent>("Storage");
        // No null check needed
    }
};
```

**Decision:** Manual approach with `getCore()` is clean and explicit. Advanced DI patterns add complexity without clear benefits for current use cases.

---

### Storage Namespace Configuration
**Priority:** VERY LOW  
**Effort:** Low  
**Status:** Not planned

**Proposal:** Configurable namespace per component instead of shared "domotics" namespace.

**Current:**
```cpp
// All components share "domotics" namespace
StorageComponent storage;  // Uses "domotics" namespace
```

**Proposed:**
```cpp
StorageConfig config;
config.namespace_ = "watermeter";  // Per-project namespace

auto* storage = new StorageComponent(config);
```

**Benefits:**
- Isolation between projects on same device
- Avoid key conflicts
- Easier multi-project development

**Decision:** Shared namespace works fine for current use cases. Can be added if needed without breaking changes.

---

## Completed Features

See [CHANGELOG.md](CHANGELOG.md) for details on completed features:
- v1.1.0: Lifecycle Callback (`afterAllComponentsReady()`) + Optional Dependencies
- v1.0.2: Lazy Core Injection
- v1.0.1: Core Access in IComponent, Storage uint64_t Support

---

## Contributing

If you have suggestions for future enhancements, please:
1. Open an issue on GitHub with use case description
2. Provide real-world examples if possible
3. Consider priority and backward compatibility

**Note:** This roadmap focuses on enhancements that maintain backward compatibility within the 1.x series.

---

## 📚 Technical Notes

### Why Early-Init is Necessary in System

**Background:** System::begin() uses "early initialization" for LED and Storage components.

**Reasons:**

1. **LED Component** - Must be initialized BEFORE other components to show boot errors visually
   ```cpp
   // LED initialized early so it can show errors during boot sequence
   if (led->begin() == ComponentStatus::Success) {
       led->setActive(true);  // Skip double-init in core.begin()
   }
   ```

2. **Storage Component** - Must load WiFi credentials BEFORE creating WifiComponent
   ```cpp
   // Storage initialized early to load credentials
   if (storage->begin() == ComponentStatus::Success) {
       storage->setActive(true);
       // Load WiFi credentials from storage
       config.wifiSSID = storage->getString("wifi_ssid", "");
       config.wifiPassword = storage->getString("wifi_password", "");
   }
   // NOW create WifiComponent with loaded credentials
   auto wifi = new WifiComponent(config.wifiSSID, config.wifiPassword);
   ```

**Why This Works:**
- ComponentRegistry detects already-initialized components (line 106-108)
- Skips `begin()` call if component already active
- Dependency resolution works correctly (early-init components are in registry)

**✅ ELIMINATED Since v1.1.1:**
WifiComponent refactored to eliminate Storage early-init:
- Constructor accepts empty credentials (defaults)
- Added `setCredentials()` method for late configuration
- Added `afterAllComponentsReady()` to connect if credentials set late
- System loads credentials from Storage AFTER `core.begin()`

**Result:** Only LED early-init remains (justified for boot error visualization). Storage early-init eliminated!


---

## 🐛 v1.1.1 Bug Report: setCredentials() Ambiguity

**Reporter:** WaterMeter Project  
**Date:** October 31, 2025  
**Severity:** CRITICAL - Prevents compilation

### Bug: Overloaded setCredentials() Ambiguity

**File:** `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h`

**Problem:**
```cpp
// Line ~74
void setCredentials(const String& newSsid, const String& newPassword = "") { ... }

// Line ~290
void setCredentials(const String& newSsid, const String& newPassword, bool reconnectNow = true) { ... }

// In System.h:531
wifi->setCredentials(savedSSID, savedPassword);  // ❌ Ambiguous\!
```

**Error:**
```
error: call of overloaded 'setCredentials(String&, String&)' is ambiguous
```

**Root Cause:**
Both methods match the 2-argument call. Compiler can't decide.

**Quick Fix:**
Remove the second overload's default parameter OR rename one method:

**Option A (Recommended):**
```cpp
// Keep first
void setCredentials(const String& newSsid, const String& newPassword = "")

// Remove second OR change to:
void setCredentialsAndReconnect(const String& newSsid, const String& newPassword)
```

**Option B:**
```cpp
// Make reconnect non-optional in second
void setCredentials(const String& newSsid, const String& newPassword)  // No defaults
void setCredentials(const String& newSsid, const String& newPassword, bool reconnectNow)  // No default
```

**Impact:**
- ❌ v1.1.1 does not compile
- ❌ Blocks all users from upgrading

**Status:** Awaiting v1.1.2


---

## 🐛 v1.1.1 Bug Report #2: Storage Still Not in Dependency Graph

**Reporter:** WaterMeter Project  
**Date:** October 31, 2025  
**Severity:** HIGH - Optional dependencies don't work

### Bug: Storage Initialized AFTER Components That Depend On It

**Problem:**
Storage is no longer "early-init" BUT it's still initialized LAST, after all other components.

**Evidence from logs:**
```
[583]  resolveDependencies(): Component 'WaterMeter' optional dependency 'Storage' not available (OK)
[1156] Initializing component: WaterMeter
[1292] Component initialized: WaterMeter
[1357] Initializing component: Storage  ← Storage AFTER WaterMeter\!
[1385] Component initialized: Storage
[1394] All components initialized
[1425] afterAllComponentsReady(): Storage not available  ← Called but Storage exists\!
```

**Root Cause:**
When `resolveDependencies()` runs, Storage is NOT in the dependency graph yet, so:
1. WaterMeter declares `{"Storage", false}` as optional dependency
2. Storage not found in componentMap → marked "not available (OK)"
3. Topological sort puts WaterMeter before Storage
4. WaterMeter.begin() called → WaterMeter.afterAllComponentsReady() called
5. Storage.begin() called AFTER
6. Storage exists but WaterMeter can't access it

**Expected Behavior:**
Components with optional dependency on Storage should initialize AFTER Storage.

**Current Workaround:**
None - optional dependencies on Storage don't work at all.

**Proper Fix:**
Storage must be added to componentMap BEFORE resolveDependencies() runs, or:
- Use a two-phase init: built-ins first, then custom components
- Or: call afterAllComponentsReady() in a second pass after ALL begin()

**Impact:**
- ❌ Optional dependencies feature completely broken for Storage
- ❌ Must use defensive checks in begin() (old v1.0.2 pattern)
- ❌ afterAllComponentsReady() callback useless for Storage access

**Logs show:** `getComponent<StorageComponent>("Storage")` returns nullptr even though Storage is initialized.

**Status:** Critical - makes v1.1.1 features non-functional


---

## 🐛 v1.1.1 Bug Report #3: Storage Component Has Empty Name

**Reporter:** WaterMeter Project  
**Date:** October 31, 2025  
**Severity:** HIGH - Breaks dependency system

### Bug: Storage Component getName() Returns Empty String

**Evidence from logs:**
```
[185] Registered component:  v1.0.0  ← Empty name\!
[193] Storage component registered
```

**Problem:**
- Storage component registered with empty name: `""`
- Cannot declare Storage in `getDependencies()` - framework can't find it
- Must use `getComponent<StorageComponent>()` without name parameter

**Code Impact:**
```cpp
// ❌ Doesn't work - Storage has empty name
auto* storage = getCore()->getComponent<StorageComponent>("Storage");  

// ✅ Workaround - get by type only
auto* storage = getCore()->getComponent<StorageComponent>();
```

**Expected:**
```cpp
// Storage.h should have:
String getName() const override {
    return "Storage";
}
```

**Impact:**
- ❌ Cannot declare Storage as dependency (optional or required)
- ❌ Breaks dependency resolution system
- ❌ Users must remember to use type-only lookup

**Related to:** Bug #2 (dependency graph issue)

**Fix:** Add proper `getName()` implementation returning `"Storage"` in StorageComponent.

**Status:** Workaround applied in user code

