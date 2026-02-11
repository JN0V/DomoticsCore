# Quickstart: ESP8266 Memory Optimization

**Feature**: 004-esp8266-memory-optim
**Date**: 2025-01-20

---

## Prerequisites

- PlatformIO CLI installed
- ESP8266 board (d1_mini) connected or available for testing
- Baseline captured in `baseline.md` before starting

## Build & Test Commands

```bash
# Build FullStack example for ESP8266 (primary target)
cd DomoticsCore-System/examples/FullStack
pio run -e esp8266dev

# Build Standard example for ESP8266
cd DomoticsCore-System/examples/Standard
pio run -e esp8266dev

# Run unit tests (native platform)
cd DomoticsCore-WebUI
pio test -e native

cd DomoticsCore-Core
pio test -e native

# Run specific test suite
cd DomoticsCore-WebUI
pio test -e native -f test_streaming_serializer
pio test -e native -f test_webui_component

# Build for ESP32 (compatibility check)
cd DomoticsCore-System/examples/FullStack
pio run -e esp32dev
```

## Phase 1 Execution Order

### Step 0: Capture Baseline

```bash
# Record build sizes
cd DomoticsCore-System/examples/FullStack
pio run -e esp8266dev 2>&1 | grep -E "RAM|Flash"
# Save output to specs/004-esp8266-memory-optim/baseline.md

cd DomoticsCore-System/examples/Standard
pio run -e esp8266dev 2>&1 | grep -E "RAM|Flash"
```

### Step 1: FR-P1.1 — WebUIConfig char[]

**Files to modify**:
1. `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h` — struct definition
2. `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h` — all `config.xxx` read/write sites
3. `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebSocketHandler.h` — config copy usage
4. `DomoticsCore-System/include/DomoticsCore/System.h` — config initialization
5. `DomoticsCore-System/include/DomoticsCore/SystemWebUISetup.h` — deviceName callback

**Pattern** (copy from existing MQTTConfig):
```cpp
struct WebUIConfig {
    char deviceName[32] = "DomoticsCore";
    // ...
    String getDeviceName() const { return String(deviceName); }
    void setDeviceName(const char* v) {
        if (strlen(v) >= sizeof(deviceName)) {
            DLOG_W("WebUI", "deviceName truncated: '%s' (%d > %d)", v, strlen(v), sizeof(deviceName)-1);
        }
        strncpy(deviceName, v, sizeof(deviceName));
        deviceName[sizeof(deviceName) - 1] = '\0';
    }
};
```

**Verify**:
```bash
pio run -e esp8266dev  # Check RAM % decreased
pio test -e native     # All tests pass
```

### Step 2: FR-P1.2 — EventBus const ref

**File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`

3 changes in `poll()` method (lines ~164, ~172, ~186):
```cpp
// Before:
auto handlers = itT->second;  // copy
// After:
const auto& handlers = itT->second;  // reference
```

Plus add `dispatching_` flag and debug asserts.

**Verify**:
```bash
cd DomoticsCore-Core && pio test -e native
```

### Step 3: FR-P1.3 — CachingWebUIProvider fix

**File**: `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`

Add override in `CachingWebUIProvider` class (~line 580):
```cpp
WebUIContext getWebUIContext(const String& contextId) override {
    ensureContextsCached();
    for (const auto& ctx : cachedContexts_) {
        if (ctx.contextId == contextId) return ctx;
    }
    return WebUIContext();
}
```

**Verify**:
```bash
cd DomoticsCore-WebUI && pio test -e native
```

### Phase 1 Gate Check

```bash
# All tests pass
cd DomoticsCore-WebUI && pio test -e native
cd DomoticsCore-Core && pio test -e native

# Build sizes
cd DomoticsCore-System/examples/FullStack && pio run -e esp8266dev
# Target: RAM ≤ 74%

# ESP32 compatibility
cd DomoticsCore-System/examples/FullStack && pio run -e esp32dev
# Target: no new warnings
```

## Phase 2 Execution Order

### Step 4: FR-P2.1 — Flash pointers in WebUIContext

**Files**:
1. `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h` — add Ptr members + builders
2. `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h` — add `writeJsonCString()` + conditional dispatch

### Step 5: FR-P2.2 — Migrate BaseWebUIComponents

**File**: `DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h`
- `generateChartCss()` → return `const char*`
- `createLineChart()` → use `withCustomCss(const char*)`

### Step 6: FR-P2.3 — ComponentMetadata const char*

**Files**:
1. `DomoticsCore-Core/include/DomoticsCore/IComponent.h` — struct changes
2. `DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h` — map insertion
3. All component headers — constructor updates (mechanical)

### Step 7: IWebUIProvider cleanup — Remove getWebUIContexts()

**Files**:
1. `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h` — remove method, make forEachContext pure virtual
2. `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp` — ~57 call sites
3. `DomoticsCore-WebUI/test/test_streaming_serializer/test/test_streaming_serializer.cpp` — ~6 call sites
4. `DomoticsCore-WebUI/test/test_schema_memory/test_schema_memory.cpp` — ~8 call sites

### Phase 2 Gate Check

Same as Phase 1 gate + target RAM ≤ 72%.

## Phase 3 Execution Order

### Step 8: FR-P3.1 — Hybrid WebUIField

**File**: `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- Add pointer members + constructors + accessors
- Update copy constructor and assignment operator

### Step 9: FR-P3.2 — Update StreamingContextSerializer

**File**: `DomoticsCore-WebUI/include/DomoticsCore/WebUI/StreamingContextSerializer.h`
- All field value states check pointer first

### Step 10: FR-P3.3 — WebUIContext identity fields hybrid

**File**: `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`
- Add Ptr members for contextId, title, icon, apiEndpoint
- Update factory methods
- Update copy/assignment

### Phase 3 Gate Check

Full test suite + runtime heap measurement ≥ 8 KB with 2 WS clients.

---

## Troubleshooting

**Build fails on ESP32 after changes**: Check that `const char*` constructors don't create ambiguity with existing `String` constructors. ESP32 toolchain may resolve overloads differently.

**Tests fail after getWebUIContexts() removal**: Most test code uses `auto contexts = provider.getWebUIContexts()`. Replace with:
```cpp
std::vector<WebUIContext> contexts;
provider.forEachContext([&](const WebUIContext& ctx) {
    contexts.push_back(ctx);
    return true;
});
```

**Dangling pointer crash**: If a `const char*` field points to freed memory, the device will crash with an exception. Always ensure pointers point to string literals (static storage duration). Use `withCustomHtmlDynamic()` for runtime-generated content.
