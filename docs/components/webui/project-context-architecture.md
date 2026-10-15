<!-- workline
sources: [DomoticsCore-WebUI/library.json, DomoticsCore-WebUI/include/DomoticsCore/WebUI.h, DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebServerManager.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h]
checked: c1eb8f3
verified: agent:claude-code
-->
# DomoticsCore-WebUI -- Key Classes and Dependencies

Part of the [DomoticsCore-WebUI Project Context](project-context.md) (AI reference).

## 3. Key Classes and Their Relationships

```
WebUIComponent (IComponent + CachingWebUIProvider + IComponentLifecycleListener)
 |-- owns WebServerManager        (HTTP server, static assets)
 |-- owns WebSocketHandler        (SSE/polling, broadcast)
 |-- owns ProviderRegistry        (context-to-provider map, discovery, schema)
 |       |-- uses StreamingContextSerializer (chunked JSON output)
 |       |-- tracks ProviderInfo   (component association, enabled state)
 |
 |-- implements CachingWebUIProvider (self-registration for uptime + settings)
 |-- listens to ComponentRegistry  (dynamic add/remove of providers)
 |-- subscribes to "wifi/ap/enabled" (close connections on network change)
```

### Provider Interface Hierarchy

```
IWebUIProvider (abstract)
 |-- CachingWebUIProvider (caches contexts, implements forEachContext/getContextAtRef)
      |-- WebUIComponent (self-provider)
      |-- [user components]
```

---

## 4. Dependencies

### Library Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| `DomoticsCore-Core` | (framework) | `IComponent`, `ComponentRegistry`, `Logger`, `MemoryManager`, `Platform_HAL`, `Filesystem_HAL` |
| `bblanchon/ArduinoJson` | >= 7.0.0 | JSON serialization for API responses and config |
| `ESP32Async/ESPAsyncWebServer` | >= 3.8.0 | Async HTTP server and SSE (`AsyncEventSource`) |
| `ESP32Async/AsyncTCP` | >= 3.4.8 | TCP transport for ESP32 |
| `ESP32Async/ESPAsyncTCP` | >= 2.0.0 | TCP transport for ESP8266 |

### Internal Core Dependencies

| Symbol | Header | Usage |
|--------|--------|-------|
| `IComponent` | `IComponent.h` | Base class |
| `ComponentRegistry` | `ComponentRegistry.h` | Provider discovery, lifecycle listener |
| `Logger` / `DLOG_*` | `Logger.h` | Structured logging |
| `MemoryManager` | `MemoryManager.h` | Adaptive WS client limits, low-memory detection |
| `HAL::Platform` | `Platform_HAL.h` | `getMillis()`, `getFreeHeap()`, `getMaxAllocHeap()` |
| `HAL::Filesystem` | `Filesystem_HAL.h` | Optional filesystem-based asset serving |
| `EventBus` (`on<T>()`) | via `IComponent` | Subscribe to `wifi/ap/enabled` events |

