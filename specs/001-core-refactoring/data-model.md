# Data Model: DomoticsCore Library Refactoring

**Date**: 2025-12-17
**Feature**: 001-core-refactoring

## Core Entities

### IComponent (Interface)

Base interface for all DomoticsCore components.

| Field/Method | Type | Description |
|--------------|------|-------------|
| `metadata` | ComponentMetadata | Component name, version, author, etc. |
| `begin()` | ComponentStatus | Initialize component, returns status |
| `loop()` | void | Called every main loop iteration |
| `shutdown()` | ComponentStatus | Clean shutdown, returns status |
| `getDependencies()` | vector<Dependency> | Declare required/optional dependencies |
| `afterAllComponentsReady()` | void | Called after all components initialized |
| `getCore()` | Core* | Access to parent Core instance |
| `isActive()` | bool | Whether component is active |

**Lifecycle States**: Uninitialized → Initializing → Active → ShuttingDown → Stopped

### ComponentMetadata

| Field | Type | Description |
|-------|------|-------------|
| `name` | String | Unique component name |
| `version` | String | SemVer version (e.g., "1.2.0") |
| `author` | String | Component author |
| `description` | String | Brief description |
| `category` | String | Category for WebUI grouping |
| `tags` | vector<String> | Searchable tags |

### Dependency

| Field | Type | Description |
|-------|------|-------------|
| `name` | String | Name of required component |
| `required` | bool | true = must exist, false = optional |

### ComponentStatus (Enum)

| Value | Description |
|-------|-------------|
| `Success` | Operation completed successfully |
| `ConfigError` | Configuration validation failed |
| `HardwareError` | Hardware initialization failed |
| `DependencyError` | Required dependency missing |
| `NetworkError` | Network operation failed |
| `StorageError` | Storage operation failed |
| `Unknown` | Unspecified error |

### EventBus

Central pub/sub messaging system.

| Method | Description |
|--------|-------------|
| `subscribe(topic, callback)` | Register for topic events |
| `publish(topic, data)` | Publish event to all subscribers |
| `unsubscribe(topic, callback)` | Remove subscription |
| `publishSticky(topic, data)` | Publish sticky event (new subscribers get it immediately) |

### Lifecycle Events

| Event Constant | Publisher | When |
|----------------|-----------|------|
| `Events::COMPONENT_READY` | Each component | After successful begin() |
| `Events::STORAGE_READY` | StorageComponent | After storage initialized |
| `Events::NETWORK_READY` | WiFiComponent | After network connected |
| `Events::SYSTEM_READY` | Core | After all components ready |
| `Events::COMPONENT_ERROR` | Any component | On unrecoverable error |
| `Events::SHUTDOWN_START` | Core | Before shutdown begins |

### ComponentRegistry

| Method | Description |
|--------|-------------|
| `addComponent(component)` | Register component |
| `getComponent<T>(name)` | Get typed component by name |
| `begin()` | Initialize all in topological order |
| `loop()` | Call loop() on all active components |
| `shutdown()` | Shutdown in reverse order |
| `getComponents()` | Get all registered components |

### Core

Main orchestrator class.

| Method | Description |
|--------|-------------|
| `begin(config)` | Initialize Core and all components |
| `loop()` | Main loop iteration |
| `shutdown()` | Graceful shutdown |
| `addComponent(component)` | Add component to registry |
| `getComponent<T>(name)` | Get component by name |
| `getEventBus()` | Access EventBus instance |

### CoreConfig

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `deviceName` | String | "DomoticsCore" | Device identifier |
| `logLevel` | int | 1 | Debug log verbosity (0-4) |

## Storage Entities

### StorageComponent

| Method | Description |
|--------|-------------|
| `getString(ns, key, default)` | Get string value |
| `putString(ns, key, value)` | Store string value |
| `getInt(ns, key, default)` | Get integer value |
| `putInt(ns, key, value)` | Store integer value |
| `getBool(ns, key, default)` | Get boolean value |
| `putBool(ns, key, value)` | Store boolean value |
| `remove(ns, key)` | Remove key |
| `clear(ns)` | Clear namespace |

**Namespaces**: Each component gets isolated namespace to prevent key conflicts.

## Network Entities

### WiFiComponent

| Event | Description |
|-------|-------------|
| `wifi/connected` | Network connected with IP |
| `wifi/disconnected` | Network lost |
| `wifi/ap_start` | AP mode started |

### MQTTComponent

| Event | Description |
|-------|-------------|
| `mqtt/connected` | Broker connected |
| `mqtt/disconnected` | Broker connection lost |
| `mqtt/message` | Message received |

### NTPComponent

| Event | Description |
|-------|-------------|
| `ntp/synced` | Time synchronized |
| `ntp/failed` | Sync failed |

## Validation Rules

1. **Component names**: Must be unique, alphanumeric + underscore
2. **Namespace keys**: Max 15 characters (ESP32 Preferences limit)
3. **Dependencies**: No circular dependencies allowed
4. **Events**: Use constants from Events.h, no magic strings

## State Transitions

```
Component Lifecycle:
[Uninitialized] --begin()--> [Initializing] --success--> [Active]
                                            --failure--> [Error]
[Active] --shutdown()--> [ShuttingDown] --> [Stopped]
[Active] --error--> [Error] --shutdown()--> [Stopped]
```
