<!-- workline
sources: [DomoticsCore-WebUI/library.json, DomoticsCore-WebUI/include/DomoticsCore/WebUI.h, DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_HAL.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_ESP32.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_ESP8266.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_Stub.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebResponse_HAL.h]
checked: 3c4d96c
verified: agent:documentalist
-->
# DomoticsCore-WebUI -- Constitution Compliance

Part of the [DomoticsCore-WebUI Project Context](project-context.md) (AI reference).

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

## Known Compliance Gaps

- **VII. File Size**: Three files exceed the 800-line Constitution VII limit and are tracked as roadmap items:
  - `WebUI.h` -- 950 lines. Planned split into server setup, API routes, and self-provider modules.
  - `StreamingContextSerializer.h` -- 921 lines. Planned extraction of field sub-state machine.
  - `Wifi.h` (DomoticsCore-Wifi) -- 881 lines. Cross-component awareness; tracked here for visibility.
- **II. TDD**: The component has limited unit test coverage due to its dependency on `ESPAsyncWebServer` which is difficult to mock. Contract tests for the provider interface exist.

## Resolved (v2.0.1): SSE Broadcast Log Severity

The SSE broadcast log was changed from `DLOG_W` to `DLOG_D` in v2.0.1 (SSE-1). Routine broadcasts no longer pollute WARNING-level logs. The low-heap skip case on line 933 still correctly uses `DLOG_W` for the genuinely abnormal condition.

