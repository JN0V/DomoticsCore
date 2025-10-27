# WebUI State Tracking - Quick Reference

> **📘 For complete WebUI development guide, see [WebUI-Developer-Guide.md](WebUI-Developer-Guide.md)**

## Overview

This document is a quick reference for the `LazyState<T>` helper class used in WebUI providers to optimize WebSocket updates through efficient state change detection.

---

## The Problem

WebUI providers may be created before components are initialized, leading to two challenges:

### **Challenge 1: Initialization Order**
```cpp
// Provider created BEFORE component initialization
core.addComponent(std::make_unique<WifiComponent>());
auto provider = new WifiWebUI(wifi);  // ← WiFi not initialized yet!
core.begin();  // ← WiFi initialized HERE
```

**Issue:** Constructor can't read component state because it's not initialized yet.

### **Challenge 2: False Positives**
```cpp
// Without proper initialization
bool hasDataChanged(const String& contextId) {
    bool current = component->getValue();
    bool changed = (current != lastValue);  // ← First call: false != true = CHANGE!
    lastValue = current;
    return changed;
}
```

**Issue:** First call always returns `true` even if state hasn't changed.

---

## The Solution: `LazyState<T>`

The `LazyState<T>` template class handles both challenges automatically:

```cpp
#include <DomoticsCore/IWebUIProvider.h>

template<typename T>
struct LazyState {
    bool hasChanged(const T& current);     // Main method: check if changed
    const T& getValue() const;             // Get stored value
    bool isInitialized() const;            // Check if initialized
    void reset();                          // Reset to uninitialized
};
```

---

## Usage Patterns

### **Pattern 1: Simple State (bool, int, String)**

```cpp
class MyWebUI : public IWebUIProvider {
private:
    LazyState<bool> connectedState;
    LazyState<String> ipAddressState;
    
public:
    bool hasDataChanged(const String& contextId) override {
        if (contextId == "status") {
            return connectedState.hasChanged(component->isConnected());
        }
        else if (contextId == "network") {
            return ipAddressState.hasChanged(component->getIP());
        }
        return false;
    }
};
```

**Benefits:**
- ✅ No manual initialization needed
- ✅ No `initialized` flag tracking
- ✅ Returns `false` on first call (no false positive)
- ✅ Works regardless of initialization order

---

### **Pattern 2: Composite State (Multiple Values)**

For contexts that track multiple related values:

```cpp
class WifiWebUI : public IWebUIProvider {
private:
    struct ComponentState {
        bool connected;
        String ssid;
        String ip;
        
        // Required: equality operators
        bool operator==(const ComponentState& other) const {
            return connected == other.connected && 
                   ssid == other.ssid && 
                   ip == other.ip;
        }
        bool operator!=(const ComponentState& other) const {
            return !(*this == other);
        }
    };
    
    LazyState<ComponentState> wifiState;
    
public:
    bool hasDataChanged(const String& contextId) override {
        if (contextId == "wifi_component") {
            ComponentState current = {
                wifi->isConnected(),
                wifi->getSSID(),
                wifi->getLocalIP()
            };
            return wifiState.hasChanged(current);
        }
        return false;
    }
};
```

**Requirements:**
- ✅ Define `operator==` and `operator!=` for your state struct
- ✅ All members must be copyable

---

### **Pattern 3: Testing and Validation**

Use `isInitialized()` to force initialization in test methods:

```cpp
bool validateStateSync() {
    // Force initialization if not already done
    if (!connectedState.isInitialized()) {
        connectedState.hasChanged(component->isConnected());
    }
    
    // Now validate
    if (component->isConnected() != connectedState.getValue()) {
        DLOG_W(LOG_TAG, "State mismatch!");
        return false;
    }
    
    return true;
}
```

---

## Quick Example

```cpp
class MyWebUI : public IWebUIProvider {
private:
    MyComponent* component;
    
    // Simple states
    LazyState<bool> enabledState;
    LazyState<int> valueState;
    
    // Composite state
    struct Status {
        String mode;
        int level;
        bool operator==(const Status& o) const { return mode == o.mode && level == o.level; }
        bool operator!=(const Status& o) const { return !(*this == o); }
    };
    LazyState<Status> statusState;
    
public:
    explicit MyWebUI(MyComponent* comp) : component(comp) {}
    
    bool hasDataChanged(const String& contextId) override {
        if (!component) return false;
        
        if (contextId == "simple") {
            return enabledState.hasChanged(component->isEnabled());
        }
        else if (contextId == "composite") {
            Status current = {component->getMode(), component->getLevel()};
            return statusState.hasChanged(current);
        }
        
        return false;
    }
};
```

> **💡 For complete examples with all IWebUIProvider methods, see [WebUI-Developer-Guide.md](WebUI-Developer-Guide.md)**

---

## Migration Guide

### **Before (Manual State Tracking)**

```cpp
struct MyState {
    bool value = false;
    bool initialized = false;
} myState;

bool hasDataChanged(const String& contextId) {
    bool current = component->getValue();
    if (!myState.initialized) {
        myState.value = current;
        myState.initialized = true;
        return false;
    }
    bool changed = (current != myState.value);
    myState.value = current;
    return changed;
}
```

### **After (LazyState)**

```cpp
LazyState<bool> myState;

bool hasDataChanged(const String& contextId) {
    return myState.hasChanged(component->getValue());
}
```

**Result:** 10+ lines → 1 line! 🎯

---

## Best Practices

### ✅ **DO:**
- Use `LazyState<T>` for all state tracking in WebUI providers
- Define equality operators for composite state structs
- Use `isInitialized()` in test/validation methods
- Choose appropriate types (`bool`, `int`, `String`, custom structs)

### ❌ **DON'T:**
- Don't initialize state in the constructor (LazyState handles it)
- Don't track `initialized` flags manually
- Don't call `getValue()` before checking `isInitialized()` in validation code
- Don't use pointers/references as state type (use values)

---

## Benefits Summary

✅ **Timing-independent** - Works regardless of initialization order  
✅ **No false positives** - First call returns `false`  
✅ **Type-safe** - Compile-time type checking  
✅ **Minimal boilerplate** - One-line change detection  
✅ **Testable** - Easy to validate state synchronization  
✅ **Consistent** - Same pattern across all WebUI providers  
✅ **Self-documenting** - Clear intent with `hasChanged()`  

---

## LazyState<T> API Reference

```cpp
template<typename T>
struct LazyState {
    // Check if value changed, update internal state
    bool hasChanged(const T& current);
    
    // Get current stored value (undefined if not initialized)
    const T& getValue() const;
    
    // Check if state has been initialized
    bool isInitialized() const;
    
    // Initialize/get value with lazy loading
    T& get(std::function<T()> initializer);
    
    // Reset to uninitialized state
    void reset();
};
```

**Type Requirements:**
Any type `T` must support:
- Copy construction: `T(const T&)`
- Assignment: `T& operator=(const T&)`
- Equality comparison: `bool operator==(const T&)` and `bool operator!=(const T&)`

---

## See Also

- **[WebUI-Developer-Guide.md](WebUI-Developer-Guide.md)** - Complete WebUI development guide with examples
- **IWebUIProvider.h** - Base interface with `LazyState` definition
- **WifiWebUI.h** - Reference implementation using LazyState
- **NTPWebUI.h** - Time-based data example

---

**Version:** DomoticsCore v2.0+  
**Last Updated:** 2025-10-05
