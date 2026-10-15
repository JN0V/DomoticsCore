<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h]
checked: 6687dc4
verified: agent:claude-code
-->
# EventBus Architecture - DomoticsCore

**Version**: 2.0
**Date**: 2026-03-10
**Status**: Production

---

## 📋 Pages

1. [Overview](#overview) — below
2. [EventBus Decoupling](eventbus-decoupling.md) — direct callbacks against events
3. [Event Flow Diagram](eventbus-event-flows.md) — boot sequence, command handling
4. [Component Events](eventbus-component-events.md) — what MQTT, HomeAssistant, WiFi and System emit and listen to
5. [Implementation Details and Best Practices](eventbus-implementation.md) — listeners first, the PubSubClient callback bridge
6. [Troubleshooting and Migration](eventbus-troubleshooting.md) — events not received, moving off direct callbacks
7. [Summary](#summary) — below

---

## Overview

DomoticsCore uses an **EventBus architecture** for inter-component communication. Components communicate by emitting and listening to typed events, eliminating direct dependencies and improving modularity.

### Key Benefits

- ✅ **Loose Coupling**: Components don't need direct references to each other
- ✅ **Testability**: Components can be tested in isolation
- ✅ **Flexibility**: New components can tap into existing events without modifying core
- ✅ **Maintainability**: Clear separation of concerns

### Architecture Principle

**Components should register EventBus listeners BEFORE checking runtime configuration.**

This ensures that listeners are always available, even if a component starts without configuration (e.g., MQTT without broker settings).

---

## Summary

The EventBus architecture provides a robust, maintainable foundation for DomoticsCore's component system. Key takeaways:

- ✅ Register EventBus listeners BEFORE configuration checks
- ✅ Use typed events for type safety
- ✅ Emit events instead of calling methods directly
- ✅ Handle missing components gracefully
- ✅ PubSubClient callback bridge is necessary and correct

This architecture enables components to be developed, tested, and deployed independently while maintaining a clear communication contract through well-defined events.
