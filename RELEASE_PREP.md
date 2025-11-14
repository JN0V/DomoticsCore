# Release Preparation: v1.2.1 - EventBus Migration

**Date**: 2025-11-14  
**Version**: v1.2.1  
**Type**: Patch (breaking API changes with EventBus migration)

---

## 🎯 What Changed

### Core Changes
1. **EventBus Architecture**: Complete decoupling of MQTT and HomeAssistant components
2. **Callback Removal**: Direct callbacks (onConnect, onDisconnect, onMessage) replaced with EventBus events
3. **Dependency Decoupling**: HomeAssistant no longer requires direct MQTT component reference

### Bug Fixes (CRITICAL)
1. **EventBus Listener Registration**: Moved BEFORE configuration checks (Bug #1)
2. **PubSubClient Callback Registration**: Moved BEFORE configuration checks (Bug #2)
3. **Impact**: Discovery, subscriptions, and commands now work immediately after WebUI configuration (no reboot needed)

---

## 📦 Components to Update

### Version Bumps (1.0.x → 1.1.0)

| Component | Current | New | Reason |
|-----------|---------|-----|--------|
| **DomoticsCore-MQTT** | 1.0.1 | **1.1.0** | EventBus events, callback registration fix |
| **DomoticsCore-HomeAssistant** | 1.0.2 | **1.1.0** | Removed MQTT dependency, EventBus only |
| **DomoticsCore-System** | 1.0.2 | **1.1.0** | Updated HA component initialization |
| **DomoticsCore-Core** | 1.0.0 | 1.0.0 | No changes |

---

## 🔧 Migration Guide

### For MQTT Component Users

**Before (Old API):**
```cpp
auto mqtt = std::make_unique<MQTTComponent>(config);
auto* mqttPtr = mqtt.get();
core.addComponent(std::move(mqtt));

mqttPtr->onConnect([mqttPtr]() {
    mqttPtr->publish("topic", "payload");
    mqttPtr->subscribe("command/#");
});

mqttPtr->onDisconnect([]() { 
    // Handle disconnect
});

mqttPtr->onMessage("topic", [](String topic, String payload) {
    // Handle message
});
```

**After (EventBus API):**
```cpp
core.addComponent(std::make_unique<MQTTComponent>(config));

// Listen to mqtt/connected event
core.on<bool>("mqtt/connected", [](const bool&) {
    // Publish via EventBus
    MQTTPublishEvent ev{};
    strncpy(ev.topic, "topic", sizeof(ev.topic) - 1);
    strncpy(ev.payload, "payload", sizeof(ev.payload) - 1);
    ev.qos = 0;
    ev.retain = false;
    core.emit("mqtt/publish", ev);
    
    // Subscribe via EventBus
    MQTTSubscribeEvent subEv{};
    strncpy(subEv.topic, "command/#", sizeof(subEv.topic) - 1);
    subEv.qos = 0;
    core.emit("mqtt/subscribe", subEv);
});

// Listen to mqtt/disconnected event
core.on<bool>("mqtt/disconnected", [](const bool&) {
    // Handle disconnect
});

// Listen to mqtt/message event
core.on<MQTTMessageEvent>("mqtt/message", [](const MQTTMessageEvent& ev) {
    String topic = String(ev.topic);
    String payload = String(ev.payload);
    // Handle message
});
```

### For HomeAssistant Component Users

**Before (Old API):**
```cpp
auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
auto* mqttPtr = mqtt.get();
core.addComponent(std::move(mqtt));

auto ha = std::make_unique<HomeAssistantComponent>(mqttPtr, haConfig);
```

**After (EventBus API):**
```cpp
core.addComponent(std::make_unique<MQTTComponent>(mqttConfig));

// No MQTT reference needed!
auto ha = std::make_unique<HomeAssistantComponent>(haConfig);
```

---

## 📄 Documentation Updates Needed

### 1. Component READMEs
- [ ] **DomoticsCore-MQTT/README.md**: Update with EventBus API examples
- [ ] **DomoticsCore-HomeAssistant/README.md**: Update constructor signature
- [ ] **DomoticsCore-System/README.md**: Update component initialization example

### 2. Architecture Documentation
- [x] **docs/EVENTBUS_ARCHITECTURE.md**: Complete reference (already created)
- [ ] **README.md** (root): Update overview with EventBus mention
- [ ] **ARCHITECTURE.md**: Update system diagrams

### 3. Migration Guide
- [ ] Create **MIGRATING_TO_V1.1.md** with detailed migration steps

---

## 🧪 Testing Status

### Examples Fixed
- [x] DomoticsCore-HomeAssistant/BasicHA
- [x] DomoticsCore-HomeAssistant/HAWithWebUI
- [x] DomoticsCore-MQTT/BasicMQTT
- [x] DomoticsCore-MQTT/MQTTWithWebUI
- [x] DomoticsCore-MQTT/MQTTWifiWithWebUI

### Test Results
- **Before Fix**: 23/28 passed (5 failed)
- **After Fix**: 28/28 passed ✅
  - Fixed: HomeAssistant/BasicHA, HomeAssistant/HAWithWebUI (removed mqttPtr param)
  - Fixed: MQTT/BasicMQTT, MQTT/MQTTWithWebUI, MQTT/MQTTWifiWithWebUI (use Core EventBus)
  - Fixed: test_examples_fast.sh (BasicWebUI → HeadlessAPI + WebUIOnly)

---

## 📋 Commit Checklist

- [x] All examples compile successfully (19/19 ✅)
- [x] Version numbers bumped in library.json files
  - [x] DomoticsCore-MQTT: 1.0.1 → 1.2.0
  - [x] DomoticsCore-HomeAssistant: 1.0.2 → 1.2.0
  - [x] DomoticsCore-System: 1.0.2 → 1.2.0
  - [x] DomoticsCore-Core: 1.1.4 → 1.2.0 (added on<>/emit() helpers)
- [x] READMEs updated (main README.md with EventBus mention)
- [x] Migration guide created (docs/migration/v1.2.1.md)
- [x] Documentation reorganized (guides/, migration/, reference/ folders)
- [x] CHANGELOG.md updated with breaking changes
- [x] Removed obsolete files (test_examples_fast.sh, Roadmap.md)
- [ ] Git commit with detailed message (ready)
- [ ] Git tag with v1.2.1 (after commit)
- [ ] GitHub release notes prepared (use release notes below)

---

## 🚀 Release Notes Draft

### v1.2.1 - EventBus Architecture Migration

**Breaking Changes:**
- MQTT component now uses EventBus for all communication
- Direct callbacks (`onConnect`, `onDisconnect`, `onMessage`) removed
- HomeAssistant component no longer requires MQTT component reference

**Bug Fixes:**
- Fixed EventBus listener registration timing (listeners now registered before config checks)
- Fixed PubSubClient callback registration timing  
- Discovery and subscriptions now work immediately after WebUI configuration
- No reboot required after MQTT broker configuration

**Migration:**
See `MIGRATING_TO_V1.1.md` for detailed migration guide.

**Documentation:**
- Complete EventBus architecture reference in `docs/EVENTBUS_ARCHITECTURE.md`
- Event flow diagrams
- Best practices and troubleshooting guide

---

## 📊 Files Changed Summary

```
Modified Components:
  DomoticsCore-MQTT/include/DomoticsCore/MQTT.h
  DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h  
  DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h
  DomoticsCore-System/include/DomoticsCore/System.h

Fixed Examples:
  DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp
  DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp
  DomoticsCore-MQTT/examples/BasicMQTT/src/main.cpp
  DomoticsCore-MQTT/examples/MQTTWithWebUI/src/main.cpp
  DomoticsCore-MQTT/examples/MQTTWifiWithWebUI/src/main.cpp

Documentation:
  docs/EVENTBUS_ARCHITECTURE.md (new)
  MIGRATING_TO_V1.1.md (pending)
  CHANGELOG.md (pending update)
  
Version Files:
  DomoticsCore-MQTT/library.json (1.0.1 → 1.1.0)
  DomoticsCore-HomeAssistant/library.json (1.0.2 → 1.1.0)
  DomoticsCore-System/library.json (1.0.2 → 1.1.0)
```
