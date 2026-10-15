<!-- workline
sources: [DomoticsCore-WebUI/include/DomoticsCore/WebUI.h, DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h, DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_HAL.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_ESP32.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_ESP8266.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_Stub.h, DomoticsCore-WebUI/include/DomoticsCore/DocMainpage.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/DeclaredApiHandler.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebServerManager.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebResponse_HAL.h]
checked: c1eb8f3
verified: agent:claude-code
-->
# DomoticsCore-WebUI -- File Inventory

Part of the [DomoticsCore-WebUI Project Context](project-context.md) (AI reference).

## Primary Headers (`include/DomoticsCore/`)

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

## Sub-module Headers (`include/DomoticsCore/WebUI/`)

| File | Lines | Purpose |
|------|-------|---------|
| `DeclaredApiHandler.h` | 38 | Single `AsyncWebHandler` answering `GET` on every path declared with `withAPI()` |
| `ProviderRegistry.h` | 345 | Context-to-provider map, provider discovery, enable/disable, schema chunk state |
| `StreamingContextSerializer.h` | 921 | State-machine JSON serializer for chunked HTTP responses (zero intermediate allocation) |
| `WebServerManager.h` | 149 | AsyncWebServer lifecycle, static asset serving, route registration |
| `WebSocketHandler.h` | 154 | SSE/polling dual-mode handler, broadcast, client tracking |
| `WebUIConfig.h` | 91 | Configuration struct with fixed-size `char[]` fields |
| `WebResponse_HAL.h` | 76 | HAL for PROGMEM response creation (ESP32 vs ESP8266 API differences) |

## Generated / Build

| File | Purpose |
|------|---------|
| `Generated/WebUIAssets.h` | Auto-generated PROGMEM arrays for HTML/CSS/JS (from `embed_webui.py`) |
| `embed_webui.py` | Build pre-script: minifies + gzips `webui_src/` into `WebUIAssets.h` |
