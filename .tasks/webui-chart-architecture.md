# WebUI Chart Architecture - Research & Implementation Plan

## Problem Statement

Currently, the WebUI system attempts to handle charts using `BaseWebUIComponents::createLineChart()` which embeds custom HTML/CSS/JS in each context. This approach has several issues:

1. **Memory overhead on ESP8266**: Custom HTML/JS strings consume precious RAM
2. **Complexity in backend**: Chart rendering logic mixed with data collection
3. **Inflexibility**: Each component must implement chart logic separately
4. **Broken implementation**: Charts don't render properly (custom JS not evaluated correctly)

## Current Architecture (Problematic)

```
Backend (SystemInfoWebUI.h)
├── Maintains chart data buffers (cpuHistory[], heapHistory[])
├── Updates chart buffers in updateChartData()
├── Serializes arrays to JSON strings
├── Uses BaseWebUIComponents::createLineChart()
│   └── Embeds ~2KB of HTML/CSS/JS per chart
└── Returns: {"cpu_chart_data": "[0,1,2,...]"}

Frontend (app.js)
├── Receives custom HTML/CSS/JS in context schema
├── Attempts to inject and evaluate custom code
└── Charts don't render (evaluation issues)
```

## Proposed Architecture (Clean Separation)

```
Backend (SystemInfoWebUI.h)
├── Send ONLY raw current values
├── No chart buffers, no chart logic
└── Returns: {"cpu_load": 12.5, "heap_free": 45000}

Frontend (app.js)
├── Detect numeric fields that should be charted
├── Maintain chart history buffers in JavaScript
├── Use lightweight chart library (Chart.js mini / custom canvas)
├── Auto-create charts for marked fields
└── Full control over visualization
```

## Benefits of Frontend-Based Charts

1. **ESP8266 memory savings**: No custom HTML/CSS/JS strings
2. **Single implementation**: Chart logic in one place (app.js)
3. **Flexibility**: Easy to change chart style/library without reflashing
4. **Performance**: JavaScript engines optimized for this
5. **Rich features**: Zoom, pan, legends, tooltips - all in JS

## Implementation Plan

### Phase 1: Frontend Chart System (app.js)

**File:** `DomoticsCore-WebUI/webui_src/app.js`

Add chart detection and rendering:

```javascript
class ChartManager {
    constructor() {
        this.charts = new Map(); // contextId -> Chart instance
        this.history = new Map(); // fieldId -> array of values
        this.maxPoints = 60; // 2 minutes at 2s refresh
    }

    // Detect fields that should be charted (marked with metadata)
    shouldChart(field) {
        return field.chartable === true ||
               field.type === 'Display' && field.unit === '%';
    }

    // Update history for a field
    updateHistory(fieldId, value) {
        if (!this.history.has(fieldId)) {
            this.history.set(fieldId, []);
        }
        const history = this.history.get(fieldId);

        // Parse numeric value (remove unit suffix)
        const numValue = parseFloat(value);
        if (isNaN(numValue)) return;

        history.push(numValue);
        if (history.length > this.maxPoints) {
            history.shift();
        }
    }

    // Create or update chart for a context
    renderChart(contextId, canvasId, data, options) {
        // Use simple canvas API or mini Chart.js
        // Implementation here...
    }
}
```

### Phase 2: Backend Simplification

**File:** `DomoticsCore-SystemInfo/include/DomoticsCore/SystemInfoWebUI.h`

Simplify to send only raw values:

```cpp
std::vector<WebUIContext> getWebUIContexts() override {
    // Static hardware info
    contexts.push_back(WebUIContext::dashboard("system_info", "Device Information")
        .withField(WebUIField("manufacturer", "Manufacturer", WebUIFieldType::Display, cfg.manufacturer, "", true))
        // ... other static fields
    );

    // Real-time metrics with chart hint
    auto metricsCtx = WebUIContext::dashboard("system_metrics", "System Metrics")
        .withField(WebUIField("cpu_load", "CPU Load", WebUIFieldType::Display)
            .withUnit("%")
            .withMetadata("chartable", "true")
            .withMetadata("chartColor", "#007acc"))
        .withField(WebUIField("heap_usage", "Memory Usage", WebUIFieldType::Display)
            .withUnit("%")
            .withMetadata("chartable", "true")
            .withMetadata("chartColor", "#e74c3c"))
        .withRealTime(2000);

    contexts.push_back(metricsCtx);
    return contexts;
}

String getWebUIData(const String& contextId) override {
    if (contextId == "system_metrics") {
        JsonDocument doc;
        doc["cpu_load"] = metrics.cpuLoad;  // Raw float, no formatting
        float heapPercent = ((float)(metrics.totalHeap - metrics.freeHeap) / metrics.totalHeap) * 100.0f;
        doc["heap_usage"] = heapPercent;    // Raw float
        String json;
        serializeJson(doc, json);
        return json;
    }
    return "{}";
}
```

### Phase 3: WebUIField Metadata Support

**File:** `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h`

Add metadata support to WebUIField:

```cpp
struct WebUIField {
    String name;
    String label;
    WebUIFieldType type;
    String value;
    String unit;
    bool readOnly;
    std::map<String, String> metadata;  // NEW: flexible metadata

    WebUIField& withMetadata(const String& key, const String& value) {
        metadata[key] = value;
        return *this;
    }
};
```

### Phase 4: Chart Rendering Options

**Option A: Custom Canvas (Lightweight, ~1KB)**
- Draw lines using Canvas 2D API
- Full control, minimal overhead
- Current implementation in BaseWebUIComponents is close

**Option B: Chart.js Custom Build (~15KB minified)**
- Professional-looking charts
- Features: tooltips, legends, responsive
- Might be too heavy for ESP8266

**Option C: Minimal SVG Charts (~3KB)**
- Use SVG for vector graphics
- Scales well, accessible
- Easier animations than Canvas

**Recommendation:** Start with Option A (custom canvas), already partially implemented in app.js

## Current Issue Analysis

**Update: Custom HTML/CSS/JS system DOES work!**

Investigation of `app.js` shows the system is properly implemented:
- **Line 128-129**: `customHtml` replaces card content
- **Line 1101-1107**: `customCss` injected into global `<style>` tag
- **Line 1124-1130**: `customJs` injected into global `<script>` tag

**So why don't BaseWebUIComponents charts work?**

Tested with ESP8266, charts don't render. Possible causes:

1. **Data format mismatch**: Chart JS expects `window.systemChartData[contextId]` but data might not be in correct format
2. **Update function not called**: `update${contextId}Chart()` functions generated but never invoked
3. **WebSocket data timing**: Charts initialized before first data arrives
4. **Field name mismatch**: Chart expects `cpu_chart_data` field but it's not in the context schema
5. **Canvas rendering issues**: Canvas might exist but drawing fails silently

**Key finding from BaseWebUIComponents.h:**
```cpp
// Chart expects field named: contextId + "_data"
WebUIContext::dashboard(contextId, title, "dc-chart")
    .withField(WebUIField(contextId + "_data", title + " Data", WebUIFieldType::Chart))
    .withCustomHtml(...) // Creates canvas
    .withCustomJs(...)   // Defines updateXXXChart() function
```

The chart field type is `Chart` (not `Display`), which might not be handled by `updateCharts()` in app.js!

**Root cause hypothesis:**
The `updateCharts()` function (line 1050) looks for fields ending with `_data`, but the field type is `Chart` which might be filtered out elsewhere in the update pipeline.

**Evidence from logs:**
```
Context cpu_chart JSON length: 1682
First 500 chars: {"contextId":"cpu_chart","title":"CPU Load","icon":"dc-chart",...,"customHtml":"...
```

Context is created and sent, custom HTML/JS/CSS should be injected. But no chart appears.

## Action Items

- [x] **Research**: Examine how app.js currently handles custom HTML/CSS/JS
  - ✅ Confirmed: system works, injects HTML/CSS/JS properly
  - ✅ Issue: Charts don't render despite injection
- [x] **Investigate**: Check if `WebUIFieldType::Chart` is handled in field rendering
  - ✅ Added Chart case in app.js renderField()
- [x] **Implement**: Chart rendering with canvas in app.js
  - ✅ updateChart() stores history (60 points max)
  - ✅ drawChart() renders with Canvas 2D API
  - ✅ Auto-detects Chart field type and renders automatically
- [x] **Test**: Verify charts work on ESP8266
  - ✅ Tested on ESP8266, charts render correctly
  - ✅ Memory usage: 58.5% RAM (acceptable)
- [ ] **Audit**: Review all component WebUI implementations for consistency
  - [ ] Check LED WebUI (if has metrics, should use Chart type)
  - [ ] Check NTP WebUI (if has metrics, should use Chart type)
  - [ ] Check MQTT WebUI (if has metrics, should use Chart type)
  - [ ] Check OTA WebUI (progress should use Progress type)
  - [ ] Check HomeAssistant WebUI (if has metrics, should use Chart type)
  - [ ] Verify all use raw numeric values, not formatted strings for Chart fields
  - [ ] Ensure consistent patterns: static info (Display), editable (Text/etc), metrics (Chart)
- [ ] **Cleanup**: Remove or deprecate BaseWebUIComponents chart methods
  - [ ] Mark `createLineChart()` as deprecated
  - [ ] Add comment directing to use `WebUIFieldType::Chart` instead
- [ ] **Test**: Verify charts work on ESP32
- [ ] **Document**: Update WebUI README with Chart field type usage examples

## Files to Investigate

1. `DomoticsCore-WebUI/webui_src/app.js` (lines ~1050-1075: current chart handling)
2. `DomoticsCore-WebUI/webui_src/app.js` (search for: customHtml, customCss, customJs)
3. `DomoticsCore-WebUI/include/DomoticsCore/IWebUIProvider.h` (WebUIContext structure)
4. `DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h` (current chart implementation)

## Related Issues

- ESP8266 memory constraints (64% RAM usage)
- WebSocket data refresh timing (2s intervals)
- Chart data persistence (browser-side only vs. device history)
- Multiple chart instances on same page

## Success Criteria

✅ Charts render properly in browser
✅ No custom HTML/CSS/JS strings in backend
✅ Backend only sends raw numeric values
✅ Charts auto-detect from field metadata
✅ Memory usage on ESP8266 stays under 70%
✅ Works on both ESP32 and ESP8266
✅ Easy to add charts to any component (just add metadata)

---

**Status:** Research phase
**Priority:** Medium (charts are nice-to-have, current display fields work)
**Complexity:** Medium (requires frontend + backend coordination)
