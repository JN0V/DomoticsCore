# DomoticsCore - Technical Reference

> Cross-cutting technical reference for the entire DomoticsCore framework. For component-specific details, see `docs/components/{name}/technical-reference.md`.

> **IMPORTANT**: All development MUST comply with the [DomoticsCore Constitution](../.specify/memory/constitution.md). The constitution defines non-negotiable principles (TDD, memory leak prevention, HAL isolation, SOLID, KISS, YAGNI) that supersede all other practices. Read it before making any code change.

## Table of Contents

- [Component Model](#component-model)
- [Core Runtime](#core-runtime)
- [EventBus System](#eventbus-system)
- [HAL Architecture](#hal-architecture)
- [WebUI Framework](#webui-framework)
- [Configuration System](#configuration-system)
- [Logging System](#logging-system)
- [Memory Management](#memory-management)
- [Build System & CI](#build-system--ci)
- [Testing Infrastructure](#testing-infrastructure)
- [Versioning Strategy](#versioning-strategy)

---

## Component Model

### IComponent Interface

Every component implements this base interface defined in `DomoticsCore-Core/include/DomoticsCore/IComponent.h`:

```cpp
class IComponent {
public:
    ComponentMetadata metadata;

    // Lifecycle (pure virtual — all components must implement)
    virtual ComponentStatus begin() = 0;
    virtual void loop() = 0;
    virtual ComponentStatus shutdown() = 0;

    // Dependencies (override to declare required/optional dependencies)
    virtual std::vector<Dependency> getDependencies() const;

    // Status and State
    virtual bool isActive() const;
    virtual ComponentStatus getLastStatus() const;
    virtual const ComponentMetadata& getMetadata() const;

    // Optional overrides
    virtual const char* getTypeKey() const;
    virtual IWebUIProvider* getWebUIProvider();
    virtual void onComponentsReady(const ComponentRegistry& registry);
    virtual void afterAllComponentsReady();

    // Core access
    DomoticsCore::Core* getCore() const;
    void setActive(bool state);
    void setStatus(ComponentStatus status);

    // EventBus (typed event helpers)
    DomoticsCore::Utils::EventBus& eventBus();

    template<typename T>
    uint32_t on(const String& topic, std::function<void(const T&)> cb,
                bool replayLast = false);

    template<typename T>
    void emit(const String& topic, const T& payload, bool sticky = false);
};
```

### Dependency Declaration

Dependencies use the `Dependency` struct which supports required and optional dependencies:

```cpp
struct Dependency {
    const char* name;
    bool required;  // If false, component initializes even if dependency missing

    Dependency(const char* n) : name(n), required(true) {}
    Dependency(const char* n, bool req) : name(n), required(req) {}
};
```

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

### Component Lifecycle

Components track their state via `isActive()` (boolean) and `getLastStatus()` (ComponentStatus enum). There is no `ComponentState` enum — the active/inactive flag combined with the last status provides the lifecycle model.

### Registration and Initialization Order

1. Components are added via `core.addComponent(std::make_unique<T>())`
2. `Core::begin()` resolves dependency graph topologically
3. Components are initialized in dependency order
4. Optional dependencies are resolved but not required
5. Circular dependencies are detected and reported as errors

---

## Core Runtime

### Core Class (`DomoticsCore-Core/include/Core.h`)

The `Core` class is the central runtime:

```cpp
Core core;

// Add components before begin()
core.addComponent(std::make_unique<WifiComponent>());
core.addComponent(std::make_unique<MQTTComponent>());

core.begin();   // Resolves deps, initializes in order

void loop() {
    core.loop(); // Calls loop() on all active components
}
```

### ComponentRegistry

Stores all registered components. Access via:

```cpp
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
```

Components are registered by name (without "Component" suffix by convention).

### NonBlockingDelay (`DomoticsCore-Core/include/DomoticsCore/Timer.h`)

Non-blocking delay utility (class `DomoticsCore::Utils::NonBlockingDelay`):

```cpp
NonBlockingDelay timer(5000); // 5 seconds interval

if (timer.isReady()) {
    // Fires every 5 seconds, auto-resets
}

timer.setInterval(10000);  // Change interval
timer.disable();           // Temporarily disable
timer.enable();            // Re-enable
unsigned long r = timer.remaining();  // Time until next trigger
```

---

## EventBus System

### Architecture

Instance-based publish/subscribe system in `DomoticsCore-Core/include/DomoticsCore/EventBus.h`. The EventBus is **not a static class** — it is accessed via `core.getEventBus()` or through the component helper `eventBus()`.

### Dispatch Model

The EventBus is **queue-based** with polling dispatch:

- Events are queued on `publish()` (not dispatched immediately)
- `poll(maxPerPoll)` processes up to `maxPerPoll` events per call (default: 8)
- **Backpressure**: Queue capped at 32 events — oldest dropped on overflow
- `Core::loop()` calls `eventBus.poll()` automatically

### Event Topics

Events use **slash-separated** topics (not dots). Standard events defined in component `*Events.h` files:

| Event Topic | Defined In | Published By |
|-------------|-----------|--------------|
| `component/ready` | Events.h | Core |
| `component/error` | Events.h | Core |
| `system/ready` | Events.h | System |
| `system/reboot` | Events.h | System |
| `shutdown/start` | Events.h | Core |
| `wifi/sta/connected` | WifiEvents.h | Wifi |
| `wifi/ap/enabled` | WifiEvents.h | Wifi |
| `network/ready` | WifiEvents.h | Wifi |
| `mqtt/connected` | MQTTEvents.h | MQTT |
| `mqtt/disconnected` | MQTTEvents.h | MQTT |
| `mqtt/message` | MQTTEvents.h | MQTT |
| `mqtt/publish` | MQTTEvents.h | MQTT |
| `mqtt/subscribe` | MQTTEvents.h | MQTT |
| `ntp/synced` | NTPEvents.h | NTP |
| `ntp/sync_failed` | NTPEvents.h | NTP |
| `ota/start` | OTAEvents.h | OTA |
| `ota/progress` | OTAEvents.h | OTA |
| `ota/end` | OTAEvents.h | OTA |
| `ota/error` | OTAEvents.h | OTA |
| `ota/info` | OTAEvents.h | OTA |
| `ota/complete` | OTAEvents.h | OTA |
| `ota/completed` | OTAEvents.h | OTA |
| `ha/discovery_published` | HAEvents.h | HomeAssistant |
| `ha/entity_added` | HAEvents.h | HomeAssistant |
| `storage/ready` | StorageEvents.h | Storage |

### Usage

```cpp
// From within a component — typed event helpers
on<String>("wifi/sta/connected", [](const String& ip) {
    DLOG_I("APP", "WiFi connected: %s", ip.c_str());
}, true);  // replayLast = true for sticky events

emit<String>("my/custom/event", "payload data");
emit<String>("my/state", "ready", true);  // sticky = true

// Direct EventBus access (raw API)
auto& bus = core.getEventBus();
bus.subscribe("mqtt/connected", [](const void* payload) {
    // Handler receives const void* — cast to expected type
});
bus.publish("custom/topic", myPayload);
bus.publishSticky("custom/state", myState);
```

### Wildcard Subscriptions

The EventBus supports prefix wildcards using `*`:

```cpp
bus.subscribe("sensor.*", handler);  // Matches "sensor/temperature", "sensor/humidity", etc.
```

> **Note**: EventBus wildcards use `*` (not MQTT's `+`/`#`). This is an internal EventBus feature, separate from MQTT topic wildcards.

### Best Practices

- Use slash-separated topic names: `component/action`
- Prefer EventBus over direct component references
- Use sticky events for state that late subscribers need
- Keep event handlers fast — they execute during `poll()` on the main loop thread
- Subscribe/unsubscribe must NOT be called during `poll()` dispatch (not thread-safe)

---

## HAL Architecture

### Compile-Time Platform Routing

Each HAL file uses preprocessor detection:

```cpp
// Platform_HAL.h
#if defined(ESP32)
    #include "Platform_ESP32.h"
#elif defined(ESP8266)
    #include "Platform_ESP8266.h"
#else
    #include "Platform_Stub.h"  // Native testing
#endif
```

### HAL Files in the Project

| HAL | Component | Platforms |
|-----|-----------|-----------|
| `Platform_HAL.h` | Core | ESP32, ESP8266, Stub |
| `Filesystem_HAL.h` | Core | ESP32, ESP8266, Stub |
| `Wifi_HAL.h` | Wifi | ESP32, ESP8266, Stub |
| `WiFiServer_HAL.h` | Wifi | ESP32, ESP8266, Stub |
| `WebUI_HAL.h` | WebUI | ESP32, ESP8266, Stub |
| `MQTT_HAL.h` | MQTT | ESP32, ESP8266, Stub |
| `NTP_HAL.h` | NTP | ESP32, ESP8266, Stub |
| `Storage_HAL.h` | Storage | ESP32, ESP8266, Stub |
| `Update_HAL.h` | OTA | ESP32, ESP8266, Stub |

### Adding a New Platform

1. Create `*_NewPlatform.h` implementing the same interface
2. Add `#elif` branch in the `*_HAL.h` router
3. Ensure all platform-specific APIs are wrapped

---

## WebUI Framework

### Architecture

The WebUI system has 4 layers:

1. **WebServerManager** — HTTP server lifecycle (ESPAsyncWebServer)
2. **WebSocketHandler** — Real-time bidirectional communication
3. **ProviderRegistry** — Manages component UI providers
4. **StreamingContextSerializer** — Chunked JSON for large payloads

### WebUI Provider Interface

Defined in `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`:

```cpp
class IWebUIProvider {
public:
    // Context management (pure virtual)
    virtual void forEachContext(std::function<bool(const WebUIContext&)> callback) = 0;
    virtual size_t getContextCount() = 0;
    virtual bool getContextAt(size_t index, WebUIContext& outContext) = 0;
    virtual const WebUIContext* getContextAtRef(size_t index);  // Returns nullptr by default

    // Request handling (pure virtual)
    virtual String handleWebUIRequest(const String& contextId, const String& endpoint,
                                      const String& method,
                                      const std::map<String, String>& params) = 0;

    // Data retrieval
    virtual String getWebUIData(const String& contextId);    // Returns "{}" by default
    virtual bool hasDataChanged(const String& contextId);     // Returns true by default

    // Metadata (pure virtual)
    virtual String getWebUIName() const = 0;
    virtual String getWebUIVersion() const = 0;
    virtual WebUIContext getWebUIContext(const String& contextId) = 0;
    virtual bool isWebUIEnabled();  // Returns true by default
};
```

### WebUI Locations

Providers contribute to 6 UI locations via `WebUILocation`:

| Location | Description |
|----------|-------------|
| `Dashboard` | Main dashboard cards |
| `ComponentDetail` | Full component detail view |
| `HeaderStatus` | Status badges in header bar |
| `QuickControls` | Quick control widgets |
| `Settings` | Settings forms |
| `HeaderInfo` | Information displays in header |

### UI Field Types

The WebUI uses `WebUIField` and `WebUIContext` fluent builders (not `BaseWebUIComponents`). Available `WebUIFieldType` values: `Text`, `Number`, `Float`, `Boolean`, `Select`, `Slider`, `Color`, `Button`, `Display`, `Chart`, `Status`, `Progress`, `Password`, `File`.

### Presentation Styles

`WebUIPresentation` enum: `Card`, `Gauge`, `Graph`, `StatusBadge`, `ProgressBar`, `Table`, `Toggle`, `Slider`, `Text`, `Button`.

### Embedded Assets

Web frontend files (HTML/CSS/JS) are compiled into `Generated/WebUIAssets.h` via `embed_webui.py`. Assets are gzip-compressed and served with proper headers.

---

## Configuration System

### Pattern

Each component defines a `*Config` struct:

```cpp
struct MQTTConfig {
    const char* server = "localhost";
    uint16_t port = 1883;
    const char* clientId = nullptr;  // Auto-generated if null
    bool enabled = true;
    // ...
};
```

### Usage

```cpp
MQTTConfig mqttConfig;
mqttConfig.server = "192.168.1.100";
mqttConfig.port = 1883;

auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
core.addComponent(std::move(mqtt));
```

### Persistence

Components that need persistent configuration use the `StorageComponent`:

```cpp
auto* storage = core.getComponent<StorageComponent>("Storage");
storage->putString("mqtt_server", "192.168.1.100");
String server = storage->getString("mqtt_server", "localhost");
```

---

## Logging System

### Macros

Logging is **macro-based**, defined in `DomoticsCore-Core/include/DomoticsCore/Logger.h`. There is no `Logger` class — logging is controlled at compile-time via the `CORE_DEBUG_LEVEL` build flag.

```cpp
DLOG_E("TAG", "Error: %s failed", operation);   // Error
DLOG_W("TAG", "Warning: low heap %d", heap);     // Warning
DLOG_I("TAG", "Info message: %s", text);          // Info
DLOG_D("TAG", "Debug message: %d", value);        // Debug
DLOG_V("TAG", "Verbose details: %d", detail);     // Verbose
```

Convenience macros for simple strings (no format args): `DLOG_ES`, `DLOG_WS`, `DLOG_IS`, `DLOG_DS`, `DLOG_VS`.

### Log Levels

```cpp
enum LogLevel {
    LOG_LEVEL_NONE    = 0,
    LOG_LEVEL_ERROR   = 1,
    LOG_LEVEL_WARN    = 2,
    LOG_LEVEL_INFO    = 3,
    LOG_LEVEL_DEBUG   = 4,
    LOG_LEVEL_VERBOSE = 5
};
```

- Compile-time level set via `CORE_DEBUG_LEVEL` (default: `LOG_LEVEL_INFO`)
- Macros below the configured level are compiled out entirely (zero overhead)

### Standard Tags

Pre-defined tag constants: `LOG_CORE`, `LOG_WIFI`, `LOG_MQTT`, `LOG_HTTP`, `LOG_HA`, `LOG_OTA`, `LOG_LED`, `LOG_SECURITY`, `LOG_WEB`, `LOG_SYSTEM`, `LOG_STORAGE`, `LOG_NTP`, `LOG_CONSOLE`.

### Callbacks

`LoggerCallbacks::addCallback()` allows registering listeners for log messages (used by RemoteConsole for Telnet streaming).

---

## Memory Management

### MemoryManager (`DomoticsCore-Core/include/DomoticsCore/MemoryManager.h`)

Singleton providing adaptive memory profiles based on available heap at boot:

```cpp
auto& mm = MemoryManager::instance();
MemoryProfile profile = mm.getProfile();

// Adaptive buffer sizing
size_t bufSize = mm.getBufferSize(BufferType::JsonDocument);

// Feature gating
if (mm.shouldEnable(Feature::ChartHistory)) {
    // Only on devices with enough memory
}

// Runtime checks
if (mm.isLowMemory()) { /* reduce allocations */ }
if (mm.isCriticalMemory()) { /* emergency mode */ }
```

### Memory Profiles

```cpp
enum class MemoryProfile {
    FULL,       // > 30KB free heap
    STANDARD,   // 15-30KB free heap
    MINIMAL,    // 8-15KB free heap
    CRITICAL    // < 8KB free heap
};
```

| Profile | WebSocket Buffer | JSON Doc | HTTP Response |
|---------|-----------------|----------|---------------|
| FULL | 8192 | 8192 | 4096 |
| STANDARD | 4096 | 4096 | 2048 |
| MINIMAL | 2048 | 2048 | 1024 |
| CRITICAL | 1024 | 1024 | 512 |

### HeapTracker (`DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker.h`)

Memory leak detection for tests using a **checkpoint + assertion** pattern:

```cpp
HeapTracker tracker;
tracker.checkpoint("before");
// ... operations ...
tracker.checkpoint("after");

// Assert heap stability
MemoryTestResult result = tracker.assertStable("before", "after", 100);
if (!result.passed) {
    // result.delta, result.message contain details
}

// Or use assertion macros
HEAP_CHECKPOINT(tracker, "start");
// ... operations ...
HEAP_CHECKPOINT(tracker, "end");
HEAP_ASSERT_STABLE(tracker, "start", "end", 100);
HEAP_ASSERT_NO_GROWTH(tracker, "start", 50);
```

### Memory Discipline (Constitution XIV)

These rules are **non-negotiable** per the constitution. Violations will cause OOM crashes on long-running devices.

- **`shrink_to_fit()` after every erase/clear**: When a `std::vector` or other container shrinks (via `erase()`, `clear()`, `pop_back()`), always call `shrink_to_fit()` immediately after to release unused capacity back to the heap.
- **No String concatenation in loops**: Use `snprintf()` with static buffers instead of `String` `+` operator in hot paths or loops.
- **PROGMEM for constants**: All static strings, HTML, CSS, JS must be stored in flash (PROGMEM), not RAM.
- **Heap stability testing**: Every feature must be tested with `HeapTracker` — checkpoint before/after repeated operations, assert no growth.

```cpp
// WRONG — vector capacity never shrinks
clients.erase(it);

// CORRECT — release unused memory
clients.erase(it);
clients.shrink_to_fit();

// WRONG — String concatenation in loop
for (auto& s : sensors) {
    String msg = prefix + "/" + s.name + "/state";  // N allocations
}

// CORRECT — static buffer
char buf[128];
for (auto& s : sensors) {
    snprintf(buf, sizeof(buf), "%s/%s/state", prefix, s.name);
}
```

### Memory Guidelines

| Platform | Free Heap | Recommendations |
|----------|-----------|-----------------|
| ESP32 | ~200KB | Full features, large buffers |
| ESP32-C3 | ~150KB | Full features, moderate buffers |
| ESP8266 | ~40KB | Reduced features, small buffers, no AP+STA |

---

## Build System & CI

### PlatformIO Configuration

Root `library.json` defines:
- Include paths for all components (`-IDomoticsCore-*/include`)
- Source filter (`+<DomoticsCore-*/src/*>`)
- External dependencies (ArduinoJson, ESPAsyncWebServer, AsyncTCP, PubSubClient)
- WebUI build script (`embed_webui.py`)

### Build Scripts

| Script | Purpose |
|--------|---------|
| `build_all_examples.sh` | Compiles all component examples |
| `run_all_tests.sh` | Runs all native platform tests |
| `check_everything.sh` | Full validation (build + test) |
| `clean_examples.py` | Cleans build artifacts |

### CI/CD

GitHub Actions in `.github/workflows/`:
- Build validation on push/PR
- Test execution on native platform
- Release creation on tagged commits

---

## Testing Infrastructure

### Test Structure

```
tests/
├── unit/
│   ├── 01-optional-dependencies/   # Optional dependency resolution
│   ├── 02-lifecycle-callback/      # Lifecycle callback mechanism
│   ├── 05-storage-namespace/       # Storage namespace isolation
│   └── 06-webui-refactor/          # WebUI refactoring validation
├── mocks/
│   ├── MockWifiHAL.h              # WiFi mock
│   ├── MockMQTTClient.h           # MQTT client mock
│   ├── MockEventBus.h             # EventBus mock
│   ├── MockStorage.h              # Storage mock
│   ├── MockNTPClient.h            # NTP mock
│   └── MockAsyncWebServer.h       # Web server mock
└── README.md
```

### Running Tests

```bash
# All tests
./run_all_tests.sh

# Specific test suite
cd tests/unit/01-optional-dependencies
pio test -e native
```

### Writing Tests

Tests run on the `native` platform (no hardware). Use mocks from `tests/mocks/` and stubs from `*_Stub.h` files.

---

## Versioning Strategy

### Library Version

The root `library.json` carries the overall library version (currently `1.7.0`). Each component also has its own version in its `library.json`.

### Component Versions

Components are versioned independently:

| Component | Version |
|-----------|---------|
| Core | 1.5.0 |
| LED | 1.3.0 |
| MQTT | 1.4.1 |
| NTP | 1.3.0 |
| OTA | 1.4.1 |
| RemoteConsole | 1.4.1 |
| Storage | 1.4.2 |
| System | 1.4.1 |
| SystemInfo | 1.4.0 |
| WebUI | 1.5.0 |
| Wifi | 1.4.1 |
| HomeAssistant | 1.5.0 |

### Semantic Versioning

- **Major**: Breaking API changes
- **Minor**: New features, backward compatible
- **Patch**: Bug fixes

### Changelog

All changes are tracked in `CHANGELOG.md` at the project root.
