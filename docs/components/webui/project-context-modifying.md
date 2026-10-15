<!-- workline
sources: [DomoticsCore-WebUI/include/DomoticsCore/WebUI.h, DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h, DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/ProviderRegistry.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_HAL.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_Stub.h]
checked: c1eb8f3
verified: agent:claude-code
-->
# DomoticsCore-WebUI -- Modification Scenarios and Testing

Part of the [DomoticsCore-WebUI Project Context](project-context.md) (AI reference).

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
| front end | `webui_test/app.test.js` | Node + jsdom | `app.js` against a scripted fetch: field ids, Cancel, refusal display, the CSRF header |

The front-end suite is described on the [Front End](project-context-frontend.md) page.

**The digits-only rule has two implementations**: `Utils::digitsOnly` (`DomoticsCore/StringParse.h`, used by the console's `level` command) and `webUIParseUnsigned`, which checks the digits while it accumulates the value and adds the `uint32_t` ceiling. Noted here so the next reader does not rediscover it as a defect.

The host mocks under `tests/mocks/libraries/` (`ESPAsyncWebServer.h`, `AsyncEventSource.h`, `FS.h`, `pgmspace.h`) record calls and parse no HTTP. They prove handler logic and nothing about ESPAsyncWebServer: multipart parsing, the real `setRxTimeout` and every timing question are covered only by `tools/on-device/ota_upload_check.py`, which needs a board.

### Examples

| Example | Path | Description |
|---------|------|-------------|
| `HeadlessAPI` | `examples/HeadlessAPI/` | Pure REST API server without serving web assets |
| `WebUIOnly` | `examples/WebUIOnly/` | Minimal dashboard with self-provider only |
