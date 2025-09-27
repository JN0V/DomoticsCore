#pragma once

#include <Arduino.h>
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
        return WebUIContext::dashboard(contextId, title, "fas fa-chart-line")
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
    /**
     * Generate a status badge for header display
     * @param contextId Context identifier
     * @param title Badge title
     * @param icon SVG icon reference
     * @return WebUI context for status badge
     */
    static WebUIContext createStatusBadge(const String& contextId, const String& title, const String& icon) {
        return WebUIContext::statusBadge(contextId, title, icon)
            .withCustomHtml(String(R"(<svg class="icon status-icon" viewBox="0 0 1024 1024"><use href="#)") + icon + R"("/></svg>)")
            .withCustomCss(R"(
                .status-icon {
                    transition: all 0.3s ease;
                }
                .status-indicator.active .status-icon {
                    color: var(--primary-color);
                }
            )");
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
