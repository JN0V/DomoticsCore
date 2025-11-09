# DomoticsCore - Future Enhancements Roadmap

This file tracks potential future enhancements for DomoticsCore.

**Current Version:** v1.1.3

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

## Completed Features

See [CHANGELOG.md](CHANGELOG.md) for details on completed features:
- **v1.1.3:** Storage Namespace Configuration (project isolation)
- **v1.1.2:** AP mode bugfix
- **v1.1.1:** Eliminated Storage early-init requirement
- **v1.1.0:** Lifecycle Callback (`afterAllComponentsReady()`) + Optional Dependencies
- **v1.0.2:** Lazy Core Injection
- **v1.0.1:** Core Access in IComponent, Storage uint64_t Support

---

## Contributing

If you have suggestions for future enhancements, please:
1. Open an issue on GitHub with use case description
2. Provide real-world examples if possible
3. Consider priority and backward compatibility

**Note:** This roadmap focuses on enhancements that maintain backward compatibility within the 1.x series.

---

## 📚 Architecture Notes

### Component Initialization

**v1.1.1+ Pattern:** Clean component initialization with `afterAllComponentsReady()` lifecycle hook.

**Example:**
```cpp
// System registers all components normally
core.addComponent(std::make_unique<StorageComponent>());
core.addComponent(std::make_unique<WifiComponent>());

// Core initializes all in dependency order
core.begin();

// Load configuration AFTER all components ready
auto* storage = core.getComponent<StorageComponent>("Storage");
auto* wifi = core.getComponent<WifiComponent>("Wifi");
wifi->setCredentials(storage->getString("ssid", ""), storage->getString("password", ""));
```

**LED Early-Init Exception:** LED component is early-initialized in System to display boot errors visually. This is the only remaining early-init pattern and is justified by its error-reporting purpose.

See [CHANGELOG.md](CHANGELOG.md) for complete v1.1.x evolution history.


---

## 🐛 Storage Dependency Declaration Issue (v1.1.3)

**Reporter:** WaterMeter Project  
**Date:** November 7, 2025  
**Severity:** MEDIUM - Workaround available but pattern broken

### Bug: Storage Cannot Be Declared in getDependencies()

**Problem:**
Components cannot declare Storage as a dependency because Storage's name is not set until `begin()` is called, which happens AFTER dependency resolution.

**Root Cause:**
```cpp
// DomoticsCore-Storage/include/DomoticsCore/Storage.h

StorageComponent(const StorageConfig& config = StorageConfig()) {
    // ❌ metadata.name is NOT set here
}

ComponentStatus begin() override {
    // ⚠️ Name set too late - after resolveDependencies()
    metadata.name = "Storage";
}
```

**Sequence:**
1. System.h:326 - Storage added to registry (name = "")
2. ComponentRegistry::resolveDependencies() - Builds map by getName()
   - Storage->getName() returns "" (empty)
   - Storage NOT in map with key "Storage"
3. ComponentRegistry::initializeAll() - Calls Storage::begin()
   - metadata.name = "Storage" set NOW
   - Too late - dependencies already resolved

**Impact:**
```cpp
// ❌ Does NOT work
std::vector<Dependency> getDependencies() const override {
    return {
        {"Storage", false}  // Not found - name empty during resolution
    };
}

// ✅ Workaround required
auto* storage = getCore()->getComponent<Components::StorageComponent>("");  // Empty name
```

**Proposed Fix:**
```cpp
StorageComponent(const StorageConfig& config = StorageConfig()) 
    : storageConfig(config), ... {
    // ✅ Initialize name immediately
    metadata.name = "Storage";
    metadata.version = "1.0.1";
    metadata.author = "DomoticsCore";
    metadata.description = "Key-value storage component";
    metadata.category = "Storage";
}

ComponentStatus begin() override {
    // Name already set, no change needed here
    DLOG_I(LOG_STORAGE, "Initializing...");
    // ...
}
```

**Benefits:**
- ✅ Storage findable in dependency resolution
- ✅ Can be declared in getDependencies()
- ✅ Correct init order via topological sort
- ✅ No workaround needed
- ✅ Consistent with other components (LED, WiFi, etc.)

**Current Workaround:**
See WaterMeter project `docs/STORAGE_DEPENDENCY_ISSUE.md` for detailed workaround pattern.

**Status:** Open - Awaiting fix in v1.1.4+

