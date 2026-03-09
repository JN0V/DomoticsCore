# DomoticsCore-Core -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

---

## Table of Contents

1. [Core](#1-core)
2. [IComponent](#2-icomponent)
3. [ComponentRegistry](#3-componentregistry)
4. [ComponentConfig and Metadata](#4-componentconfig-and-metadata)
5. [EventBus](#5-eventbus)
6. [Logger](#6-logger)
7. [Timer (NonBlockingDelay)](#7-timer-nonblockingdelay)
8. [MemoryManager](#8-memorymanager)
9. [HeapTracker (Testing)](#9-heaptracker-testing)
10. [Platform HAL](#10-platform-hal)
11. [Lifecycle States and Transitions](#11-lifecycle-states-and-transitions)
12. [Dependency Resolution Algorithm](#12-dependency-resolution-algorithm)
13. [Event System Details](#13-event-system-details)
14. [Configuration Patterns](#14-configuration-patterns)

---

## 1. Core

**Header:** `DomoticsCore/Core.h`
**Source:** `DomoticsCore-Core/src/Core.cpp`
**Namespace:** `DomoticsCore`

The `Core` class is the central runtime. It owns a `ComponentRegistry`, drives the lifecycle of all registered components, and provides convenience accessors for the EventBus.

### CoreConfig

```cpp
struct CoreConfig {
    String deviceName = "DomoticsCore";
    String deviceId   = "";        // Auto-generated from chip ID if empty
    uint8_t logLevel  = 3;         // INFO level
};
```

### Core Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **Constructor** | `Core()` | Creates an uninitialized Core runtime. |
| **Destructor** | `~Core()` | Calls `shutdown()` if still initialized. |
| **begin** | `bool begin(const CoreConfig& cfg = CoreConfig())` | Initializes logging, detects memory profile, injects Core reference into the registry, and calls `ComponentRegistry::initializeAll()`. Returns `false` if any component fails. |
| **loop** | `void loop()` | Calls `ComponentRegistry::loopAll()` which iterates active components and dispatches queued EventBus events. Emits a debug heartbeat every 60 seconds. |
| **shutdown** | `void shutdown()` | Calls `ComponentRegistry::shutdownAll()` (reverse order). |
| **getConfiguration** | `CoreConfig getConfiguration() const` | Returns current config. |
| **setConfiguration** | `void setConfiguration(const CoreConfig& cfg)` | Replaces config (before `begin()`). |
| **getDeviceId** | `String getDeviceId() const` | Returns the device ID (auto-generated if not provided). |
| **getDeviceName** | `String getDeviceName() const` | Returns the device name. |
| **addComponent** | `template<T> bool addComponent(std::unique_ptr<T> component)` | Registers a component. `T` must derive from `IComponent`. Static-asserted at compile time. Returns `false` on duplicate name. |
| **getComponent** | `IComponent* getComponent(const String& name)` | Returns raw pointer to component by name, or `nullptr`. |
| **getComponent\<T\>** | `template<T> T* getComponent(const String& name)` | Returns component cast to `T*`, or `nullptr`. |
| **getComponentCount** | `size_t getComponentCount() const` | Number of registered components. |
| **removeComponent** | `bool removeComponent(const String& name)` | Shuts down and removes a component at runtime. |
| **createTimer** | `static NonBlockingDelay createTimer(unsigned long intervalMs)` | Convenience factory for timers. |
| **getEventBus** | `EventBus& getEventBus()` | Returns reference to the shared EventBus. |
| **on\<PayloadT\>** | `template<PayloadT> uint32_t on(const String& topic, handler, bool replayLast = false)` | Subscribe to a topic with typed payload. Returns subscription ID. |
| **emit\<PayloadT\>** | `template<PayloadT> void emit(const String& topic, const PayloadT& payload, bool sticky = false)` | Publish an event with payload. If `sticky` is true, the event is stored and replayed to late subscribers. |
| **emit** | `void emit(const String& topic)` | Publish an event without payload. |

### begin() Sequence

1. Guard against double initialization.
2. Store configuration; auto-generate `deviceId` from chip ID if empty.
3. Initialize serial logging via `HAL::initializeLogging()`.
4. Detect memory profile via `MemoryManager::instance().detectProfile()`.
5. Inject `this` into `ComponentRegistry` via `setCore()`.
6. Call `ComponentRegistry::initializeAll()`.
7. Set `initialized = true` on success.

---

## 2. IComponent

**Header:** `DomoticsCore/IComponent.h`
**Source:** `DomoticsCore-Core/src/IComponent.cpp`
**Namespace:** `DomoticsCore::Components`

Abstract base class for all framework components. Provides lifecycle hooks, dependency declaration, EventBus access, and Core injection.

### Dependency Struct

```cpp
struct Dependency {
    const char* name;       // Component name (must be a string literal)
    bool required = true;   // If false, init proceeds even if dependency missing

    Dependency(const char* n);
    Dependency(const char* n, bool req);
};
```

### IComponent Public Members and Methods

| Member / Method | Signature | Description |
|----------------|-----------|-------------|
| **metadata** | `ComponentMetadata metadata` | Public field; set in constructor. |
| **begin** | `virtual ComponentStatus begin() = 0` | Pure virtual. Called during `initializeAll()` after dependencies are resolved. Perform internal initialization only (GPIO, state). |
| **loop** | `virtual void loop() = 0` | Pure virtual. Called every iteration for active components. |
| **shutdown** | `virtual ComponentStatus shutdown() = 0` | Pure virtual. Called during `shutdownAll()` or `removeComponent()`. |
| **getDependencies** | `virtual std::vector<Dependency> getDependencies() const` | Returns dependency list. Default: empty. |
| **isActive** | `virtual bool isActive() const` | Returns `active` flag. |
| **getLastStatus** | `virtual ComponentStatus getLastStatus() const` | Returns last lifecycle status. |
| **getMetadata** | `virtual const ComponentMetadata& getMetadata() const` | Returns metadata reference. |
| **getTypeKey** | `virtual const char* getTypeKey() const` | Optional stable type identifier (e.g., `"system_info"`). Used by WebUI for automatic wrapper attachment. Default: `""`. |
| **getWebUIProvider** | `virtual IWebUIProvider* getWebUIProvider()` | Optional. Returns WebUI provider pointer. Default: `nullptr`. |
| **onComponentsReady** | `virtual void onComponentsReady(const ComponentRegistry&)` | Called after all components are initialized. For cross-component discovery. |
| **afterAllComponentsReady** | `virtual void afterAllComponentsReady()` | Called after `onComponentsReady()`. Use for late initialization that depends on other components. Dependencies declared via `getDependencies()` are guaranteed available here. |
| **getCore** | `Core* getCore() const` | Returns the injected Core pointer. Uses lazy injection from registry on first access. |
| **setActive** | `void setActive(bool state)` | Sets the `active` flag. Called by registry. |
| **eventBus** | `EventBus& eventBus()` | Returns reference to the injected EventBus. |
| **on\<T\>** | `template<T> uint32_t on(const String& topic, handler, bool replayLast = false)` | Subscribe to a topic with typed payload. Owner defaults to `this`. |
| **emit\<T\>** | `template<T> void emit(const String& topic, const T& payload, bool sticky = false)` | Publish (or sticky-publish) an event with typed payload. |

### Protected Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **setStatus** | `void setStatus(ComponentStatus status)` | Update the internal `lastStatus` field. Components can call this from their lifecycle methods to track status transitions. |

### Protected Members

| Member | Type | Description |
|--------|------|-------------|
| `active` | `bool` | Whether the component is running. |
| `lastStatus` | `ComponentStatus` | Last status from a lifecycle operation. |
| `__dc_eventBus` | `EventBus*` | Injected by registry before `begin()`. |
| `__dc_core` | `Core*` | Injected by registry; lazy via `getCore()`. |
| `__dc_registry` | `ComponentRegistry*` | Injected immediately on registration. |

### Lifecycle Order

1. `begin()` -- Internal initialization only (GPIO, timers, state)
2. `onComponentsReady(registry)` -- Cross-component discovery
3. `afterAllComponentsReady()` -- Late initialization; all dependencies are guaranteed available
4. `loop()` -- Normal operation (called repeatedly)
5. `shutdown()` -- Cleanup and resource release

---

## 3. ComponentRegistry

**Header:** `DomoticsCore/ComponentRegistry.h`
**Namespace:** `DomoticsCore::Components`

Owns all registered components (via `std::unique_ptr`), resolves dependencies, and coordinates lifecycle.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **registerComponent** | `bool registerComponent(std::unique_ptr<IComponent> component)` | Register a component. Injects registry pointer immediately. Returns `false` on null or duplicate name. Notifies lifecycle listeners. |
| **initializeAll** | `ComponentStatus initializeAll()` | Resolves dependency order, then calls `begin()` on each component in order. Injects EventBus and Core before each `begin()`. Publishes `component/ready` after each and `system/ready` after all. Calls `onComponentsReady()` and `afterAllComponentsReady()` hooks. |
| **loopAll** | `void loopAll()` | Calls `loop()` on each active component in initialization order, then `eventBus.poll()`. |
| **shutdownAll** | `void shutdownAll()` | Publishes `shutdown/start`, polls EventBus, then shuts down components in **reverse** initialization order. Cleans up EventBus subscriptions per component. |
| **removeComponent** | `bool removeComponent(const String& name)` | Shuts down (if active), notifies listeners, removes from all internal containers. |
| **getAllComponents** | `std::vector<IComponent*> getAllComponents() const` | Returns raw pointers to all registered components. |
| **getComponent** | `IComponent* getComponent(const String& name)` | Lookup by name; returns `nullptr` if not found. |
| **getComponentCount** | `size_t getComponentCount() const` | Number of registered components. |
| **isInitialized** | `bool isInitialized() const` | Whether `initializeAll()` has completed. |
| **getEventBus** | `EventBus& getEventBus()` | Returns reference to the internal EventBus. |
| **setCore** | `void setCore(Core* core)` | Sets Core reference for injection into components. Called by `Core::begin()`. |
| **getCore** | `Core* getCore() const` | Returns stored Core pointer. Used by `IComponent::getCore()` for lazy injection. |
| **addListener** | `void addListener(IComponentLifecycleListener* listener)` | Register a lifecycle observer. |
| **removeListener** | `void removeListener(IComponentLifecycleListener* listener)` | Unregister a lifecycle observer. |

### IComponentLifecycleListener

```cpp
class IComponentLifecycleListener {
public:
    virtual ~IComponentLifecycleListener() = default;
    virtual void onComponentAdded(IComponent* comp) {}
    virtual void onComponentRemoved(IComponent* comp) {}
};
```

### Internal Data Structures

| Field | Type | Purpose |
|-------|------|---------|
| `components` | `std::vector<std::unique_ptr<IComponent>>` | Ownership of all components. |
| `componentMap` | `std::map<String, IComponent*>` | Name-to-pointer lookup. |
| `initializationOrder` | `std::vector<IComponent*>` | Topologically sorted order. |
| `listeners` | `std::vector<IComponentLifecycleListener*>` | Lifecycle observers. |
| `eventBus` | `EventBus` | Shared event bus instance. |
| `core_` | `Core*` | Injected Core reference. |

---

## 4. ComponentConfig and Metadata

**Header:** `DomoticsCore/ComponentConfig.h`
**Namespace:** `DomoticsCore::Components`

### ComponentStatus Enum

```cpp
enum class ComponentStatus {
    Success = 0,
    ConfigError,
    HardwareError,
    DependencyError,
    NetworkError,
    MemoryError,
    TimeoutError,
    InvalidState,
    NotSupported
};
```

Use `statusToString(ComponentStatus)` to convert to a human-readable C string.

### ComponentMetadata

```cpp
struct ComponentMetadata {
    const char* name        = "";
    const char* version     = "1.0.0";
    const char* author      = "";
    const char* description = "";
    const char* category    = "";
    std::vector<String> tags;
};
```

### ConfigType Enum

```cpp
enum class ConfigType {
    String, Integer, Float, Boolean, IPAddress, Port
};
```

### ConfigParam

```cpp
struct ConfigParam {
    String name;
    ConfigType type;
    bool required;
    String defaultValue;
    String description;
    int minValue = INT_MIN;
    int maxValue = INT_MAX;
    size_t maxLength = 0;
    std::vector<String> allowedValues;

    // Fluent interface
    ConfigParam& min(int minVal);
    ConfigParam& max(int maxVal);
    ConfigParam& length(size_t maxLen);
    ConfigParam& options(const std::vector<String>& opts);
};
```

### ComponentConfig Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **defineParameter** | `void defineParameter(const ConfigParam& param)` | Define a configuration parameter. Sets default value if provided. |
| **setValue** | `void setValue(const String& name, const String& value)` | Set a parameter value. |
| **getValue** | `String getValue(const String& name, const String& defaultVal = "") const` | Get raw string value. |
| **getInt** | `int getInt(const String& name, int defaultVal = 0) const` | Get integer value. |
| **getFloat** | `float getFloat(const String& name, float defaultVal = 0.0f) const` | Get float value. |
| **getBool** | `bool getBool(const String& name, bool defaultVal = false) const` | Get boolean value. Accepts `true/false/1/0/yes/no/on/off`. |
| **validate** | `ValidationResult validate() const` | Validate all defined parameters against their constraints. |
| **getParameters** | `const std::vector<ConfigParam>& getParameters() const` | Get all parameter definitions. |
| **hasParameter** | `bool hasParameter(const String& name) const` | Check if a value is set. |

### ValidationResult

```cpp
struct ValidationResult {
    ComponentStatus status;
    String errorMessage;
    String parameterName;

    bool isValid() const;
    String toString() const;
};
```

Validation includes: required-field check, integer range, float format, boolean format, string length and allowed-values, IP address format (4 octets 0-255), and port range (1-65535).

---

## 5. EventBus

**Header:** `DomoticsCore/EventBus.h`
**Namespace:** `DomoticsCore::Utils`

A queued, single-threaded publish/subscribe bus supporting both `EventType`-based and topic-string-based subscriptions.

### Key Types

```cpp
using Handler = std::function<void(const void* payload)>;

struct Subscription {
    uint32_t id;
    void* owner;
    Handler handler;
};

struct QueuedEvent {
    EventType type;
    String topic;
    std::vector<uint8_t> data;  // Byte-copy of payload
};
```

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **subscribe** (EventType) | `uint32_t subscribe(EventType type, Handler handler, void* owner = nullptr)` | Subscribe to an event type. Returns subscription ID. |
| **subscribe** (topic) | `uint32_t subscribe(const String& topic, Handler handler, void* owner = nullptr, bool replayLast = false)` | Subscribe to a topic string. Supports wildcard topics (e.g., `"sensor/*"`). If `replayLast` is true and a sticky event exists, the handler fires immediately. Returns subscription ID. |
| **unsubscribe** | `void unsubscribe(uint32_t id)` | Remove a subscription by ID. |
| **unsubscribeOwner** | `void unsubscribeOwner(void* owner)` | Remove all subscriptions belonging to an owner. Used during component shutdown. |
| **publish** (EventType+payload) | `template<PayloadT> void publish(EventType type, const PayloadT& payload)` | Queue a typed event with payload (byte-copied). |
| **publish** (EventType) | `void publish(EventType type)` | Queue a typed event without payload. |
| **publish** (topic+payload) | `template<PayloadT> void publish(const String& topic, const PayloadT& payload)` | Queue a topic-based event with payload. |
| **publish** (topic) | `void publish(const String& topic)` | Queue a topic-based event without payload. |
| **publishSticky** (topic+payload) | `template<PayloadT> void publishSticky(const String& topic, const PayloadT& payload)` | Store the last payload for the topic and publish. Late subscribers with `replayLast=true` receive the stored value. |
| **publishSticky** (topic) | `void publishSticky(const String& topic)` | Sticky publish without payload. |
| **poll** | `void poll(size_t maxPerPoll = 8)` | Dispatch up to `maxPerPoll` queued events. Called by `ComponentRegistry::loopAll()`. |
| **reset** | `void reset()` | Clear all subscriptions and the queue. |

### Threading and Safety

- **Single-threaded assumption.** `subscribe()`, `unsubscribe()`, and `unsubscribeOwner()` must NOT be called during `poll()` dispatch. An `assert` guards this in debug builds.
- **Backpressure.** The internal queue is capped at 32 events. When full, the oldest event is dropped.
- **Wildcard matching.** Topics containing `*` are matched as prefix patterns (e.g., `"sensor/*"` matches `"sensor/temperature"` and `"sensor/humidity"`).

---

## 6. Logger

**Header:** `DomoticsCore/Logger.h`

Macro-based, platform-agnostic logging system with component tags and multiple severity levels.

### Log Macros

| Macro | Level | Numeric |
|-------|-------|---------|
| `DLOG_E(tag, fmt, ...)` | Error | 1 |
| `DLOG_W(tag, fmt, ...)` | Warning | 2 |
| `DLOG_I(tag, fmt, ...)` | Info | 3 |
| `DLOG_D(tag, fmt, ...)` | Debug | 4 |
| `DLOG_V(tag, fmt, ...)` | Verbose | 5 |

Simple-string variants: `DLOG_ES`, `DLOG_WS`, `DLOG_IS`, `DLOG_DS`, `DLOG_VS` -- take `(tag, message)` with no format arguments.

### Predefined Tags

`LOG_CORE`, `LOG_WIFI`, `LOG_MQTT`, `LOG_HTTP`, `LOG_HA`, `LOG_OTA`, `LOG_LED`, `LOG_SECURITY`, `LOG_WEB`, `LOG_SYSTEM`, `LOG_STORAGE`, `LOG_NTP`, `LOG_CONSOLE`

### Compile-Time Control

Set `CORE_DEBUG_LEVEL` in `platformio.ini` (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose). Macros below the configured level are compiled out entirely.

### LoggerCallbacks

```cpp
class LoggerCallbacks {
public:
    static void addCallback(std::function<void(LogLevel, const char*, const char*)> cb);
    static void removeCallback(std::function<void(LogLevel, const char*, const char*)> cb);
    static void broadcast(LogLevel level, const char* tag, const char* message);
};
```

Every log macro calls `LoggerCallbacks::broadcast()` after serial output, enabling RemoteConsole streaming, file logging, alert systems, etc.

### Platform Adaptation

- **ESP8266:** Uses `PSTR()`/PROGMEM format strings and a 128-byte buffer (`DOMOTICS_DLOG_BUF_SIZE`) to save ~11KB DRAM.
- **ESP32 / Other:** Standard `snprintf` with a 256-byte buffer.

---

## 7. Timer (NonBlockingDelay)

**Header:** `DomoticsCore/Timer.h`
**Namespace:** `DomoticsCore::Utils`

Non-blocking delay utility that uses `HAL::getMillis()` for platform-independent timing.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **Constructor** | `NonBlockingDelay(unsigned long intervalMs = 1000)` | Creates a timer with the given interval. Starts immediately. |
| **isReady** | `bool isReady()` | Returns `true` if the interval has elapsed since last trigger. Auto-resets on true. Returns `false` if disabled. |
| **reset** | `void reset()` | Reset the timer start point to now. |
| **setInterval** | `void setInterval(unsigned long intervalMs)` | Change the interval. |
| **getInterval** | `unsigned long getInterval() const` | Get current interval in milliseconds. |
| **enable** | `void enable()` | Enable the timer. |
| **disable** | `void disable()` | Disable the timer (`isReady()` always returns false). |
| **isEnabled** | `bool isEnabled() const` | Check enabled state. |
| **remaining** | `unsigned long remaining() const` | Milliseconds until next trigger (0 if ready or disabled). |
| **elapsed** | `unsigned long elapsed() const` | Milliseconds since last trigger. |

---

## 8. MemoryManager

**Header:** `DomoticsCore/MemoryManager.h`
**Namespace:** `DomoticsCore`

Singleton that detects available heap at boot and provides adaptive configuration. Auto-detected during `Core::begin()`.

### MemoryProfile Enum

| Profile | Free Heap | Description |
|---------|-----------|-------------|
| `FULL` | > 30 KB | All features enabled, maximum buffers |
| `STANDARD` | 15-30 KB | Moderate reductions |
| `MINIMAL` | 8-15 KB | Economy mode |
| `CRITICAL` | < 8 KB | Emergency mode, minimal operation |

### BufferType Enum

`WebSocket`, `HttpResponse`, `JsonDocument`, `LogBuffer`

### Feature Enum

`WebSocketUpdates`, `ChartHistory`, `SettingsLazyLoad`, `SchemaCompression`, `FullDashboard`

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **instance** | `static MemoryManager& instance()` | Meyer's singleton. |
| **detectProfile** | `MemoryProfile detectProfile() const` | Detect profile from current free heap. Called once in `Core::begin()`. |
| **getProfile** | `MemoryProfile getProfile() const` | Returns cached profile; auto-detects on first call. |
| **getProfileName** | `const char* getProfileName() const` | Human-readable profile name. |
| **getBufferSize** | `size_t getBufferSize(BufferType type) const` | Adaptive buffer size for the current profile. |
| **shouldEnable** | `bool shouldEnable(Feature feature) const` | Whether a feature should be enabled for the current profile. |
| **getWsUpdateInterval** | `uint32_t getWsUpdateInterval() const` | WebSocket push interval (ms). |
| **getMaxWsClients** | `uint8_t getMaxWsClients() const` | Max WebSocket clients. |
| **getChartHistoryPoints** | `uint8_t getChartHistoryPoints() const` | Chart history depth. |
| **getHeapAtBoot** | `uint32_t getHeapAtBoot() const` | Heap size when `detectProfile()` was called. |
| **getCurrentFreeHeap** | `uint32_t getCurrentFreeHeap() const` | Live free heap query. |
| **isLowMemory** | `bool isLowMemory() const` | Live check: free heap < MINIMAL threshold. |
| **isCriticalMemory** | `bool isCriticalMemory() const` | Live check: free heap < half of MINIMAL threshold. |
| **setThresholds** | `void setThresholds(const MemoryThresholds& t)` | Customize thresholds before `detectProfile()`. |
| **getThresholds** | `const MemoryThresholds& getThresholds() const` | Get current thresholds. |

### Profile Buffer Sizes

| Profile | WebSocket | HttpResponse | JsonDocument | LogBuffer |
|---------|-----------|-------------|-------------|-----------|
| FULL | 8192 | 4096 | 8192 | 200 |
| STANDARD | 4096 | 2048 | 4096 | 100 |
| MINIMAL | 2048 | 1024 | 2048 | 50 |
| CRITICAL | 1024 | 512 | 1024 | 20 |

### Profile Limits

| Profile | Max WS Clients | Max Providers | Chart Points | WS Interval |
|---------|---------------|---------------|-------------|-------------|
| FULL | 8 | 32 | 60 | 2s |
| STANDARD | 4 | 16 | 30 | 5s |
| MINIMAL | 2 | 8 | 10 | 10s |
| CRITICAL | 1 | 4 | 0 | disabled |

---

## 9. HeapTracker (Testing)

**Header:** `DomoticsCore/Testing/HeapTracker.h`
**Namespace:** `DomoticsCore::Testing`

Platform-agnostic heap monitoring for unit tests. Uses the HAL pattern (`HeapTracker_HAL.h` routing to `_Native.h`, `_ESP32.h`, or `_ESP8266.h`).

### HeapSnapshot

```cpp
struct HeapSnapshot {
    uint32_t freeHeap;
    uint32_t largestFreeBlock;
    uint32_t totalHeap;
    uint32_t timestamp;

    float getFragmentation() const;  // 0-100%
};
```

### HeapTracker Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| **takeSnapshot** | `HeapSnapshot takeSnapshot() const` | Get current heap state. |
| **checkpoint** | `void checkpoint(const String& name)` | Create a named checkpoint with current heap state. |
| **getCheckpoint** | `HeapSnapshot getCheckpoint(const String& name) const` | Get snapshot at a checkpoint. Returns zero-initialized if not found. |
| **hasCheckpoint** | `bool hasCheckpoint(const String& name) const` | Check existence. |
| **getDelta** | `int32_t getDelta(const String& start, const String& end) const` | Heap difference in bytes. Positive = memory used/leaked. |
| **getLeakRate** | `float getLeakRate(const String& start, const String& end) const` | Bytes leaked per minute. |
| **assertStable** | `MemoryTestResult assertStable(const String& start, const String& end, int32_t tolerance = 0) const` | Assert heap delta is within tolerance. |
| **assertNoGrowth** | `MemoryTestResult assertNoGrowth(const String& checkpoint, int32_t tolerance = 0) const` | Assert no growth since checkpoint (compared to current heap). |
| **clear** | `void clear()` | Remove all checkpoints. |
| **getCheckpointCount** | `size_t getCheckpointCount() const` | Number of stored checkpoints. |
| **getFreeHeap** | `uint32_t getFreeHeap() const` | Convenience: current free heap. |
| **toJson** | `String toJson() const` | JSON report of all checkpoints. |

### Test Macros

```cpp
HEAP_CHECKPOINT(tracker, "name")
HEAP_ASSERT_STABLE(tracker, "start", "end", toleranceBytes)
HEAP_ASSERT_NO_GROWTH(tracker, "checkpoint", toleranceBytes)
```

These macros integrate with Unity's `TEST_ASSERT_TRUE_MESSAGE`.

### Native Allocation Tracking Utilities

**Header:** `DomoticsCore/Testing/HeapTracker_Native.h`
**Namespace:** `DomoticsCore::Testing`

These utilities provide detailed heap tracking on native (desktop) platforms using system APIs (`mallinfo` on Linux, `mach_task_basic_info` on macOS). They complement the `HeapTracker` checkpoint-based approach with per-allocation granularity.

#### Standalone Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| **getRealHeapUsage** | `size_t getRealHeapUsage()` | Returns actual bytes currently allocated on the heap. |
| **getRealHeapTotal** | `size_t getRealHeapTotal()` | Returns total heap arena size. |
| **getRealLargestFreeBlock** | `size_t getRealLargestFreeBlock()` | Approximation of the largest contiguous free block. |
| **takeHeapSnapshot** | `HeapSnapshot takeHeapSnapshot()` | Creates a `HeapSnapshot` using real system heap metrics (not simulated). |

#### AllocationRecord

```cpp
struct AllocationRecord {
    void* ptr = nullptr;
    size_t size = 0;
    const char* file = nullptr;
    int line = 0;
    bool freed = false;
};
```

Records a single allocation with optional source location for leak diagnostics.

#### NativeAllocTracker

Singleton class for detailed per-allocation tracking. Enable it around the code under test and query unfreed allocations afterward.

| Method | Signature | Description |
|--------|-----------|-------------|
| **instance** | `static NativeAllocTracker& instance()` | Meyer's singleton accessor. |
| **setEnabled** | `void setEnabled(bool)` | Enable or disable tracking. |
| **isEnabled** | `bool isEnabled() const` | Check if tracking is active. |
| **recordAlloc** | `void recordAlloc(void* ptr, size_t size, const char* file = nullptr, int line = 0)` | Record an allocation event. |
| **recordFree** | `void recordFree(void* ptr)` | Record a deallocation event. |
| **getTotalAllocated** | `size_t getTotalAllocated() const` | Cumulative bytes allocated while tracking was enabled. |
| **getTotalFreed** | `size_t getTotalFreed() const` | Cumulative bytes freed while tracking was enabled. |
| **getCurrentUsage** | `size_t getCurrentUsage() const` | `totalAllocated - totalFreed`. |
| **getUnfreedAllocations** | `std::vector<AllocationRecord> getUnfreedAllocations() const` | List of allocations not yet freed. |
| **getUnfreedCount** | `size_t getUnfreedCount() const` | Number of unfreed allocations. |
| **getUnfreedBytes** | `size_t getUnfreedBytes() const` | Total bytes in unfreed allocations. |
| **reset** | `void reset()` | Clear all recorded allocations and counters. |

#### ScopedAllocTracking

RAII helper that enables `NativeAllocTracker` on construction (with reset) and disables it on destruction. Convenient for wrapping a test body.

| Method | Signature | Description |
|--------|-----------|-------------|
| **constructor** | `ScopedAllocTracking()` | Resets the tracker and enables it. |
| **destructor** | `~ScopedAllocTracking()` | Disables tracking. |
| **getUnfreedCount** | `size_t getUnfreedCount() const` | Delegates to `NativeAllocTracker::instance()`. |
| **getUnfreedBytes** | `size_t getUnfreedBytes() const` | Delegates to `NativeAllocTracker::instance()`. |

---

## 10. Platform HAL

**Header:** `DomoticsCore/Platform_HAL.h`
**Namespace:** `DomoticsCore::HAL`

The HAL routing header detects the platform at compile time and includes the appropriate implementation (`Platform_ESP32.h`, `Platform_ESP8266.h`, or `Platform_Stub.h`). All platform `#ifdef` directives are confined to HAL files per the constitution.

### Platform Detection Macros

| Macro | Value | Meaning |
|-------|-------|---------|
| `DOMOTICS_PLATFORM_ESP32` | 1 | ESP32 detected |
| `DOMOTICS_PLATFORM_ESP8266` | 1 | ESP8266 detected |
| `DOMOTICS_PLATFORM_AVR` | 1 | Arduino AVR detected |
| `DOMOTICS_PLATFORM_ARM` | 1 | Arduino ARM detected |
| `DOMOTICS_PLATFORM_UNKNOWN` | 1 | Unknown / native test environment |

### Feature Availability Macros

`DOMOTICS_HAS_WIFI`, `DOMOTICS_HAS_PREFERENCES`, `DOMOTICS_HAS_FREERTOS`, `DOMOTICS_HAS_ASYNC_TCP`, `DOMOTICS_HAS_SNTP`, `DOMOTICS_HAS_OTA`, `DOMOTICS_HAS_SPIFFS`, `DOMOTICS_HAS_LITTLEFS`, `DOMOTICS_RAM_SIZE_KB`, `DOMOTICS_FLASH_SIZE_KB`

### Convenience Check Macros

`DOMOTICS_SUPPORTS_WIFI()`, `DOMOTICS_SUPPORTS_STORAGE()`, `DOMOTICS_SUPPORTS_NTP()`, `DOMOTICS_SUPPORTS_OTA()`, `DOMOTICS_SUPPORTS_FULL_FRAMEWORK()`

### HAL Functions (`DomoticsCore::HAL::`)

`initializeLogging`, `isLoggerReady`, `getMillis`, `delay`, `formatChipIdHex`, `toUpperCase`, `substring`, `indexOf`, `startsWith`, `endsWith`, `getPlatformName`, `getChipModel`, `getChipRevision`, `getChipId`, `getFreeHeap`, `getTotalRAM_KB`, `getCpuFreqMHz`, `restart`, `ledBuiltinOn`, `ledBuiltinOff`, `isInternalLEDInverted`, `digitalWrite`, `pinMode`, `analogWrite`, `digitalRead`, `map`

### HAL Type Aliases and Constants

| Symbol | Kind | Description |
|--------|------|-------------|
| `HAL::SHA256` | Type alias | Alias for `Platform::SHA256`. Provides SHA-256 hash computation (uses mbedtls on ESP32). Methods: `begin()`, `update(data, len)`, `finish(digest)`, `abort()`, `toHex(digest, len)`. |
| `HAL::PI` | `constexpr double` | Mathematical constant PI for platform-independent trigonometry (e.g., LED effects). |

### Platform-Only Functions (`HAL::Platform::`)

The following functions are available in the `DomoticsCore::HAL::Platform` namespace but are **not** forwarded to the top-level `HAL::` namespace. Access them explicitly via `HAL::Platform::`.

| Function | Signature | Description |
|----------|-----------|-------------|
| **getTemperature** | `float getTemperature()` | Chip temperature in Celsius (ESP32 only; returns NAN on unsupported platforms). |
| **getTotalHeap** | `uint32_t getTotalHeap()` | Total heap size in bytes. |
| **getMinFreeHeap** | `uint32_t getMinFreeHeap()` | Minimum free heap ever recorded since boot. |
| **getMaxAllocHeap** | `uint32_t getMaxAllocHeap()` | Largest allocatable block in bytes. |
| **getFlashSize** | `uint32_t getFlashSize()` | Flash chip size in bytes. |
| **getSketchSize** | `uint32_t getSketchSize()` | Size of the uploaded sketch (program) in bytes. |
| **getFreeSketchSpace** | `uint32_t getFreeSketchSpace()` | Free space available for OTA updates in bytes. |
| **getResetReason** | `ResetReason getResetReason()` | Returns the platform-agnostic reset reason (see ResetReason enum below). |
| **getResetReasonString** | `String getResetReasonString(ResetReason)` | Human-readable string for a reset reason value. |
| **wasUnexpectedReset** | `bool wasUnexpectedReset(ResetReason)` | Returns `true` if the reset was caused by a crash (Panic, watchdog, brownout). |

### ResetReason Enum

**Namespace:** `DomoticsCore::HAL::Platform`

Platform-agnostic enumeration of reset causes. Defined in `Platform_ESP32.h` (and corresponding stub/ESP8266 headers).

```cpp
enum class ResetReason : uint8_t {
    Unknown      = 0,
    PowerOn      = 1,
    External     = 2,
    Software     = 3,
    Panic        = 4,
    IntWatchdog  = 5,
    TaskWatchdog = 6,
    Watchdog     = 7,
    DeepSleep    = 8,
    Brownout     = 9,
    SDIO         = 10
};
```

| Value | Meaning |
|-------|---------|
| `Unknown` | Cause could not be determined |
| `PowerOn` | Normal power-on boot |
| `External` | External reset pin asserted |
| `Software` | Software-initiated restart (`ESP.restart()`) |
| `Panic` | Unhandled exception or assertion failure |
| `IntWatchdog` | Interrupt watchdog timeout |
| `TaskWatchdog` | Task watchdog timeout |
| `Watchdog` | Other watchdog timeout |
| `DeepSleep` | Wake from deep sleep |
| `Brownout` | Supply voltage dropped below threshold |
| `SDIO` | SDIO reset |

### Filesystem HAL

**Header:** `DomoticsCore/Filesystem_HAL.h`
**Namespace:** `DomoticsCore::HAL::Filesystem`

The Filesystem HAL provides a platform-agnostic interface to the on-chip filesystem. Like the main Platform HAL, it routes to the appropriate implementation at compile time (`Filesystem_ESP32.h`, `Filesystem_ESP8266.h`, or `Filesystem_Stub.h`).

| Function | Signature | Description |
|----------|-----------|-------------|
| **begin** | `bool begin()` | Initialize the filesystem. Returns `true` on success. |
| **exists** | `bool exists(const String& path)` | Check whether a file exists at the given path. |
| **getFS** | `fs::FS& getFS()` | Return a reference to the underlying `fs::FS` object (for use with `AsyncWebServer`, etc.). |
| **format** | `bool format()` | Format (erase) the entire filesystem. Returns `true` on success. |
| **totalBytes** | `size_t totalBytes()` | Total filesystem capacity in bytes. |
| **usedBytes** | `size_t usedBytes()` | Bytes currently used on the filesystem. |

---

## 11. Lifecycle States and Transitions

```
  [Registered]
       |
       | Core::begin() / ComponentRegistry::initializeAll()
       v
  [Dependency Resolution]  --- fail ---> [DependencyError]
       |
       | (topological order)
       v
  [begin()]  --- fail ---> [Error status returned]
       |
       | success
       v
  [Active]  <--- component.setActive(true)
       |
       | onComponentsReady(registry)
       | afterAllComponentsReady()
       |
       | Core::loop()
       v
  [Looping]  --- loop() called each iteration
       |
       | Core::shutdown() / removeComponent()
       v
  [shutdown()]
       |
       v
  [Inactive]  <--- component.setActive(false)
                    eventBus.unsubscribeOwner(component)
```

**Shutdown order:** Reverse of initialization order. The `shutdown/start` event is published and dispatched before any component is shut down.

---

## 12. Dependency Resolution Algorithm

The `ComponentRegistry::resolveDependencies()` method uses **Kahn's algorithm** for topological sort:

1. **Build in-degree map.** For every registered component, count how many of its dependencies are present.
2. **Handle optional dependencies.** If a dependency has `required = false` and is not registered, skip it (log info). If `required = true` and missing, return `false` (DependencyError).
3. **Initialize queue.** All components with in-degree 0 (no unsatisfied dependencies) enter the queue.
4. **Process queue.** Pop a component, append to `initializationOrder`, decrement in-degree of its dependents. When a dependent reaches in-degree 0, push it to the queue.
5. **Circular dependency check.** If `initializationOrder.size() != components.size()`, a cycle exists -- return `false`.

**Time complexity:** O(V + E) where V = number of components, E = number of dependency edges.

---

## 13. Event System Details

### Core Lifecycle Events

Defined in `DomoticsCore/Events.h` (`DomoticsCore::Events` namespace):

| Constant | Topic String | Payload | When |
|----------|-------------|---------|------|
| `EVENT_COMPONENT_READY` | `"component/ready"` | `const char*` (component name) | After each component's `begin()` succeeds |
| `EVENT_COMPONENT_ERROR` | `"component/error"` | -- | Available for components to signal errors |
| `EVENT_SYSTEM_READY` | `"system/ready"` | `String("")` | After all components initialized |
| `EVENT_SYSTEM_REBOOT` | `"system/reboot"` | -- | Before system reboot |
| `EVENT_SHUTDOWN_START` | `"shutdown/start"` | `String("")` | Before shutdown sequence begins |

Component-specific events are defined in their respective libraries (e.g., `WifiEvents.h`, `MQTTEvents.h`).

### Sticky Events

`publishSticky()` stores the last payload for a topic. When a new subscriber calls `subscribe()` with `replayLast = true`, it immediately receives the stored value without waiting for the next publish. This is essential for state events like `wifi/sta/connected` that late-starting components need.

### Wildcard Subscriptions

Topics containing `*` are treated as prefix patterns. Example: subscribing to `"sensor/*"` matches `"sensor/temperature"`, `"sensor/humidity"`, etc. The matching algorithm:
- Extract the prefix before `*`.
- If `*` is at the end, check `startsWith(concrete, prefix)`.
- If `*` is in the middle, check `startsWith` and `endsWith` on the suffix after `*`.

### Backpressure

The event queue is capped at **32 events**. When the queue is full, the **oldest event is dropped** to make room for the new one. The `poll()` method processes up to `maxPerPoll` (default 8) events per call.

---

## 14. Configuration Patterns

### Defining Component Configuration

```cpp
class MyComponent : public IComponent {
    ComponentConfig config_;

public:
    MyComponent() {
        metadata = {"MyComp", "1.0.0", "", "My component"};

        config_.defineParameter(
            ConfigParam("interval", ConfigType::Integer, true, "5000", "Poll interval (ms)")
                .min(100).max(60000)
        );
        config_.defineParameter(
            ConfigParam("host", ConfigType::String, true, "", "Server hostname")
                .length(64)
        );
        config_.defineParameter(
            ConfigParam("enabled", ConfigType::Boolean, false, "true", "Enable feature")
        );
    }

    ComponentStatus begin() override {
        auto result = config_.validate();
        if (!result.isValid()) {
            DLOG_E("MYCOMP", "Config error: %s", result.toString().c_str());
            return result.status;
        }

        int interval = config_.getInt("interval");
        String host = config_.getValue("host");
        bool enabled = config_.getBool("enabled");
        // ... use values
        return ComponentStatus::Success;
    }
};
```

### Fluent Constraint API

```cpp
ConfigParam("port", ConfigType::Port, true, "1883", "Broker port");
ConfigParam("mode", ConfigType::String, false, "auto", "Mode")
    .options({"auto", "manual", "off"});
ConfigParam("threshold", ConfigType::Integer, false, "50", "Threshold")
    .min(0).max(100);
```
