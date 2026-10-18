<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/Core.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h, DomoticsCore-Core/include/DomoticsCore/ComponentConfig.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/Events.h, DomoticsCore-Core/include/DomoticsCore/Logger.h, DomoticsCore-Core/include/DomoticsCore/Timer.h, DomoticsCore-Core/include/DomoticsCore/StringParse.h, DomoticsCore-Core/include/DomoticsCore/MemoryManager.h, DomoticsCore-Core/include/DomoticsCore/Platform_HAL.h, DomoticsCore-Core/include/DomoticsCore/Platform_Arduino.h, DomoticsCore-Core/include/DomoticsCore/Platform_Stub.h, DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker.h, DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_Native.h, DomoticsCore-Core/src/Core.cpp, DomoticsCore-Core/src/IComponent.cpp, DomoticsCore-Core/library.json]
checked: 4cdb3e6
judged: b678013
verified: agent:documentalist
-->
# DomoticsCore-Core -- File Inventory

Part of the [DomoticsCore-Core Project Context](project-context.md) (AI agent reference).

## Header Files (`DomoticsCore-Core/include/DomoticsCore/`)

| File | Purpose |
|------|---------|
| `Core.h` | `Core` class -- central runtime, owns `ComponentRegistry`, drives lifecycle |
| `IComponent.h` | `IComponent` abstract base -- lifecycle hooks, dependency declaration, EventBus helpers, Core injection |
| `ComponentRegistry.h` | `ComponentRegistry` -- registration, dependency resolution (Kahn's algorithm), coordinated init/loop/shutdown |
| `ComponentConfig.h` | `ComponentStatus` enum, `statusToString()`, `ComponentMetadata` |
| `EventBus.h` | `EventBus` -- queued pub/sub with topic strings, wildcard matching, sticky events |
| `Events.h` | Core lifecycle event topic constants (`component/ready`, `system/ready`, `shutdown/start`, etc.) |
| `Logger.h` | Logging macros (`DLOG_E/W/I/D/V`), `LoggerCallbacks`, predefined component tags |
| `Timer.h` | `NonBlockingDelay` utility -- non-blocking interval timer using `HAL::getMillis()` |
| `StringParse.h` | `Utils::digitsOnly()` -- the digits-only rule for user-typed numbers |
| `MemoryManager.h` | `MemoryManager` singleton -- boot heap profile, WebSocket client limit, live low-memory check |
| `Platform_HAL.h` | Platform detection routing header -- includes `Platform_ESP32.h`, `Platform_ESP8266.h`, or `Platform_Stub.h` |
| `Platform_ESP32.h` | ESP32-specific HAL implementation |
| `Platform_ESP8266.h` | ESP8266-specific HAL implementation (PROGMEM-optimized logging) |
| `Platform_Stub.h` | Native/test stub implementation |
| `Platform_Arduino.h` | Shared Arduino utilities (time, string, GPIO, math, logging) included by `Platform_ESP32.h` and `Platform_ESP8266.h` to avoid code duplication |
| `Filesystem_HAL.h` | Filesystem HAL routing header |
| `Filesystem_ESP32.h` | ESP32 filesystem implementation |
| `Filesystem_ESP8266.h` | ESP8266 filesystem implementation |
| `Filesystem_Stub.h` | Native/test filesystem stub |
| `ArduinoJsonString.h` | ArduinoJson 7 converter functions for stub `String` class (native tests only; no-op on Arduino platforms) |
| `DocMainpage.h` | Doxygen main page documentation |
| `Testing.h` | Umbrella header for testing utilities (currently includes HeapTracker; StabilityTestRunner planned) |
| `Testing/HeapTracker.h` | `HeapTracker` class -- checkpoint-based heap monitoring for leak detection |
| `Testing/HeapTracker_HAL.h` | HeapTracker platform routing |
| `Testing/HeapTracker_Native.h` | Native platform heap tracking (mallinfo) |
| `Testing/HeapTracker_ESP32.h` | ESP32 heap tracking (heap_caps API) |
| `Testing/HeapTracker_ESP8266.h` | ESP8266 heap tracking (ESP.getFreeHeap) |

## Source Files (`DomoticsCore-Core/src/`)

| File | Purpose |
|------|---------|
| `Core.cpp` | `Core` implementation -- `begin()`, `loop()`, `shutdown()` |
| `IComponent.cpp` | `IComponent::getCore()` lazy injection implementation |

## Other Files

| File | Purpose |
|------|---------|
| `library.json` | PlatformIO library manifest |
| `README.md` | Component README |
| `LOGGING.md` | Comprehensive logging system guide |

