<!-- workline
sources: [DomoticsCore-WebUI/include/DomoticsCore/WebUI.h, DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebServerManager.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/UpdateBuilder.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_ESP32.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_ESP8266.h]
checked: c1eb8f3
verified: agent:claude-code
-->
# DomoticsCore-WebUI -- Critical Development Conventions

Part of the [DomoticsCore-WebUI Project Context](project-context.md) (AI reference).

## Never Hold WebUIContext References Across Calls

The `StreamingContextSerializer` and `SchemaChunkState` hold `const WebUIContext*` pointers into the provider's cached vector. **Do NOT call `invalidateContextCache()` while an HTTP schema request is in flight.** The pointer would dangle and cause a crash.

## Use Streaming for Large Payloads

Schema responses use `beginChunkedResponse()` with `StreamingContextSerializer` to avoid allocating the entire JSON in memory. This is critical on ESP8266 where free heap can be as low as 2-3 KB during AP+STA mode. Never build a full `JsonDocument` for schema output.

## Hybrid String Storage

`WebUIContext` and `WebUIField` use a hybrid pattern: a `const char*` pointer (for static/PROGMEM strings that cost zero heap) paired with a `String` member (for dynamic content). The `getCStr()` accessors return the pointer if set, otherwise `String::c_str()`. When building contexts with string literals, always use the `const char*` constructor to avoid heap allocation.

## ESP8266 Optimizations

- **Combined asset mode**: When `MemoryManager::isLowMemory()` is true, the server inlines CSS+JS into HTML and serves a single gzipped response, avoiding multiple concurrent HTTP connections.
- **POST for actions, params in the query string**: UI actions use `POST /api/ui/action?contextId=X&field=Y&value=Z` — the method is POST (with a CSRF token, SEC-10) but the parameters stay in the query string, which is parsed for every method. Body accumulation only engages for a urlencoded body, so this costs the same heap as a GET; an earlier note here claimed a ~500 B body parser forced a GET, which was wrong as applied and is what allowed the cross-origin `<img>` shape.
- **Buffer sizes**: WS buffer is 1 KB on ESP8266 vs 8 KB on ESP32. The `buildUpdateJson()` method truncates gracefully if the buffer fills.
- **Polling mode**: When heap < 20 KB at startup, SSE is not created. The frontend polls instead.

## Static WS Buffer

`WebUIComponent::wsBuffer_` is a single static `char[WEBUI_WS_BUFFER_SIZE]` shared across all update builds. This is safe because the component is single-threaded (Arduino loop).

## CORS Headers

When `config.enableCORS = true`, the private `addCorsHeaders()` method adds `Access-Control-Allow-Origin: *` and related headers to every API response. This is opt-in and defaults to off.

## Configuration Persistence

`WebUIConfig` uses fixed-size `char[]` arrays (not `String`) to avoid heap fragmentation. All setters use `strncpy` with null-termination and log warnings on truncation. External persistence (e.g., via Storage component) is handled through the `onConfigChanged` callback.

