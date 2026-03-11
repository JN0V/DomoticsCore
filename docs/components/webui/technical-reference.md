# DomoticsCore-WebUI -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides the complete API reference for the DomoticsCore-WebUI component (v1.5.0). All types reside in namespace `DomoticsCore::Components` unless otherwise noted.

---

## Table of Contents

1. [WebUIComponent](#webuicomponent)
2. [IWebUIProvider](#iwebuiprovider)
3. [CachingWebUIProvider](#cachingwebuiprovider)
4. [WebUIContext](#webuicontext)
5. [WebUIField](#webuifield)
6. [Enumerations](#enumerations)
7. [LazyState\<T\>](#lazystatet)
8. [BaseWebUIComponents](#basewebuicomponents)
9. [ProviderRegistry](#providerregistry)
10. [StreamingContextSerializer](#streamingcontextserializer)
11. [WebSocketHandler](#websockethandler)
12. [WebServerManager](#webservermanager)
13. [WebUIConfig](#webuiconfig)
14. [HAL Layer](#hal-layer)
15. [Embedded Assets](#embedded-assets)

---

## WebUIComponent

**Header**: `DomoticsCore/WebUI.h`
**Namespace**: `DomoticsCore::Components`
**Inherits**: `IComponent`, `CachingWebUIProvider`, `ComponentRegistry::IComponentLifecycleListener`

The main component class. It creates the async web server, discovers providers, and pushes real-time updates via SSE or polling.

### Constructor

```cpp
WebUIComponent(const WebUIConfig& cfg = WebUIConfig());
```

Constructs the component with the given configuration. Sets `metadata.name = "WebUI"`, `metadata.version = "1.5.0"`, `metadata.author = "DomoticsCore"`, and `metadata.description = "Web dashboard and API component"`. Internally creates a `ProviderRegistry`, `WebServerManager`, and `WebSocketHandler`.

### IComponent Lifecycle

| Method | Signature | Description |
|--------|-----------|-------------|
| `begin()` | `ComponentStatus begin()` | Adapts WS client limits from `MemoryManager`, initializes server and socket handler, sets up API routes, and starts listening. |
| `loop()` | `void loop()` | Pumps SSE/polling updates, runs schema memory probes, and calls `sendWebSocketUpdates()` when the interval elapses. |
| `shutdown()` | `ComponentStatus shutdown()` | Stops the web server. |
| `onComponentsReady()` | `void onComponentsReady(const ComponentRegistry&)` | Discovers all providers via the registry, subscribes to `wifi/ap/enabled` to close connections when the AP goes down, and registers as a lifecycle listener for future add/remove events. |

### Provider Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `registerProvider` | `void registerProvider(IWebUIProvider* provider)` | Register a standalone provider. |
| `registerProviderWithComponent` | `void registerProviderWithComponent(IWebUIProvider* provider, IComponent* component)` | Register a provider tied to a component for lifecycle callbacks. |
| `unregisterProvider` | `void unregisterProvider(IWebUIProvider* provider)` | Remove all contexts contributed by a provider. |
| `registerProviderFactory` | `void registerProviderFactory(const String& typeKey, std::function<IWebUIProvider*(IComponent*)> factory)` | Register a factory that creates providers for components matching a type key. |
| `registerApiRoute` | `void registerApiRoute(const String& uri, WebRequestMethod method, ArRequestHandlerFunction handler)` | Register a custom REST endpoint on the web server. |
| `registerApiUploadRoute` | `void registerApiUploadRoute(const String& uri, ArRequestHandlerFunction handler, ArUploadHandlerFunction uploadHandler)` | Register a file upload endpoint. |

### Configuration & State

| Method | Signature | Description |
|--------|-----------|-------------|
| `getConfig` | `const WebUIConfig& getConfig() const` | Return current config by const reference. |
| `setConfig` | `void setConfig(const WebUIConfig& cfg)` | Replace current config. |
| `setConfigCallback` | `void setConfigCallback(std::function<void(const WebUIConfig&)> callback)` | Set a callback invoked when settings change via the UI. |
| `getPort` | `uint16_t getPort() const` | Return the HTTP listen port. |
| `getWebSocketClients` | `int getWebSocketClients() const` | Return the total number of connected clients (SSE + polling). |
| `notifyWiFiNetworkChanged` | `void notifyWiFiNetworkChanged()` | Send a `wifi_network_changed` event to SSE clients. |
| `closeAllWebSocketConnections` | `void closeAllWebSocketConnections()` | Close all SSE connections and reset polling client count. |

### Self-Registration (CachingWebUIProvider)

WebUIComponent is itself a provider, exposing:
- **`webui_uptime`** -- Header info context showing device uptime (1 s real-time refresh).
- **`webui_settings`** -- Settings context for theme, primary color, authentication, username, password.

---

## IWebUIProvider

**Header**: `DomoticsCore/IWebUIProvider.h`
**Namespace**: `DomoticsCore::Components`

Abstract interface that any component can implement to contribute UI elements.

### Pure Virtual Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `forEachContext` | `void forEachContext(std::function<bool(const WebUIContext&)> callback)` | Iterate contexts without copying. Return `false` from callback to stop. |
| `getContextCount` | `size_t getContextCount()` | Return the number of contexts. |
| `getContextAt` | `bool getContextAt(size_t index, WebUIContext& outContext)` | Copy context at `index` into `outContext`. Returns `false` if out of range. |
| `handleWebUIRequest` | `String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String,String>& params)` | Handle a UI action. Return JSON response. |
| `getWebUIName` | `String getWebUIName() const` | Display name for the provider. |
| `getWebUIVersion` | `String getWebUIVersion() const` | Version string. |
| `getWebUIContext` | `WebUIContext getWebUIContext(const String& contextId)` | Return a single context by ID. |

### Virtual Methods with Defaults

| Method | Signature | Default | Description |
|--------|-----------|---------|-------------|
| `getContextAtRef` | `const WebUIContext* getContextAtRef(size_t index)` | `nullptr` | Zero-copy reference to a cached context. |
| `getWebUIData` | `String getWebUIData(const String& contextId)` | `"{}"` | Return current real-time data as JSON. |
| `hasDataChanged` | `bool hasDataChanged(const String& contextId)` | `true` | Delta check for SSE broadcasts. Override to skip unchanged data. |
| `isWebUIEnabled` | `bool isWebUIEnabled()` | `true` | Return `false` to hide from the UI. |

---

## CachingWebUIProvider

**Header**: `DomoticsCore/IWebUIProvider.h`
**Inherits**: `IWebUIProvider`

Base class that caches contexts after the first build, preventing repeated heap allocations on ESP8266.

### Protected

| Method | Signature | Description |
|--------|-----------|-------------|
| `buildContexts` | `virtual void buildContexts(std::vector<WebUIContext>& contexts) = 0` | Subclasses populate contexts here. Called once, then cached. |
| `ensureContextsCached` | `void ensureContextsCached() const` | Lazy-init trigger. |

### Public

| Method | Signature | Description |
|--------|-----------|-------------|
| `invalidateContextCache` | `void invalidateContextCache()` | Clear the cache so the next access rebuilds. |

All `IWebUIProvider` methods are implemented using the internal cache, including `getContextAtRef()` which returns a direct pointer into the cached vector.

---

## WebUIContext

**Header**: `DomoticsCore/IWebUIProvider.h`

Describes where and how a piece of component UI appears in the dashboard.

### Data Members

| Member | Type | Description |
|--------|------|-------------|
| `contextId` / `contextIdPtr` | `String` / `const char*` | Unique identifier (hybrid storage). |
| `title` / `titlePtr` | `String` / `const char*` | Display title. |
| `icon` / `iconPtr` | `String` / `const char*` | Icon class name. |
| `location` | `WebUILocation` | Where to display (Dashboard, HeaderStatus, Settings, etc.). |
| `presentation` | `WebUIPresentation` | How to display (Card, Gauge, Graph, StatusBadge, etc.). |
| `priority` | `int` | Sort order (higher = displayed first). Default `0`. |
| `fields` | `std::vector<WebUIField>` | The fields in this context. |
| `apiEndpoint` / `apiEndpointPtr` | `String` / `const char*` | Optional API endpoint. |
| `realTime` | `bool` | Enable real-time updates. Default `false`. |
| `updateInterval` | `int` | Update interval in ms. Default `5000`. |
| `alwaysInteractive` | `bool` | If true, controls remain enabled even when Settings lock is active. |
| `customHtml` / `customHtmlPtr` | `String` / `const char*` | Custom HTML snippet (hybrid). |
| `customCss` / `customCssPtr` | `String` / `const char*` | Custom CSS snippet (hybrid). |
| `customJs` / `customJsPtr` | `String` / `const char*` | Custom JS snippet (hybrid). |
| `contextConfig` | `std::unique_ptr<JsonDocument>` | Optional custom JSON configuration. |

### Fluent Builder Methods

```cpp
WebUIContext& withField(const WebUIField& field);
WebUIContext& withAPI(const char* ep);
WebUIContext& withAPI(const String& ep);
WebUIContext& withRealTime(int interval = 5000);
WebUIContext& withAlwaysInteractive(bool interactive = true);
WebUIContext& withPriority(int p);
WebUIContext& configure(const String& key, const JsonVariant& value);
WebUIContext& withCustomHtml(const char* html);      // static/PROGMEM
WebUIContext& withCustomCss(const char* css);         // static/PROGMEM
WebUIContext& withCustomJs(const char* js);           // static/PROGMEM
WebUIContext& withCustomHtmlDynamic(const String& html);  // heap
WebUIContext& withCustomCssDynamic(const String& css);    // heap
WebUIContext& withCustomJsDynamic(const String& js);      // heap
```

### Factory Methods

Each factory has both `const char*` and `String` overloads:

| Factory | Location | Presentation | Default Icon |
|---------|----------|-------------|--------------|
| `dashboard(id, title, icon)` | Dashboard | Card | `fas fa-tachometer-alt` |
| `gauge(id, title, icon)` | Dashboard | Gauge | `fas fa-gauge` |
| `statusBadge(id, title, icon)` | HeaderStatus | StatusBadge | `dc-info` |
| `headerInfo(id, label, icon)` | HeaderInfo | Text | `dc-info` |
| `graph(id, title, icon)` | ComponentDetail | Graph | `dc-chart` |
| `quickControl(id, title, icon)` | QuickControls | Toggle | `dc-settings` |
| `settings(id, title, icon)` | Settings | Card | `dc-cog` |

### Hybrid String Accessors

| Accessor | Returns |
|----------|---------|
| `getContextIdCStr()` | `contextIdPtr` if non-null, else `contextId.c_str()` |
| `getTitleCStr()` | `titlePtr` if non-null, else `title.c_str()` |
| `getIconCStr()` | `iconPtr` if non-null, else `icon.c_str()` |
| `getApiEndpointCStr()` | `apiEndpointPtr` if non-null, else `apiEndpoint.c_str()` |
| `hasCustomHtml()` | `true` if either pointer or String is set |
| `getCustomHtmlCStr()` | Pointer or String content |

---

## WebUIField

**Header**: `DomoticsCore/IWebUIProvider.h`

Describes a single data field or control within a context.

### Data Members

| Member | Type | Description |
|--------|------|-------------|
| `name` / `namePtr` | `String` / `const char*` | Field identifier (hybrid). |
| `label` / `labelPtr` | `String` / `const char*` | Display label (hybrid). |
| `type` | `WebUIFieldType` | Field type enum. |
| `value` / `valuePtr` | `String` / `const char*` | Default value (hybrid). |
| `unit` / `unitPtr` | `String` / `const char*` | Unit of measurement (hybrid). |
| `readOnly` | `bool` | Read-only flag. |
| `minValue` | `float` | Minimum for sliders/numbers. Default `0`. |
| `maxValue` | `float` | Maximum for sliders/numbers. Default `100`. |
| `options` | `std::vector<String>` | Option values for Select fields. |
| `optionLabels` | `std::map<String, String>` | Value-to-label mapping for options. |
| `endpoint` / `endpointPtr` | `String` / `const char*` | API endpoint for field updates (hybrid). |
| `config` | `std::unique_ptr<JsonDocument>` | Optional custom JSON configuration. |

### Constructors

```cpp
// const char* -- stores pointers, zero heap allocation
WebUIField(const char* name, const char* label, WebUIFieldType type,
           const char* value = "", const char* unit = "", bool readOnly = false);

// String -- stores in heap String members
WebUIField(const String& name, const String& label, WebUIFieldType type,
           const String& value = "", const String& unit = "", bool readOnly = false);
```

### Fluent Methods

```cpp
WebUIField& range(float min, float max);
WebUIField& choices(const std::vector<String>& opts);
WebUIField& addOption(const String& value, const String& label);
WebUIField& api(const char* endpoint);
WebUIField& configure(const String& key, const JsonVariant& val);
```

---

## Enumerations

### WebUILocation

| Value | Description |
|-------|-------------|
| `Dashboard` | Main dashboard overview |
| `ComponentDetail` | Detailed component view |
| `HeaderStatus` | Top-right status indicators |
| `QuickControls` | Sidebar quick actions |
| `Settings` | Settings / configuration area |
| `HeaderInfo` | Main header info zone (time, uptime) |

### WebUIPresentation

| Value | Description |
|-------|-------------|
| `Card` | Standard card layout |
| `Gauge` | Circular gauge / meter |
| `Graph` | Time-series chart |
| `StatusBadge` | Small status indicator |
| `ProgressBar` | Progress / percentage bar |
| `Table` | Tabular data |
| `Toggle` | On/off switch |
| `Slider` | Range control |
| `Text` | Simple text display |
| `Button` | Action button |

### WebUIFieldType

| Value | Description |
|-------|-------------|
| `Text` | Text input / display |
| `Number` | Number input / display |
| `Float` | Float input / display |
| `Boolean` | Checkbox / toggle |
| `Select` | Dropdown selection |
| `Slider` | Range slider |
| `Color` | Color picker |
| `Button` | Action button |
| `Display` | Read-only display |
| `Chart` | Chart data (auto-rendered with history) |
| `Status` | Status indicator |
| `Progress` | Progress value |
| `Password` | Password input |
| `File` | File upload input |

---

## LazyState\<T\>

**Header**: `DomoticsCore/IWebUIProvider.h`

Template helper for change-tracking in providers.

```cpp
LazyState<bool> connectedState;

// In hasDataChanged():
return connectedState.hasChanged(wifi->isConnected());
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `get` | `T& get(std::function<T()> initializer)` | Lazy-init on first access. |
| `hasChanged` | `bool hasChanged(const T& current)` | Returns `true` on first call or when value differs. Updates stored value. |
| `getValue` | `const T& getValue() const` | Return stored value. |
| `isInitialized` | `bool isInitialized() const` | Check if state has been initialized. |
| `reset` | `void reset()` | Reset to uninitialized. |

---

## BaseWebUIComponents

**Header**: `DomoticsCore/BaseWebUIComponents.h`
**Namespace**: `DomoticsCore::Components::WebUI`

Static class providing reusable HTML widget generators. All methods are `static` and return `String` (HTML fragments).

### Widget Generators

| Method | Signature | Description |
|--------|-----------|-------------|
| `progressBar` | `static String progressBar(const String& id, const String& label = "", bool showPercentage = true)` | Progress bar with optional label and percentage text. |
| `toggleSwitch` | `static String toggleSwitch(const String& id, const String& label, bool checked = false)` | Toggle switch (checkbox styled as slider). |
| `button` | `static String button(const String& id, const String& text, bool isPrimary = false)` | Button element. `isPrimary=true` uses `btn btn-primary` class. |
| `textInput` | `static String textInput(const String& id, const String& label, const String& placeholder = "", const String& value = "")` | Text input with label. |
| `rangeSlider` | `static String rangeSlider(const String& id, const String& label, int min, int max, int value, int step = 1)` | Range slider input. |
| `selectDropdown` | `static String selectDropdown(const String& id, const String& label, const String* options, int optionCount, int selectedIndex = 0)` | Select dropdown. Options use `"value\|label"` format. |
| `fieldRow` | `static String fieldRow(const String& label, const String& valueId, const String& initialValue = "")` | Display-only label + value row. |
| `fileInput` | `static String fileInput(const String& inputId, const String& buttonId, const String& labelId, const String& label, const String& buttonText = "Select File", const String& accept = ".bin,.bin.gz")` | File input with styled button. |
| `buttonRow` | `static String buttonRow(const String& content)` | Container row for grouping buttons. |
| `radioGroup` | `static String radioGroup(const String& name, const String& label, const String* options, int optionCount, int selectedIndex = 0)` | Radio button group. Options use `"value\|label"` format. |

### Chart Generator

```cpp
static WebUIContext createLineChart(
    const String& contextId,
    const String& title,
    const String& canvasId,
    const String& valueId,
    const String& color = "#007acc",
    const String& unit = "%"
);
```

Returns a full `WebUIContext` (location: Dashboard, presentation: Card) containing custom HTML, CSS, and JavaScript for a real-time scrolling line chart. The CSS is stored in a `static const char[] PROGMEM` to avoid heap allocation.

---

## ProviderRegistry

**Header**: `DomoticsCore/WebUI/ProviderRegistry.h`
**Namespace**: `DomoticsCore::Components::WebUI`

Manages the mapping between context IDs and their owning providers.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `registerProvider` | `void registerProvider(IWebUIProvider*)` | Index all contexts from a provider. Uses `forEachContext()` to avoid copies. |
| `registerProviderWithComponent` | `void registerProviderWithComponent(IWebUIProvider*, IComponent*)` | Register and associate a component for lifecycle callbacks. |
| `unregisterProvider` | `void unregisterProvider(IWebUIProvider*)` | Remove all context entries for a provider. |
| `registerProviderFactory` | `void registerProviderFactory(const String& typeKey, std::function<IWebUIProvider*(IComponent*)> factory)` | Register a factory for composition-based providers. |
| `discoverProviders` | `void discoverProviders(const ComponentRegistry&)` | Walk all components, register those with providers or matching factories. Stops if heap falls below `MIN_HEAP_FOR_DISCOVERY` (2048 bytes). |
| `getProviderForContext` | `IWebUIProvider* getProviderForContext(const String& contextId)` | Look up the provider owning a context ID. |
| `hasContext` | `bool hasContext(const String& contextId) const` | Check if a context ID is registered. |
| `getContextProviders` | `const std::map<String, IWebUIProvider*>& getContextProviders() const` | Direct access to the context-to-provider map. |
| `getComponentsList` | `void getComponentsList(JsonDocument& doc)` | Populate a JSON array of all registered providers with name, version, enabled status. |
| `enableComponent` | `EnableResult enableComponent(const String& name, bool enabled)` | Enable or disable a provider. Triggers component `begin()`/`shutdown()`. Returns `EnableResult` struct. |
| `prepareSchemaGeneration` | `std::shared_ptr<SchemaChunkState> prepareSchemaGeneration()` | Create a shared state object for chunked schema streaming. |
| `handleComponentRemoved` | `void handleComponentRemoved(IComponent*)` | Clean up all providers associated with a removed component. |

### SchemaChunkState (Inner Struct)

Holds the state for chunked HTTP schema responses:

| Member | Type | Description |
|--------|------|-------------|
| `providers` | `std::vector<IWebUIProvider*>` | Unique list of providers to serialize. |
| `providerIndex` | `size_t` | Current provider being processed. |
| `contextIndexInProvider` | `size_t` | Current context index within provider. |
| `began` / `finished` | `bool` | Tracks `[` and `]` delimiters. |
| `currentContextPtr` | `const WebUIContext*` | Zero-copy pointer to the context being serialized. |
| `serializer` | `StreamingContextSerializer` | The streaming serializer instance. |

### EnableResult (Inner Struct)

| Member | Type | Description |
|--------|------|-------------|
| `success` | `bool` | Whether the operation succeeded. |
| `name` | `String` | Provider name. |
| `enabled` | `bool` | New enabled state. |
| `warning` | `String` | Warning message (e.g., disabling WebUI itself). |
| `found` | `bool` | Whether the provider was found. |

---

## StreamingContextSerializer

**Header**: `DomoticsCore/WebUI/StreamingContextSerializer.h`
**Namespace**: `DomoticsCore::Components::WebUI`

State machine that serializes a `WebUIContext` to JSON, writing directly to a byte buffer. Supports pause/resume: if the buffer fills, the serializer remembers its position and continues from there on the next `write()` call.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `begin` | `void begin(const WebUIContext& context)` | Start serializing. The context reference must remain valid until `isComplete()`. |
| `isComplete` | `bool isComplete() const` | Returns `true` when the entire JSON object has been written. |
| `write` | `size_t write(uint8_t* buffer, size_t maxLen)` | Write as many bytes as possible. Returns bytes written. |
| `getTotalBytesWritten` | `size_t getTotalBytesWritten() const` | Cumulative bytes across all `write()` calls. |
| `getChunkCount` | `size_t getChunkCount() const` | Number of `write()` calls that produced output. |

### State Machine

The serializer walks through states in order: `OpenBrace` -> `ContextId` -> `Title` -> `Icon` -> `Location` -> `Presentation` -> `Priority` -> `ApiEndpoint` -> `AlwaysInteractive` -> optional `CustomHtml/Css/Js` -> `Fields` array (each field has its own sub-state machine) -> `CloseBrace` -> `Complete`.

All JSON string values are properly escaped (quotes, backslashes, control characters). Large custom HTML/CSS/JS strings are streamed incrementally.

---

## WebSocketHandler

**Header**: `DomoticsCore/WebUI/WebSocketHandler.h`
**Namespace**: `DomoticsCore::Components::WebUI`

Manages real-time server-to-client communication. Dual-mode, decided at runtime by available heap:

- **SSE mode** (heap >= 20KB): Creates an `AsyncEventSource` at `/api/ui/events`.
- **Polling mode** (heap < 20KB): Clients poll `GET /api/ui/updates` periodically.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `begin` | `void begin(AsyncWebServer* server)` | Initialize. Creates SSE source if heap allows. |
| `loop` | `void loop()` | Expire stale polling clients (10 s timeout). |
| `onPollRequest` | `void onPollRequest()` | Called when a poll request arrives. Increments client count and triggers force update. |
| `isSSEEnabled` | `bool isSSEEnabled() const` | `true` if SSE mode is active. |
| `getClientCount` | `int getClientCount() const` | Total SSE + polling clients. |
| `getSSEClientCount` | `int getSSEClientCount() const` | SSE-only client count. |
| `notifyWiFiNetworkChanged` | `void notifyWiFiNetworkChanged()` | Send `wifi_network_changed` event via SSE. |
| `closeAllConnections` | `void closeAllConnections()` | Close all SSE connections and reset polling count. |
| `broadcastSchemaChange` | `void broadcastSchemaChange(const String& componentName)` | Send `schema_changed` event to SSE clients. |
| `broadcast` | `void broadcast(const String& message)` | Broadcast a string message via SSE. |
| `broadcast` | `void broadcast(const char* buffer, size_t len)` | Broadcast a buffer via SSE. |
| `shouldSendUpdates` | `bool shouldSendUpdates()` | Returns `true` when the configured interval has elapsed and clients are connected. |
| `setUIActionCallback` | `void setUIActionCallback(UIActionCallback cb)` | Set the callback for client-to-server UI actions. |
| `setForceUpdateCallback` | `void setForceUpdateCallback(std::function<void()> cb)` | Set the callback to force a full update. |

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `SSE_HEAP_THRESHOLD` | `20000` | Minimum free heap to enable SSE mode. |

---

## WebServerManager

**Header**: `DomoticsCore/WebUI/WebServerManager.h`
**Namespace**: `DomoticsCore::Components::WebUI`

Manages the `AsyncWebServer` instance and static asset serving.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `begin` | `void begin()` | Create the `AsyncWebServer` on the configured port and set up static routes. |
| `start` | `void start()` | Call `server->begin()` to start listening. |
| `stop` | `void stop()` | Call `server->end()`. |
| `getServer` | `AsyncWebServer* getServer()` | Return the raw server pointer. |
| `setAuthHandler` | `void setAuthHandler(std::function<bool(AsyncWebServerRequest*)> handler)` | Set the authentication handler. |
| `registerRoute` | `void registerRoute(const String& uri, WebRequestMethod method, ArRequestHandlerFunction handler)` | Register an HTTP route. |
| `registerChunkedRoute` | `void registerChunkedRoute(const String& uri, WebRequestMethod method, std::function<void(AsyncWebServerRequest*)> handler)` | Register a route for chunked responses. |
| `registerUploadRoute` | `void registerUploadRoute(const String& uri, ArRequestHandlerFunction handler, ArUploadHandlerFunction uploadHandler)` | Register a POST upload route. |

### Static Asset Serving

The `setupStaticRoutes()` method registers three routes:

| Route | Content | Behavior |
|-------|---------|----------|
| `/` | `index.html` | Heap check: if low memory, serves combined gzip; otherwise serves HTML-only gzip or filesystem. |
| `/style.css` | CSS | Served from PROGMEM gzip or filesystem. |
| `/app.js` | JavaScript | Served from PROGMEM gzip or filesystem. |

---

## WebUIConfig

**Header**: `DomoticsCore/WebUI/WebUIConfig.h`
**Namespace**: `DomoticsCore::Components::WebUI`

Configuration struct using fixed-size `char[]` arrays to avoid heap fragmentation. Re-exported as `DomoticsCore::Components::WebUIConfig` for backward compatibility.

### Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `deviceName` | `char[32]` | `"DomoticsCore Device"` | Display name. |
| `theme` | `char[8]` | `"auto"` | Theme: `"dark"`, `"light"`, or `"auto"`. |
| `port` | `uint16_t` | `80` | HTTP listen port. |
| `enableWebSocket` | `bool` | `true` | Enable SSE/polling real-time updates. |
| `wsUpdateInterval` | `int` | `5000` | SSE broadcast interval in ms. |
| `useFileSystem` | `bool` | `false` | Serve assets from filesystem instead of PROGMEM. |
| `staticPath` | `char[16]` | `"/webui"` | Filesystem path prefix. |
| `primaryColor` | `char[8]` | `"#007acc"` | UI accent color. |
| `enableAuth` | `bool` | `false` | Enable HTTP basic authentication. |
| `username` | `char[32]` | `"admin"` | Auth username. |
| `password` | `char[48]` | `""` | Auth password. |
| `maxWebSocketClients` | `int` | `3` | Maximum simultaneous clients (adapted at runtime by MemoryManager). |
| `apiTimeout` | `int` | `5000` | API request timeout in ms. |
| `enableCompression` | `bool` | `true` | Enable gzip content encoding. |
| `enableCaching` | `bool` | `true` | Enable HTTP cache headers. |
| `enableCORS` | `bool` | `false` | Enable CORS headers on all responses. |

### Setters

All setters use safe `strncpy` with null-termination and log a warning if the value is truncated:

```cpp
void setDeviceName(const char* v);
void setTheme(const char* v);
void setStaticPath(const char* v);
void setPrimaryColor(const char* v);
void setUsername(const char* v);
void setPassword(const char* v);
```

---

## HAL Layer

**Header**: `DomoticsCore/WebUI_HAL.h`

Platform routing header that includes the correct platform-specific file:

| Platform | File | WS Buffer | Max Providers | Max WS Clients |
|----------|------|-----------|---------------|-----------------|
| ESP32 | `WebUI_ESP32.h` | 8192 B | 32 | 8 |
| ESP8266 | `WebUI_ESP8266.h` | 1024 B | 8 | 2 |
| Native/Stub | `WebUI_Stub.h` | 4096 B | 32 | 8 |

All constants (`WEBUI_WS_BUFFER_SIZE`, `WEBUI_MAX_PROVIDERS`, `WEBUI_MAX_WS_CLIENTS`) can be overridden via compiler defines.

### WebResponse_HAL

**Header**: `DomoticsCore/WebUI/WebResponse_HAL.h`
**Namespace**: `DomoticsCore::HAL::WebResponse`

| Function | Signature | Description |
|----------|-----------|-------------|
| `createProgmemResponse` | `AsyncWebServerResponse* createProgmemResponse(request, code, contentType, data, len)` | Create a response from PROGMEM data. Uses `beginResponse_P()` on ESP8266. |
| `sendGzipResponse` | `void sendGzipResponse(request, contentType, data, len, cacheSeconds = 3600)` | Send gzipped PROGMEM data with `Content-Encoding: gzip` and `Cache-Control`. |

---

## Embedded Assets

**Script**: `DomoticsCore-WebUI/embed_webui.py`
**Output**: `DomoticsCore/Generated/WebUIAssets.h`

The build pre-script minifies and gzip-compresses the frontend source files (`webui_src/index.html`, `webui_src/style.css`, `webui_src/app.js`) into PROGMEM byte arrays:

| Symbol | Description |
|--------|-------------|
| `WEBUI_HTML_GZ` / `WEBUI_HTML_GZ_LEN` | Gzipped HTML (~3.5 KB) |
| `WEBUI_CSS_GZ` / `WEBUI_CSS_GZ_LEN` | Gzipped CSS (~2.4 KB) |
| `WEBUI_JS_GZ` / `WEBUI_JS_GZ_LEN` | Gzipped JS (~9 KB) |
| `WEBUI_COMBINED_GZ` / `WEBUI_COMBINED_GZ_LEN` | Combined HTML+CSS+JS for low-memory mode |

The combined mode inlines CSS and JS into the HTML before compressing, avoiding multiple HTTP connections that can exhaust heap on ESP8266 in AP+STA mode.

---

## REST API Endpoints (Detail)

### `GET /api/ui/schema`

Chunked response streaming the full JSON schema array for all enabled providers. Uses `StreamingContextSerializer` to avoid heap allocation.

### `GET /api/ui/updates`

Returns JSON with system info and all context data:
```json
{
  "system": { "uptime": 12345, "heap": 45000, "clients": 2, "device_name": "My Device" },
  "contexts": { "context_id_1": { ... }, "context_id_2": { ... } }
}
```
Add `?schema=1` to receive the full schema instead of data updates.

### `GET /api/ui/action`

Query params: `contextId`, `field`, `value`. Routes the action to the owning provider's `handleWebUIRequest()`.

### `GET /api/ui/context?id=X`

Returns the full schema for a single context.

### `SSE /api/ui/events`

Server-Sent Events stream. Events use type `"message"`. Special events: `schema_changed`, `wifi_network_changed`.

### `GET /api/components`

Returns a JSON array of all registered providers with name, version, enabled/disabled status.

### `POST /api/components/enable`

POST body: `name=ComponentName&enabled=true`. Triggers component lifecycle callbacks.

### `GET /api/system/info`

Lightweight JSON: `{ "uptime": ms, "heap": bytes, "clients": count }`.

---

## Known Deviations

The following files exceed the Constitution VII 800-line file size limit. These are flagged as known deviations and tracked as roadmap items for future refactoring.

| File | Lines | Status |
|------|-------|--------|
| `WebUI.h` | 950 | Planned split into smaller units (server setup, API routes, self-provider) |
| `StreamingContextSerializer.h` | 921 | Planned split (state machine is inherently sequential, but field sub-states could be extracted) |
| `Wifi.h` | 881 | Belongs to DomoticsCore-Wifi; tracked here as cross-component awareness per Constitution VII |

These deviations are accepted under Constitution VII's roadmap provision. Refactoring is planned but deferred to avoid destabilizing the streaming serializer's pause/resume logic and the tightly coupled WebUI server setup.

---

## Resolved (v2.0.1): SSE Broadcast Log Severity

Fixed in v2.0.1: SSE broadcast log level changed from `DLOG_W` to `DLOG_D` (DEBUG). Routine broadcasts no longer pollute WARNING-level output. The low-heap guard (line 933) still correctly uses `DLOG_W`.
