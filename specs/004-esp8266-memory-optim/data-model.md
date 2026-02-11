# Data Model: ESP8266 Memory Optimization

**Feature**: 004-esp8266-memory-optim
**Date**: 2025-01-20

---

## Entity Changes by Phase

### Phase 1: WebUIConfig

**Before** (current):
```
WebUIConfig {
  String deviceName      // heap-allocated, ~20 chars typical
  String theme           // heap-allocated, ~5 chars typical
  String staticPath      // heap-allocated, ~6 chars typical
  String primaryColor    // heap-allocated, ~7 chars typical
  String username        // heap-allocated, ~0-31 chars
  String password        // heap-allocated, ~0-47 chars
  uint16_t port = 80
  bool enableAuth = false
  uint8_t maxWebSocketClients = 3
  uint32_t wsUpdateInterval = 5000
}
```

**After**:
```
WebUIConfig {
  char deviceName[32]    // fixed 32 bytes
  char theme[8]          // fixed 8 bytes
  char staticPath[16]    // fixed 16 bytes
  char primaryColor[8]   // fixed 8 bytes
  char username[32]      // fixed 32 bytes
  char password[48]      // fixed 48 bytes
  uint16_t port = 80
  bool enableAuth = false
  uint8_t maxWebSocketClients = 3
  uint32_t wsUpdateInterval = 5000

  // Accessors
  + String getDeviceName() const
  + String getTheme() const
  + String getStaticPath() const
  + String getPrimaryColor() const
  + String getUsername() const
  + String getPassword() const
  + void setDeviceName(const char*)   // DLOG_W on truncation
  + void setTheme(const char*)
  + void setStaticPath(const char*)
  + void setPrimaryColor(const char*)
  + void setUsername(const char*)
  + void setPassword(const char*)
}
```

**Storage**: 144 bytes fixed stack/BSS vs ~72 bytes (6×SSO) + variable heap

### Phase 1: EventBus

**Before**:
```
EventBus {
  // In poll():
  auto handlers = itT->second;           // COPY of vector<Subscription>
}
```

**After**:
```
EventBus {
  + bool dispatching_ = false            // NEW: reentrancy guard

  // In poll():
  const auto& handlers = itT->second;    // REFERENCE, no copy

  // In subscribe()/unsubscribe():
  + assert(!dispatching_)                // debug-only guard
}
```

### Phase 1: CachingWebUIProvider

**Before**: No `getWebUIContext()` override → falls back to `IWebUIProvider::getWebUIContext()` which calls `getWebUIContexts()` → full vector copy.

**After**:
```
CachingWebUIProvider {
  // NEW override:
  + WebUIContext getWebUIContext(const String& contextId) override {
      ensureContextsCached();
      // iterate cachedContexts_ by reference, return copy of match
  }
}
```

---

### Phase 2: WebUIContext (custom content)

**Before**:
```
WebUIContext {
  String customHtml      // heap copy of static content
  String customCss       // heap copy of static content
  String customJs        // heap copy of static content
}
```

**After**:
```
WebUIContext {
  String customHtml              // kept for dynamic content only
  String customCss
  String customJs
  + const char* customHtmlPtr = nullptr   // Flash pointer (priority)
  + const char* customCssPtr = nullptr
  + const char* customJsPtr = nullptr

  // Builders:
  + withCustomHtml(const char*)           // sets Ptr, clears String
  + withCustomHtmlDynamic(const String&)  // sets String, clears Ptr
  + withCustomCss(const char*)
  + withCustomCssDynamic(const String&)
  + withCustomJs(const char*)
  + withCustomJsDynamic(const String&)

  // Accessors (for serializer):
  + bool hasCustomHtml() const    // Ptr != null || !String.isEmpty()
  + const char* getCustomHtmlCStr() const  // Ptr if set, else String.c_str()
  + size_t getCustomHtmlLen() const
}
```

**Invariant**: When `Ptr != nullptr`, corresponding `String` is empty. Never both set.

### Phase 2: IWebUIProvider (interface cleanup)

**Before**:
```
IWebUIProvider {
  virtual std::vector<WebUIContext> getWebUIContexts() = 0;  // pure virtual
  virtual void forEachContext(cb) { /* calls getWebUIContexts() */ }
  virtual size_t getContextCount() { /* calls getWebUIContexts() */ }
  virtual bool getContextAt(i, out) { /* calls getWebUIContexts() */ }
  virtual WebUIContext getWebUIContext(id) { /* calls getWebUIContexts() */ }
}
```

**After**:
```
IWebUIProvider {
  // REMOVED: getWebUIContexts()
  virtual void forEachContext(cb) = 0;                     // NOW pure virtual
  virtual size_t getContextCount() = 0;                    // NOW pure virtual
  virtual bool getContextAt(i, out) = 0;                   // NOW pure virtual
  virtual WebUIContext getWebUIContext(const String& id) = 0; // NOW pure virtual
  virtual const WebUIContext* getContextAtRef(i) { return nullptr; } // optional
}
```

**Note**: `CachingWebUIProvider` already implements all of these. Direct `IWebUIProvider` subclasses (only in tests) must be updated.

### Phase 2: ComponentMetadata

**Before**:
```
ComponentMetadata {
  String name
  String version
  String author
  String description
}
```

**After**:
```
ComponentMetadata {
  const char* name = ""
  const char* version = ""
  const char* author = ""
  const char* description = ""
}
```

### Phase 2: Dependency

**Before**: `String name`
**After**: `const char* name`

---

### Phase 3: WebUIField (hybrid storage)

**Before**:
```
WebUIField {
  String name
  String label
  String value
  String unit
  String endpoint
  // + type, readOnly, minValue, maxValue, options, optionLabels, config
}
```

**After**:
```
WebUIField {
  // Hybrid storage: pointer for literals, String for dynamic
  const char* namePtr = nullptr
  const char* labelPtr = nullptr
  const char* valuePtr = nullptr
  const char* unitPtr = nullptr
  const char* endpointPtr = nullptr

  String name            // used only when namePtr is null (dynamic value)
  String label
  String value
  String unit
  String endpoint

  // Constructor for literals (most common):
  WebUIField(const char* n, const char* l, WebUIFieldType t,
             const char* v = "", const char* u = "", bool ro = false)
    → stores pointers directly, Strings remain empty

  // Constructor for dynamic values:
  WebUIField(const String& n, const String& l, WebUIFieldType t,
             const String& v, const String& u = "", bool ro = false)
    → stores in String members, pointers remain null

  // Accessors:
  + const char* getNameCStr() const   // Ptr ?? name.c_str()
  + const char* getLabelCStr() const
  + const char* getValueCStr() const
  + const char* getUnitCStr() const
  + const char* getEndpointCStr() const
  + size_t getNameLen() const
  // ... same for other fields
}
```

### Phase 3: WebUIContext (identity fields)

**After** (extends Phase 2 changes):
```
WebUIContext {
  // Hybrid identity fields:
  + const char* contextIdPtr = nullptr
  + const char* titlePtr = nullptr
  + const char* iconPtr = nullptr
  + const char* apiEndpointPtr = nullptr

  String contextId       // dynamic fallback
  String title
  String icon
  String apiEndpoint

  // Factory methods store pointers:
  + static WebUIContext dashboard(const char* id, const char* title, const char* icon)
  + static WebUIContext settings(const char* id, const char* title, ...)
  // ... all factory methods use const char*

  // Accessors:
  + const char* getContextIdCStr() const
  + const char* getTitleCStr() const
  + const char* getIconCStr() const
  + const char* getApiEndpointCStr() const
}
```

---

## StreamingContextSerializer Changes

### Phase 2: Add writeJsonCString()

```
StreamingContextSerializer {
  // EXISTING:
  size_t writeJsonString(uint8_t* buf, size_t max, const String& str)

  // NEW:
  + size_t writeJsonCString(uint8_t* buf, size_t max, const char* str)
    // Same JSON-escaping logic, reads from const char* instead of String
    // Uses strlen() for length, direct char access via str[pos]

  // Modified states (Phase 2):
  CustomHtmlValue → check Ptr first, use writeJsonCString or writeJsonString
  CustomCssValue  → same
  CustomJsValue   → same

  // Modified states (Phase 3):
  ContextIdValue  → check Ptr, dispatch to appropriate write method
  TitleValue      → same
  // ... all field value states
}
```

---

## State Transitions

No complex state machines are introduced. The only state-related change is the `EventBus::dispatching_` flag:

```
dispatching_ = false (idle)
  → poll() called → dispatching_ = true
    → handler executed (subscribe/unsubscribe blocked by assert)
  → all handlers done → dispatching_ = false
```

---

## Validation Rules

| Entity | Rule | Phase |
|--------|------|-------|
| WebUIConfig.deviceName | max 31 chars, DLOG_W on truncation | P1 |
| WebUIConfig.theme | max 7 chars, must be "auto"/"dark"/"light" | P1 |
| WebUIConfig.primaryColor | max 7 chars, format "#RRGGBB" | P1 |
| WebUIContext.customHtmlPtr | if non-null, customHtml String must be empty | P2 |
| WebUIField.namePtr | if non-null, name String must be empty | P3 |
| Hybrid copy | copy preserves pointer vs String state | P3 |
| Hybrid assignment | assignment preserves pointer vs String state | P3 |
