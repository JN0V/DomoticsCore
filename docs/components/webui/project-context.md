# DomoticsCore-WebUI -- Project Context (AI Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI assistants working on the DomoticsCore-WebUI component. It covers identity, file inventory, architecture, dependencies, conventions, and constitution compliance.

---

## 1. Component Identity

| Property | Value |
|----------|-------|
| **Name** | DomoticsCore-WebUI |
| **Version** | 1.5.0 |
| **Role** | Web dashboard, REST API, and real-time update server |
| **Size** | ~3,894 lines across 15 header files (largest single component in the framework) |
| **Platforms** | ESP32, ESP8266, ESP32-S2/S3/C3, native/stub |
| **License** | MIT |
| **Namespace** | `DomoticsCore::Components` (main), `DomoticsCore::Components::WebUI` (sub-modules) |

---

## 2. File Inventory

### Primary Headers (`include/DomoticsCore/`)

| File | Lines | Purpose |
|------|-------|---------|
| `WebUI.h` | 950 | Main `WebUIComponent` class -- server setup, API routes, SSE/polling loop, self-provider |
| `IWebUIProvider.h` | 682 | `IWebUIProvider` interface, `CachingWebUIProvider` base class, `WebUIContext`, `WebUIField`, enums, `LazyState<T>` |
| `BaseWebUIComponents.h` | 384 | Static HTML widget generators (progress bar, toggle, button, slider, etc.) and `createLineChart` |
| `WebUI_HAL.h` | 25 | Platform routing header (`#if` dispatches to ESP32/ESP8266/Stub) |
| `WebUI_ESP32.h` | 33 | ESP32 buffer constants (8 KB WS buffer, 32 providers, 8 clients) |
| `WebUI_ESP8266.h` | 34 | ESP8266 buffer constants (1 KB WS buffer, 8 providers, 2 clients) |
| `WebUI_Stub.h` | 32 | Native/test buffer constants (4 KB WS buffer, 32 providers, 8 clients) |
| `DocMainpage.h` | 18 | Doxygen main page placeholder |

### Sub-module Headers (`include/DomoticsCore/WebUI/`)

| File | Lines | Purpose |
|------|-------|---------|
| `ProviderRegistry.h` | 345 | Context-to-provider map, provider discovery, enable/disable, schema chunk state |
| `StreamingContextSerializer.h` | 921 | State-machine JSON serializer for chunked HTTP responses (zero intermediate allocation) |
| `WebServerManager.h` | 149 | AsyncWebServer lifecycle, static asset serving, route registration |
| `WebSocketHandler.h` | 154 | SSE/polling dual-mode handler, broadcast, client tracking |
| `WebUIConfig.h` | 91 | Configuration struct with fixed-size `char[]` fields |
| `WebResponse_HAL.h` | 76 | HAL for PROGMEM response creation (ESP32 vs ESP8266 API differences) |

### Generated / Build

| File | Purpose |
|------|---------|
| `Generated/WebUIAssets.h` | Auto-generated PROGMEM arrays for HTML/CSS/JS (from `embed_webui.py`) |
| `embed_webui.py` | Build pre-script: minifies + gzips `webui_src/` into `WebUIAssets.h` |

### Frontend Source (`webui_src/`)

| File | Lines | Purpose |
|------|-------|---------|
| `index.html` | 171 | Dashboard HTML shell |
| `style.css` | 527 | Responsive theme-aware styles (dark/light/auto) |
| `app.js` | 1349 | Frontend logic: schema fetch, SSE/polling, dynamic rendering, actions |

---

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

---

## 5. Critical Development Conventions

### Never Hold WebUIContext References Across Calls

The `StreamingContextSerializer` and `SchemaChunkState` hold `const WebUIContext*` pointers into the provider's cached vector. **Do NOT call `invalidateContextCache()` while an HTTP schema request is in flight.** The pointer would dangle and cause a crash.

### Use Streaming for Large Payloads

Schema responses use `beginChunkedResponse()` with `StreamingContextSerializer` to avoid allocating the entire JSON in memory. This is critical on ESP8266 where free heap can be as low as 2-3 KB during AP+STA mode. Never build a full `JsonDocument` for schema output.

### Hybrid String Storage

`WebUIContext` and `WebUIField` use a hybrid pattern: a `const char*` pointer (for static/PROGMEM strings that cost zero heap) paired with a `String` member (for dynamic content). The `getCStr()` accessors return the pointer if set, otherwise `String::c_str()`. When building contexts with string literals, always use the `const char*` constructor to avoid heap allocation.

### ESP8266 Optimizations

- **Combined asset mode**: When `MemoryManager::isLowMemory()` is true, the server inlines CSS+JS into HTML and serves a single gzipped response, avoiding multiple concurrent HTTP connections.
- **GET for actions**: UI actions use `GET /api/ui/action?contextId=X&field=Y&value=Z` instead of POST body, to avoid the ~500 B body parser allocation that crashes ESP8266 at < 2.5 KB free heap.
- **Buffer sizes**: WS buffer is 1 KB on ESP8266 vs 8 KB on ESP32. The `buildUpdateJson()` method truncates gracefully if the buffer fills.
- **Polling mode**: When heap < 20 KB at startup, SSE is not created. The frontend polls instead.

### Static WS Buffer

`WebUIComponent::wsBuffer_` is a single static `char[WEBUI_WS_BUFFER_SIZE]` shared across all update builds. This is safe because the component is single-threaded (Arduino loop).

### CORS Headers

When `config.enableCORS = true`, the private `addCorsHeaders()` method adds `Access-Control-Allow-Origin: *` and related headers to every API response. This is opt-in and defaults to off.

### Configuration Persistence

`WebUIConfig` uses fixed-size `char[]` arrays (not `String`) to avoid heap fragmentation. All setters use `strncpy` with null-termination and log warnings on truncation. External persistence (e.g., via Storage component) is handled through the `onConfigChanged` callback.

---

## 6. Constitution Compliance

This section maps WebUI implementation decisions to specific constitution principles.

| Principle | Compliance |
|-----------|------------|
| **I. SOLID** | `IWebUIProvider` is a focused interface (ISP). `WebUIComponent` delegates to `WebServerManager`, `WebSocketHandler`, `ProviderRegistry` (SRP). Providers depend on `IWebUIProvider` abstraction (DIP). |
| **III. KISS** | Dual-mode SSE/polling decided by a single heap threshold, no complex negotiation. |
| **V. Performance First** | Streaming serializer avoids full-buffer allocation. PROGMEM for static assets. Adaptive buffer sizes per platform. Low-heap guards throughout. |
| **VI. EventBus** | Subscribes to `wifi/ap/enabled` via `on<bool>()`. No direct dependency on Wifi module. |
| **VII. File Size** | `WebUI.h` (950 lines) and `StreamingContextSerializer.h` (921 lines) exceed the 800-line limit. Both are candidates for further splitting. See Known Compliance Gaps below. |
| **IX. HAL** | All `#ifdef` platform detection is isolated in `WebUI_HAL.h`, `WebUI_ESP32.h`, `WebUI_ESP8266.h`, `WebUI_Stub.h`, and `WebResponse_HAL.h`. Business logic is platform-agnostic. |
| **X. Non-Blocking** | `loop()` completes quickly: checks timers, pumps SSE, builds JSON only when interval elapses. No `delay()`. |
| **XII. Multi-Registry** | `library.json` declares PlatformIO dependencies. Include path uses `DomoticsCore/` prefix. |
| **XIV. Memory Leak Prevention** | `CachingWebUIProvider` prevents repeated context allocation. `SchemaChunkState` releases provider vector on completion (`swap` with empty). `shrinkToFit()` called on ArduinoJson documents. Schema memory probes log heap delta at +500ms, +2s, and +10s after each schema request. |
| **XV. Semantic Versioning** | `library.json` version `1.5.0` matches `metadata.version` in `WebUIComponent` constructor. |

### Known Compliance Gaps

- **VII. File Size**: Three files exceed the 800-line Constitution VII limit and are tracked as roadmap items:
  - `WebUI.h` -- 950 lines. Planned split into server setup, API routes, and self-provider modules.
  - `StreamingContextSerializer.h` -- 921 lines. Planned extraction of field sub-state machine.
  - `Wifi.h` (DomoticsCore-Wifi) -- 881 lines. Cross-component awareness; tracked here for visibility.
- **II. TDD**: The component has limited unit test coverage due to its dependency on `ESPAsyncWebServer` which is difficult to mock. Contract tests for the provider interface exist.

### Known Issue: Excessive SSE Broadcast WARNING Logs

The `sendWebSocketUpdates()` method in `WebUI.h` (line 939) logs every SSE broadcast at `DLOG_W` (WARNING) level:

```cpp
DLOG_W(LOG_WEB, "SSE broadcast: %d bytes, clients=%d", len, webSocket->getClientCount());
```

With the default `wsUpdateInterval` of 5000 ms, this produces a WARNING log of approximately 1560 bytes every ~5.4 seconds during normal operation with at least one SSE client connected. This is a severity mismatch: routine SSE broadcasts are not warnings. The log level should be `DLOG_D` (DEBUG) to avoid polluting the serial output and triggering false alarms in monitoring systems. The low-heap skip case on line 933 correctly uses `DLOG_W` since that *is* an abnormal condition worth flagging.

---

## 7. Common Modification Scenarios

### Adding a New API Endpoint

Add a call to `webServer->registerRoute()` inside `setupApiRoutes()` in `WebUI.h`, or use the public `registerApiRoute()` method from external code.

### Adding a New Widget to BaseWebUIComponents

Add a new `static String` method to `BaseWebUIComponents` in `BaseWebUIComponents.h`. Follow the pattern of existing widgets: accept an `id` and `label`, return an HTML fragment using CSS classes `field-row`, `field-label`, etc.

### Adding a New WebUIFieldType

1. Add the enum value at the end of `WebUIFieldType` in `IWebUIProvider.h` (preserve existing ordinals).
2. Handle the new type in the frontend `app.js` field renderer.
3. The `StreamingContextSerializer` serializes the type as an integer, so no backend changes are needed.

### Creating a New Provider

Extend `CachingWebUIProvider`, implement `buildContexts()`, `getWebUIName()`, `getWebUIVersion()`, `handleWebUIRequest()`, and optionally `getWebUIData()` and `hasDataChanged()`. Register via `registerProviderWithComponent()`.

---

## 8. Testing Notes

- Native platform builds use `WebUI_Stub.h` constants.
- The `ESPAsyncWebServer` dependency makes full integration testing require a real or emulated ESP target.
- Unit tests for `StreamingContextSerializer`, `ProviderRegistry`, and `WebUIConfig` can run natively without hardware dependencies.
- The `LazyState<T>` template is fully testable in isolation.

### Test Suites

| Suite | File | Target | Purpose |
|-------|------|--------|---------|
| `test_streaming_serializer` | `test/test_streaming_serializer/test/` | native | Pause/resume correctness, JSON validity, chunked output |
| `test_webui_component` | `test/test_webui_component/test_webui_component.cpp` | native / ESP32 | API routes, provider lifecycle, config persistence |
| `test_schema_memory` | `test/test_schema_memory/test_schema_memory.cpp` | ESP32 | Heap profiling during schema generation (has own `platformio.ini`) |
| `test_heap_esp8266` | `test/test_heap_esp8266/test_heap_esp8266.cpp` | ESP8266 | Low-heap behavior, combined asset mode, fragmentation |

### Examples

| Example | Path | Description |
|---------|------|-------------|
| `HeadlessAPI` | `examples/HeadlessAPI/` | Pure REST API server without serving web assets |
| `WebUIOnly` | `examples/WebUIOnly/` | Minimal dashboard with self-provider only |
