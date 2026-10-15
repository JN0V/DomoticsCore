<!-- workline
sources: [DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h, DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h, DomoticsCore-MQTT/include/DomoticsCore/MQTT.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h]
checked: 6687dc4
verified: agent:claude-code
-->
# EventBus Troubleshooting and Migration

Part of the [EventBus Architecture](eventbus-architecture.md) reference.

## Troubleshooting

### Problem: Events Not Received

**Symptom**: Component emits event but no handler is called

**Possible Causes:**
1. ✅ **Listener registered after early return** (most common)
   - Solution: Move listener registration before config checks
   
2. ✅ **Wrong event type**
   - Check that emit type matches listener type exactly
   
3. ✅ **Wrong event name**
   - Event names are case-sensitive strings

**Debug:**
```cpp
// Add logging to verify registration
on<MyEvent>("my/event", [](const MyEvent& ev) {
    DLOG_I(LOG, "Event received!");
});
DLOG_I(LOG, "Listener registered for: my/event");
```

### Problem: Component State Not Updated

**Symptom**: Command received but state not published back

**Possible Causes:**
1. ✅ **Auto-publish disabled** (`optimistic = true` for switches)
2. ✅ **MQTT not connected** when `publishState()` called
3. ✅ **EventBus event not emitted**

**Debug:**
```cpp
void publishState(const String& id, const String& state) {
    DLOG_I(LOG_HA, "Publishing state: %s = %s", id.c_str(), state.c_str());
    mqttPublish(topic, state, 0, entity->retained);
}
```

### Problem: Need to Reboot for Config Changes

**Symptom**: Component configured via WebUI but doesn't work until reboot

**Root Cause**: EventBus listeners registered after config check

**Solution**: Move listener registration to beginning of `begin()` method

---

## Migration from Direct Callbacks

### Old Code (Direct Callbacks)

```cpp
mqtt->onConnect([this]() {
    publishDiscovery();
});

mqtt->onMessage([this](String topic, String payload) {
    handleCommand(topic, payload);
});

mqtt->publish(topic, payload);
mqtt->subscribe(topic);
```

### New Code (EventBus)

```cpp
on<bool>("mqtt/connected", [this](const bool&) {
    publishDiscovery();
});

on<MQTTMessageEvent>("mqtt/message", [this](const MQTTMessageEvent& ev) {
    handleCommand(String(ev.topic), String(ev.payload));
});

emit("mqtt/publish", MQTTPublishEvent{topic, payload, qos, retain});
emit("mqtt/subscribe", MQTTSubscribeEvent{topic, qos});
```

### Key Changes

1. **No direct component references**
   - Before: `mqtt->onConnect(...)`
   - After: `on<bool>("mqtt/connected", ...)`

2. **Structured event data**
   - Before: Function parameters
   - After: Event structs with fixed-size buffers

3. **Fire-and-forget emission**
   - Before: Direct method call with return value
   - After: `emit()` returns void, listeners handle asynchronously

