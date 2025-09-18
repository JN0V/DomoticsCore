#pragma once

#include <Arduino.h>

namespace DomoticsCore {
namespace Components {

class WebUIContent {
public:
    static String getHTML() {
        return "<!DOCTYPE html>\n"
               "<html lang=\"en\">\n"
               "<head>\n"
               "    <meta charset=\"UTF-8\">\n"
               "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
               "    <title>DomoticsCore Dashboard</title>\n"
               "    <link rel=\"stylesheet\" href=\"/style.css\">\n"
               "</head>\n"
               "<body>\n"
               "    <div class=\"app-container\">\n"
               "        <header class=\"header\">\n"
               "            <div class=\"header-left\">\n"
               "                <button class=\"menu-toggle\" id=\"menuToggle\"><i class=\"fas fa-bars\"></i></button>\n"
               "                <h1 class=\"device-name\" id=\"deviceName\">DomoticsCore Device</h1>\n"
               "            </div>\n"
               "            <div class=\"header-center\">\n"
               "                <div class=\"datetime\" id=\"datetime\"></div>\n"
               "            </div>\n"
               "            <div class=\"header-right\">\n"
               "                <div class=\"status-indicators\">\n"
               "                    <div class=\"status-item\" id=\"wifiStatus\" title=\"WiFi Connection Status\">\n"
               "                        <i class=\"fas fa-wifi\"></i><span class=\"status-text\">WiFi</span>\n"
               "                    </div>\n"
               "                    <div class=\"status-item\" id=\"wsStatus\" title=\"WebSocket Real-time Connection\">\n"
               "                        <i class=\"fas fa-plug\"></i><span class=\"status-text\">Live</span>\n"
               "                    </div>\n"
               "                </div>\n"
               "            </div>\n"
               "        </header>\n"
               "        <aside class=\"sidebar\" id=\"sidebar\">\n"
               "            <nav class=\"sidebar-nav\">\n"
               "                <ul class=\"nav-list\">\n"
               "                    <li class=\"nav-item active\">\n"
               "                        <a href=\"#dashboard\" class=\"nav-link\" data-section=\"dashboard\">\n"
               "                            <i class=\"fas fa-tachometer-alt\"></i><span>Dashboard</span>\n"
               "                        </a>\n"
               "                    </li>\n"
               "                    <li class=\"nav-item\">\n"
               "                        <a href=\"#components\" class=\"nav-link\" data-section=\"components\">\n"
               "                            <i class=\"fas fa-microchip\"></i><span>Components</span>\n"
               "                        </a>\n"
               "                    </li>\n"
               "                    <li class=\"nav-item\">\n"
               "                        <a href=\"#settings\" class=\"nav-link\" data-section=\"settings\">\n"
               "                            <i class=\"fas fa-cog\"></i><span>Settings</span>\n"
               "                        </a>\n"
               "                    </li>\n"
               "                </ul>\n"
               "            </nav>\n"
               "        </aside>\n"
               "        <main class=\"main-content\" id=\"mainContent\">\n"
               "            <div class=\"content-wrapper\">\n"
               "                <section id=\"dashboard-section\" class=\"content-section active\">\n"
               "                    <h2>System Dashboard</h2>\n"
               "                    <div class=\"dashboard-grid\">\n"
               "                        <div class=\"system-card\">\n"
               "                            <h3><i class=\"fas fa-info-circle\"></i> System Information</h3>\n"
               "                            <div class=\"info-grid\">\n"
               "                                <div class=\"info-item\">\n"
               "                                    <label>Device Name:</label>\n"
               "                                    <span id=\"deviceNameInfo\">Loading...</span>\n"
               "                                </div>\n"
               "                                <div class=\"info-item\">\n"
               "                                    <label>Uptime:</label>\n"
               "                                    <span id=\"uptime\">Loading...</span>\n"
               "                                </div>\n"
               "                                <div class=\"info-item\">\n"
               "                                    <label>Free Memory:</label>\n"
               "                                    <span id=\"freeHeap\">Loading...</span>\n"
               "                                </div>\n"
               "                                <div class=\"info-item\">\n"
               "                                    <label>WiFi Status:</label>\n"
               "                                    <span id=\"wifiStatus\">Loading...</span>\n"
               "                                </div>\n"
               "                            </div>\n"
               "                        </div>\n"
               "                    </div>\n"
               "                </section>\n"
               "                <section id=\"components-section\" class=\"content-section\">\n"
               "                    <h2>Hardware Components</h2>\n"
               "                    <div id=\"devicesContainer\" class=\"components-grid\">\n"
               "                        <div class=\"loading-message\">Loading components...</div>\n"
               "                    </div>\n"
               "                </section>\n"
               "                <section id=\"settings-section\" class=\"content-section\">\n"
               "                    <h2>System Settings</h2>\n"
               "                    <div id=\"settingsContainer\" class=\"settings-grid\">\n"
               "                        <div class=\"loading-message\">Loading settings...</div>\n"
               "                    </div>\n"
               "                </section>\n"
               "            </div>\n"
               "        </main>\n"
               "        <footer class=\"footer\">\n"
               "            <div class=\"footer-content\">\n"
               "                <span id=\"manufacturer\">DomoticsCore</span>\n"
               "                <span class=\"separator\">|</span>\n"
               "                <span id=\"version\">v1.0.0</span>\n"
               "                <span class=\"separator\">|</span>\n"
               "                <span id=\"copyright\">&copy; 2024 DomoticsCore</span>\n"
               "            </div>\n"
               "        </footer>\n"
               "    </div>\n"
               "    <script src=\"/app.js\"></script>\n"
               "</body>\n"
               "</html>";
    }
    
    static String getCSS() {
        return R"(:root {
    --bg-primary: #1a1a1a;
    --bg-secondary: #2d2d2d;
    --bg-tertiary: #3a3a3a;
    --text-primary: #ffffff;
    --text-secondary: #b0b0b0;
    --accent-primary: #007acc;
    --success: #28a745;
    --warning: #ffc107;
    --danger: #dc3545;
    --border: #404040;
    --shadow: rgba(0, 0, 0, 0.3);
    --sidebar-width: 250px;
    --header-height: 60px;
    --footer-height: 40px;
}
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background-color: var(--bg-primary);
    color: var(--text-primary);
    line-height: 1.6;
}
.app-container {
    display: grid;
    grid-template-areas: "header header" "sidebar main" "footer footer";
    grid-template-rows: var(--header-height) 1fr var(--footer-height);
    grid-template-columns: var(--sidebar-width) 1fr;
    min-height: 100vh;
}
.header {
    grid-area: header;
    background-color: var(--bg-secondary);
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 1rem;
    box-shadow: 0 2px 4px var(--shadow);
}
.header-left { display: flex; align-items: center; gap: 1rem; }
.menu-toggle {
    background: none; border: none; color: var(--text-primary);
    font-size: 1.2rem; cursor: pointer; padding: 0.5rem;
    border-radius: 4px; display: none;
}
.device-name { font-size: 1.2rem; font-weight: 600; }
.header-center { flex: 1; display: flex; justify-content: center; }
.datetime { font-size: 0.9rem; color: var(--text-secondary); font-family: monospace; }
.status-indicators { display: flex; gap: 1rem; }
.status-item {
    display: flex; align-items: center; gap: 0.5rem;
    padding: 0.25rem 0.5rem; border-radius: 4px;
    font-size: 0.8rem; cursor: pointer;
}
.status-item.online { color: var(--success); }
.status-item.offline { color: var(--danger); }
.sidebar {
    grid-area: sidebar; background-color: var(--bg-secondary);
    border-right: 1px solid var(--border); overflow-y: auto;
}
.nav-list { list-style: none; padding: 1rem 0; }
.nav-link {
    display: flex; align-items: center; gap: 0.75rem;
    padding: 0.75rem 1rem; color: var(--text-secondary);
    text-decoration: none; transition: all 0.2s;
    border-left: 3px solid transparent;
}
.nav-link:hover { background-color: var(--bg-tertiary); color: var(--text-primary); }
.nav-item.active .nav-link {
    background-color: var(--bg-tertiary); color: var(--accent-primary);
    border-left-color: var(--accent-primary);
}
.main-content {
    grid-area: main; background-color: var(--bg-primary);
    overflow-y: auto; padding: 1.5rem;
}
.content-section { display: none; }
.content-section.active { display: block; }
.content-section h2 { margin-bottom: 1.5rem; font-size: 1.8rem; }
.dashboard-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}
.dashboard-card {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px; padding: 1.5rem;
    box-shadow: 0 2px 8px var(--shadow);
}
.dashboard-card h3 {
    margin-bottom: 1rem; display: flex;
    align-items: center; gap: 0.5rem;
}
.info-grid { display: grid; gap: 0.75rem; }
.info-item {
    display: flex; justify-content: space-between;
    align-items: center; padding: 0.5rem;
    background-color: var(--bg-tertiary); border-radius: 4px;
}
.info-item .label { color: var(--text-secondary); font-size: 0.9rem; }
.info-item .value { color: var(--text-primary); font-weight: 500; font-family: monospace; }
.devices-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
    gap: 1.5rem;
}
.device-card {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px; padding: 1.5rem;
}
.form-group { margin-bottom: 1rem; }
.form-label {
    display: block; margin-bottom: 0.5rem;
    color: var(--text-secondary); font-size: 0.9rem;
}
.form-control {
    width: 100%; padding: 0.75rem;
    background-color: var(--bg-tertiary);
    border: 1px solid var(--border);
    border-radius: 4px; color: var(--text-primary);
}
.btn {
    padding: 0.75rem 1rem; border: none;
    border-radius: 4px; cursor: pointer;
    font-size: 0.9rem; display: inline-flex;
    align-items: center; gap: 0.5rem;
}
.btn-primary { background-color: var(--accent-primary); color: white; }
.btn-warning { background-color: var(--warning); color: black; }
.system-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}
.system-card {
    background-color: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px; padding: 1.5rem;
}
.footer {
    grid-area: footer; background-color: var(--bg-secondary);
    border-top: 1px solid var(--border);
    display: flex; align-items: center; justify-content: center;
}
.footer-content {
    display: flex; align-items: center; gap: 1rem;
    color: var(--text-secondary); font-size: 0.8rem;
}
.loading-overlay {
    position: fixed; top: 0; left: 0; width: 100%; height: 100%;
    background-color: rgba(26, 26, 26, 0.8);
    display: flex; align-items: center; justify-content: center;
    z-index: 9999;
}
.loading-spinner { text-align: center; }
.loading-spinner i { font-size: 2rem; color: var(--accent-primary); }
@media (max-width: 768px) {
    .app-container {
        grid-template-areas: "header" "main" "footer";
        grid-template-columns: 1fr;
    }
    .sidebar {
        position: fixed; top: var(--header-height);
        left: -var(--sidebar-width); width: var(--sidebar-width);
        height: calc(100vh - var(--header-height) - var(--footer-height));
        z-index: 1000; transition: left 0.3s ease;
    }
    .sidebar.open { left: 0; }
    .menu-toggle { display: block; }
    .dashboard-grid, .devices-grid, .system-grid { grid-template-columns: 1fr; }
    .header-center { display: none; }
    .status-text { display: none; }
}
.components-grid, .settings-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 1rem; }
.device-card { background: var(--bg-secondary); border-radius: 8px; padding: 1.5rem; border: 1px solid var(--border); }
.device-card h3 { margin-bottom: 1rem; color: var(--accent-primary); display: flex; align-items: center; gap: 0.5rem; }
.device-fields { display: flex; flex-direction: column; gap: 1rem; }
.form-group { display: flex; flex-direction: column; gap: 0.25rem; }
.form-label { font-size: 0.9rem; color: var(--text-secondary); font-weight: 500; }
.form-control { padding: 0.5rem; background: var(--bg-tertiary); border: 1px solid var(--border); border-radius: 4px; color: var(--text-primary); }
.form-control:focus { outline: none; border-color: var(--accent-primary); }
.form-control:read-only { opacity: 0.7; cursor: not-allowed; }
.loading-message { text-align: center; padding: 2rem; color: var(--text-secondary); }
.wifi-status { font-weight: 500; }
.wifi-connected { color: var(--success); }
.wifi-ap { color: var(--warning); }
.wifi-disconnected { color: var(--danger); }
.content-section { display: none; }
.content-section.active { display: block; })";
    }
    
    static String getJS() {
        return R"(class DomoticsWebUI {
    constructor() {
        this.ws = null;
        this.components = {};
        this.currentSection = 'dashboard';
        this.init();
    }
    
    init() {
        this.setupEventListeners();
        this.updateDateTime();
        this.loadSystemInfo();
        this.loadComponents();
        this.connectWebSocket();
        this.hideLoadingOverlay();
        setInterval(() => this.updateDateTime(), 1000);
    }
    
    setupEventListeners() {
        document.querySelectorAll('.nav-link').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                this.showSection(link.dataset.section);
            });
        });
        
        const menuToggle = document.getElementById('menuToggle');
        const sidebar = document.getElementById('sidebar');
        menuToggle?.addEventListener('click', () => {
            sidebar.classList.toggle('open');
        });
    }
    
    showSection(sectionName) {
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.remove('active');
        });
        document.querySelector(`[data-section="${sectionName}"]`)?.parentElement.classList.add('active');
        
        document.querySelectorAll('.content-section').forEach(section => {
            section.classList.remove('active');
        });
        document.getElementById(`${sectionName}-section`)?.classList.add('active');
        
        this.currentSection = sectionName;
        this.loadSectionData(sectionName);
        document.getElementById('sidebar')?.classList.remove('open');
    }
    
    loadSectionData(section) {
        if (section === 'dashboard') {
            this.loadSystemInfo();
            this.loadComponents();
        }
        if (section === 'components') this.loadDevices();
        if (section === 'settings') this.loadSettings();
    }
    
    updateDateTime() {
        const now = new Date();
        const dateTimeStr = now.toLocaleString('en-US', {
            year: 'numeric', month: '2-digit', day: '2-digit',
            hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false
        });
        document.getElementById('datetime').textContent = dateTimeStr;
    }
    
    async loadSystemInfo() {
        try {
            const response = await fetch('/api/system/info');
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            const data = await response.json();
            
            // Update header and footer info
            const deviceNameEl = document.getElementById('deviceName');
            const manufacturerEl = document.getElementById('manufacturer');
            const versionEl = document.getElementById('version');
            const copyrightEl = document.getElementById('copyright');
            
            if (deviceNameEl) deviceNameEl.textContent = data.device_name || 'DomoticsCore Device';
            if (manufacturerEl) manufacturerEl.textContent = data.manufacturer || 'DomoticsCore';
            if (versionEl) versionEl.textContent = data.version || 'v1.0.0';
            if (copyrightEl) copyrightEl.textContent = data.copyright || '© 2024 DomoticsCore';
            
            // Update system info in dashboard
            this.updateSystemInfo(data);
        } catch (error) {
            console.error('Failed to load system info:', error);
            // Set default WiFi status if system info fails
            const wifiStatus = document.getElementById('wifiStatus');
            if (wifiStatus) {
                wifiStatus.textContent = '🔴 Connection Error';
                wifiStatus.className = 'wifi-status wifi-disconnected';
            }
        }
    }
    
    updateSystemInfo(data) {
        document.getElementById('uptime').textContent = this.formatUptime(data.uptime || 0);
        document.getElementById('freeHeap').textContent = this.formatBytes(data.free_heap || 0);
        document.getElementById('chipModel').textContent = data.chip_model || 'Unknown';
    }
    
    async loadComponents() {
        try {
            const response = await fetch('/api/components');
            const data = await response.json();
            
            this.components = {};
            data.components?.forEach(comp => {
                this.components[comp.id] = comp;
            });
            
            this.updateComponentsOverview();
        } catch (error) {
            console.error('Failed to load components:', error);
        }
    }
    
    updateComponentsOverview() {
        const container = document.getElementById('componentsOverview');
        if (!container) return;
        
        container.innerHTML = '';
        Object.values(this.components).forEach(comp => {
            const card = document.createElement('div');
            card.className = 'component-card';
            card.innerHTML = `
                <div style="display: flex; align-items: center; gap: 0.5rem; margin-bottom: 0.5rem;">
                    <i class="${comp.icon || 'fas fa-puzzle-piece'}"></i>
                    <span style="font-weight: 500;">${comp.title}</span>
                    <span style="margin-left: auto; color: var(--success);">●</span>
                </div>
                <div style="font-size: 0.8rem; color: var(--text-secondary);">${comp.category}</div>
            `;
            card.style.cssText = 'padding: 1rem; background: var(--bg-tertiary); border-radius: 4px; cursor: pointer; margin-bottom: 0.5rem;';
            card.addEventListener('click', () => {
                if (comp.category === 'devices') this.showSection('devices');
                else if (comp.category === 'settings') this.showSection('settings');
            });
            container.appendChild(card);
        });
    }
    
    async loadDevices() {
        const container = document.getElementById('devicesContainer');
        if (!container) return;
        
        container.innerHTML = '<div class="loading-message">Loading components...</div>';
        try {
            const response = await fetch('/api/webui/sections');
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            const sections = await response.json();
            
            container.innerHTML = '';
            if (sections && sections.length > 0) {
                sections.forEach(section => {
                    if (section.category === 'hardware' || section.category === 'devices') {
                        const card = this.createDeviceCard(section);
                        container.appendChild(card);
                    }
                });
            } else {
                container.innerHTML = '<div class="loading-message">No hardware components found</div>';
            }
        } catch (error) {
            console.error('Failed to load devices:', error);
            container.innerHTML = '<div class="loading-message">Failed to load components</div>';
        }
    }
    
    async loadSettings() {
        const container = document.getElementById('settingsContainer');
        if (!container) return;
        
        container.innerHTML = '<div class="loading-message">Loading settings...</div>';
        try {
            const response = await fetch('/api/webui/sections');
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            const sections = await response.json();
            
            container.innerHTML = '';
            if (sections && sections.length > 0) {
                sections.forEach(section => {
                    if (section.category === 'settings' || section.category === 'system') {
                        const card = this.createDeviceCard(section);
                        container.appendChild(card);
                    }
                });
            } else {
                container.innerHTML = '<div class="loading-message">No settings found</div>';
            }
        } catch (error) {
            console.error('Failed to load settings:', error);
            container.innerHTML = '<div class="loading-message">Failed to load settings</div>';
        }
    }
    
    createDeviceCard(section) {
        const card = document.createElement('div');
        card.className = 'device-card';
        card.innerHTML = `
            <h3><i class="${section.icon || 'fas fa-microchip'}"></i> ${section.title}</h3>
            <div class="device-fields" id="device-${section.id}"></div>
        `;
        
        const fieldsContainer = card.querySelector('.device-fields');
        section.fields?.forEach(field => {
            const fieldElement = this.createFieldElement(field, section.id);
            fieldsContainer.appendChild(fieldElement);
        });
        
        return card;
    }
    
    createFieldElement(field, sectionId) {
        const wrapper = document.createElement('div');
        wrapper.className = 'form-group';
        
        const label = document.createElement('label');
        label.className = 'form-label';
        label.textContent = field.label + (field.unit ? ` (${field.unit})` : '');
        
        let input = document.createElement('input');
        input.className = 'form-control';
        input.value = field.value || '';
        input.readOnly = field.readonly;
        
        if (field.type === 1) input.type = 'number';
        else if (field.type === 3) input.type = 'checkbox';
        else input.type = 'text';
        
        wrapper.appendChild(label);
        wrapper.appendChild(input);
        return wrapper;
    }
    
    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        
        this.ws = new WebSocket(wsUrl);
        
        this.ws.onopen = () => {
            console.log('WebSocket connected');
            this.updateWSStatus(true);
        };
        
        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleWebSocketData(data);
            } catch (error) {
                console.error('WebSocket message error:', error);
            }
        };
        
        this.ws.onclose = () => {
            console.log('WebSocket disconnected, reconnecting in 5s...');
            this.updateWSStatus(false);
            setTimeout(() => this.connectWebSocket(), 5000);
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.updateWSStatus(false);
        };
    }
    
    handleWebSocketData(data) {
        console.log('Processing WebSocket data:', data);
        if (data.system) {
            console.log('Updating system info:', data.system);
            this.updateSystemInfo(data.system);
        }
        if (data.components) {
            console.log('Updating component data:', data.components);
            // Update component data in real-time
            Object.keys(data.components).forEach(compId => {
                this.updateComponentData(compId, data.components[compId]);
            });
        }
    }
    
    updateSystemInfo(system) {
        console.log('updateSystemInfo called with:', system);
        // Update system information in dashboard
        const deviceName = document.getElementById('deviceNameInfo');
        const uptime = document.getElementById('uptime');
        const freeHeap = document.getElementById('freeHeap');
        const wifiStatus = document.getElementById('wifiStatus');
        
        console.log('Found elements:', {deviceName, uptime, freeHeap, wifiStatus});
        
        if (deviceName) {
            deviceName.textContent = system.device_name || 'Unknown Device';
            console.log('Updated device name to:', deviceName.textContent);
        }
        if (uptime) {
            uptime.textContent = this.formatUptime(system.uptime || 0);
            console.log('Updated uptime to:', uptime.textContent);
        }
        if (freeHeap) {
            const formattedHeap = this.formatBytes(system.free_heap || 0);
            freeHeap.textContent = formattedHeap;
            console.log('Updated free heap to:', formattedHeap, 'from raw value:', system.free_heap);
        }
        
        // Enhanced WiFi status with connection info and AP mode detection
        if (wifiStatus) {
            let statusText = '';
            let statusClass = '';
            
            if (system.wifi_connected) {
                statusText = `🟢 ${system.wifi_ssid || 'Connected'} (${system.wifi_rssi || 'N/A'} dBm)`;
                statusClass = 'wifi-connected';
            } else if (system.ap_mode) {
                statusText = `🔵 AP Mode: ${system.ap_ssid || 'DomoticsCore-AP'}`;
                statusClass = 'wifi-ap';
            } else {
                statusText = '🔴 Disconnected';
                statusClass = 'wifi-disconnected';
            }
            
            wifiStatus.textContent = statusText;
            wifiStatus.className = `wifi-status ${statusClass}`;
        }
        
        // Update footer info
        const manufacturer = document.getElementById('manufacturer');
        const version = document.getElementById('version');
        const copyright = document.getElementById('copyright');
        
        if (manufacturer) manufacturer.textContent = system.manufacturer || 'DomoticsCore';
        if (version) version.textContent = system.version || '1.0.0';
        if (copyright) copyright.textContent = system.copyright || '© 2024 DomoticsCore';
    }
    
    updateComponentData(compId, data) {
        const container = document.getElementById(`device-${compId}`);
        if (!container) return;
        
        // Update field values with real-time data
        Object.keys(data).forEach(key => {
            const input = container.querySelector(`[data-field="${key}"]`);
            if (input) {
                if (input.type === 'checkbox') {
                    input.checked = data[key];
                } else {
                    input.value = data[key];
                }
            }
        });
    }
    
    updateWSStatus(connected) {
        const wsStatus = document.getElementById('wsStatus');
        if (wsStatus) {
            wsStatus.className = `status-item ${connected ? 'online' : 'offline'}`;
        }
    }
    
    hideLoadingOverlay() {
        const overlay = document.getElementById('loadingOverlay');
        if (overlay) {
            overlay.style.display = 'none';
        }
    }
    
    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        return `${days}d ${hours}h ${minutes}m`;
    }
    
    formatBytes(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
        return (bytes / 1048576).toFixed(1) + ' MB';
    }
}

function restartDevice() {
    if (confirm('Are you sure you want to restart the device?')) {
        fetch('/api/system/restart', { method: 'POST' })
            .then(() => alert('Device restart initiated'))
            .catch(err => alert('Restart failed: ' + err));
    }
}

// Initialize WebUI when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.webui = new DomoticsWebUI();
});)";
    }
};

} // namespace Components
} // namespace DomoticsCore
