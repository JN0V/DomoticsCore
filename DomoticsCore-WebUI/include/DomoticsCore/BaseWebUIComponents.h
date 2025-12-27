#pragma once

#include <DomoticsCore/Platform_HAL.h>
#include "DomoticsCore/IWebUIProvider.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * Base WebUI Components - Reusable UI elements
 * Provides common chart, gauge, switch implementations to avoid code duplication
 */
class BaseWebUIComponents {
public:
    /**
     * Generate a real-time line chart with scrolling data.
     * @param contextId Unique identifier used for WebSocket updates and DOM hooks.
     * @param title Card title displayed above the chart.
     * @param canvasId DOM ID of the `<canvas>` element used for drawing.
     * @param valueId DOM ID of the `<span>` displaying the current numeric value.
     * @param color CSS color of the plotted line (defaults to `#007acc`).
     * @param unit Unit suffix appended to the numeric value (defaults to `%`).
     * @return WebUI context containing HTML/CSS/JS snippets for the chart.
     */
    static WebUIContext createLineChart(const String& contextId, const String& title, 
                                      const String& canvasId, const String& valueId, 
                                      const String& color = "#007acc", const String& unit = "%") {
        return WebUIContext::dashboard(contextId, title, "dc-chart")
            .withField(WebUIField(contextId + "_data", title + " Data", WebUIFieldType::Chart))
            .withCustomHtml(generateChartHtml(title, canvasId, valueId, unit))
            .withCustomCss(generateChartCss())
            .withCustomJs(generateChartJs(canvasId, color, valueId, contextId));
    }

private:
    /**
     * @brief Build the HTML snippet for the chart card.
     */
    static String generateChartHtml(const String& title, const String& canvasId, 
                                  const String& valueId, const String& unit) {
        return String(R"(
            <div class="card-header">
                <h3 class="card-title">)") + title + R"(</h3>
            </div>
            <div class="card-content system-chart">
                <canvas id=")" + canvasId + R"(" width="300" height="150"></canvas>
                <div class="chart-info">
                    <span class="chart-value" id=")" + valueId + R"(">0)" + unit + R"(</span>
                    <span class="chart-label">Current</span>
                </div>
            </div>
        )";
    }

    /**
     * @brief Provide scoped CSS styles for the chart card.
     */
    static String generateChartCss() {
        return R"(
            .system-chart {
                position: relative;
                display: flex;
                flex-direction: column;
                align-items: center;
            }
            .system-chart canvas {
                max-width: 100%;
                height: auto;
                margin-bottom: 1rem;
                border-radius: 4px;
            }
            .chart-info {
                display: flex;
                flex-direction: column;
                align-items: center;
                gap: 0.25rem;
            }
            .chart-value {
                font-size: 1.5rem;
                font-weight: 600;
                color: var(--primary-color);
            }
            .chart-label {
                font-size: 0.9rem;
                color: var(--text-secondary);
            }
        )";
    }

    /**
     * @brief Produce JavaScript helper functions for rendering and updating the chart.
     */
    static String generateChartJs(const String& canvasId, const String& color, 
                                const String& valueId, const String& contextId) {
        return String(R"(
            // Enhanced chart drawing with proper scrolling
            function drawScrollingChart(canvasId, data, color, valueId) {
                const canvas = document.getElementById(canvasId);
                if (!canvas) return;
                
                const ctx = canvas.getContext('2d');
                const width = canvas.width;
                const height = canvas.height;
                
                // Clear canvas
                ctx.clearRect(0, 0, width, height);
                
                if (!data || data.length === 0) return;
                
                // Filter out zero values at the beginning for better visualization
                let validData = data.filter(val => val > 0);
                if (validData.length === 0) {
                    validData = data.slice(-5); // Show last 5 points even if zero
                }
                
                // Draw grid
                ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
                ctx.lineWidth = 1;
                for (let i = 0; i <= 4; i++) {
                    const y = (height / 4) * i;
                    ctx.beginPath();
                    ctx.moveTo(0, y);
                    ctx.lineTo(width, y);
                    ctx.stroke();
                }
                
                // Draw chart line (always show newest data on the right)
                ctx.strokeStyle = color;
                ctx.lineWidth = 2;
                ctx.beginPath();
                
                const stepX = width / Math.max(validData.length - 1, 1);
                const dataMax = Math.max(...validData, 1);
                // If values look like percentages, use fixed 0-100 scale for stability
                const maxValue = (dataMax <= 100 ? 100 : dataMax);
                
                for (let i = 0; i < validData.length; i++) {
                    const x = i * stepX;
                    const y = height - (validData[i] / maxValue) * height * 0.9; // 90% of height for padding
                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.stroke();
                
                // Fill area under curve
                const fillColor = color.includes('rgb') ? 
                    color.replace('rgb', 'rgba').replace(')', ', 0.2)') : 
                    color + '33'; // Add alpha if hex color
                ctx.fillStyle = fillColor;
                ctx.lineTo(width, height);
                ctx.lineTo(0, height);
                ctx.closePath();
                ctx.fill();
                
                // Update current value display
                const currentValue = validData[validData.length - 1] || 0;
                const valueEl = document.getElementById(valueId);
                if (valueEl) {
                    valueEl.textContent = currentValue.toFixed(1) + valueEl.textContent.slice(-1); // Keep unit
                }
            }
            
            // Update function for )") + contextId + R"(
            function update)" + contextId + R"(Chart() {
                const data = window.systemChartData?.)" + contextId + R"( || [];
                drawScrollingChart(')" + canvasId + R"(', data, ')" + color + R"(', ')" + valueId + R"(');
            }
            
            // Initialize chart
            setTimeout(update)" + contextId + R"(Chart, 100);
        )";
    }

public:

    // ========== HTML Element Generators ==========

    /**
     * Generate a progress bar with optional label
     * @param id DOM ID for the progress bar fill element
     * @param label Optional label text (empty for no label)
     * @param showPercentage If true, shows percentage text on the right
     * @return HTML string for progress bar
     */
    static String progressBar(const String& id, const String& label = "", bool showPercentage = true) {
        String html = "<div class=\"field-row\" style=\"flex-direction: column; gap: 0.25rem;\">";
        
        if (!label.isEmpty() || showPercentage) {
            html += "<div style=\"display: flex; justify-content: space-between; align-items: center;\">";
            if (!label.isEmpty()) {
                html += "<span class=\"field-label\">" + label + "</span>";
            }
            if (showPercentage) {
                html += String(R"(<span id=")") + id + R"(_text" style="font-size: 0.9em; color: var(--text-secondary);">0.0%</span>)";
            }
            html += R"(</div>)";
        }
        
        html += String(R"(<div class="progress-bar-container"><div id=")") + id + R"(" class="progress-bar-fill"></div></div>)";
        html += R"(</div>)";
        return html;
    }

    /**
     * Generate a toggle switch
     * @param id DOM ID for the checkbox input
     * @param label Label text displayed next to switch
     * @param checked Initial state (default false)
     * @return HTML string for toggle switch
     */
    static String toggleSwitch(const String& id, const String& label, bool checked = false) {
        String html = R"(<div class="field-row">)";
        html += String(R"(<span class="field-label">)") + label + R"(</span>)";
        html += String(R"(<label class="toggle-switch"><input type="checkbox" id=")") + id + R"(")";
        if (checked) html += R"( checked)";
        html += R"(><span class="slider"></span></label>)";
        html += R"(</div>)";
        return html;
    }

    /**
     * Generate a button
     * @param id DOM ID for the button
     * @param text Button text
     * @param isPrimary If true, uses primary button style
     * @return HTML string for button
     */
    static String button(const String& id, const String& text, bool isPrimary = false) {
        String cssClass = isPrimary ? "btn btn-primary" : "btn";
        return "<button class=\"" + cssClass + "\" id=\"" + id + "\">" + text + "</button>";
    }

    /**
     * Generate a text input field
     * @param id DOM ID for the input
     * @param label Label text
     * @param placeholder Placeholder text
     * @param value Initial value (default empty)
     * @return HTML string for text input
     */
    static String textInput(const String& id, const String& label, const String& placeholder = "", const String& value = "") {
        String html = R"(<div class="field-row">)";
        html += String(R"(<span class="field-label">)") + label + R"(</span>)";
        html += String(R"(<input type="text" class="field-input" id=")") + id + R"(")";
        if (!placeholder.isEmpty()) html += String(R"( placeholder=")") + placeholder + R"(")";
        if (!value.isEmpty()) html += String(R"( value=")") + value + R"(")";
        html += R"(>)";
        html += R"(</div>)";
        return html;
    }

    /**
     * Generate a range slider
     * @param id DOM ID for the range input
     * @param label Label text
     * @param min Minimum value
     * @param max Maximum value
     * @param value Initial value
     * @param step Step increment (default 1)
     * @return HTML string for range slider
     */
    static String rangeSlider(const String& id, const String& label, int min, int max, int value, int step = 1) {
        String html = "<div class=\"field-row\">";
        html += "<span class=\"field-label\">" + label + "</span>";
        html += "<input type=\"range\" class=\"field-input\" id=\"" + id + "\" min=\"" + String(min) + 
                "\" max=\"" + String(max) + "\" value=\"" + String(value) + "\" step=\"" + String(step) + "\">";
        html += "</div>";
        return html;
    }

    /**
     * Generate a select dropdown
     * @param id DOM ID for the select element
     * @param label Label text
     * @param options Array of option values/labels (format: "value|label" or just "value")
     * @param optionCount Number of options in array
     * @param selectedIndex Index of initially selected option (default 0)
     * @return HTML string for select dropdown
     */
    static String selectDropdown(const String& id, const String& label, const String* options, int optionCount, int selectedIndex = 0) {
        String html = "<div class=\"field-row\">";
        html += "<span class=\"field-label\">" + label + "</span>";
        html += "<select class=\"field-input\" id=\"" + id + "\">";
        
        for (int i = 0; i < optionCount; i++) {
            String opt = options[i];
            int pipeIdx = opt.indexOf('|');
            String value = pipeIdx >= 0 ? opt.substring(0, pipeIdx) : opt;
            String text = pipeIdx >= 0 ? opt.substring(pipeIdx + 1) : opt;
            
            html += "<option value=\"" + value + "\"";
            if (i == selectedIndex) html += " selected";
            html += ">" + text + "</option>";
        }
        
        html += "</select></div>";
        return html;
    }

    /**
     * Generate a field row with label and value
     * @param label Label text
     * @param valueId DOM ID for the value span
     * @param initialValue Initial value text (default empty)
     * @return HTML string for field row
     */
    static String fieldRow(const String& label, const String& valueId, const String& initialValue = "") {
        return "<div class=\"field-row\"><span class=\"field-label\">" + label + 
               "</span><span class=\"field-value\" id=\"" + valueId + "\">" + initialValue + "</span></div>";
    }

    /**
     * Generate a file input with custom button
     * @param inputId DOM ID for the hidden file input
     * @param buttonId DOM ID for the visible button
     * @param labelId DOM ID for the file name label
     * @param label Label text
     * @param buttonText Button text (default "Select File")
     * @param accept File types to accept (default ".bin,.bin.gz")
     * @return HTML string for file input
     */
    static String fileInput(const String& inputId, const String& buttonId, const String& labelId, 
                           const String& label, const String& buttonText = "Select File", 
                           const String& accept = ".bin,.bin.gz") {
        String html = "<div class=\"field-row\">";
        html += "<span class=\"field-label\">" + label + "</span>";
        html += "<div style=\"display: flex; gap: 0.5rem; align-items: center;\">";
        html += "<input type=\"file\" id=\"" + inputId + "\" accept=\"" + accept + "\" style=\"display: none;\" />";
        html += "<button class=\"btn\" id=\"" + buttonId + "\">" + buttonText + "</button>";
        html += "<span id=\"" + labelId + "\" style=\"font-size: 0.9em; color: var(--text-secondary);\">No file selected</span>";
        html += "</div></div>";
        return html;
    }

    /**
     * Generate a button row (for grouping buttons)
     * @param content Button HTML content (use button() helper to create)
     * @return HTML string for button row
     */
    static String buttonRow(const String& content) {
        return "<div class=\"field-row\" style=\"display: flex; gap: 0.5rem;\">" + content + "</div>";
    }

    /**
     * Generate a radio button group
     * @param name Name attribute for the radio group (all radios share this)
     * @param label Label text for the group
     * @param options Array of option values/labels (format: "value|label" or just "value")
     * @param optionCount Number of options in array
     * @param selectedIndex Index of initially selected option (default 0)
     * @return HTML string for radio group
     */
    static String radioGroup(const String& name, const String& label, const String* options, int optionCount, int selectedIndex = 0) {
        String html = "<div class=\"field-row\" style=\"margin-bottom: 1rem;\">";
        html += "<span class=\"field-label\">" + label + "</span>";
        html += "<div style=\"display: flex; gap: 1rem;\">";
        
        for (int i = 0; i < optionCount; i++) {
            String opt = options[i];
            int pipeIdx = opt.indexOf('|');
            String value = pipeIdx >= 0 ? opt.substring(0, pipeIdx) : opt;
            String text = pipeIdx >= 0 ? opt.substring(pipeIdx + 1) : opt;
            
            html += "<label style=\"display: flex; align-items: center; gap: 0.25rem; cursor: pointer;\">";
            html += "<input type=\"radio\" name=\"" + name + "\" value=\"" + value + "\" id=\"" + name + "_" + value + "\"";
            if (i == selectedIndex) html += " checked";
            html += ">";
            html += "<span>" + text + "</span>";
            html += "</label>";
        }
        
        html += "</div></div>";
        return html;
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
