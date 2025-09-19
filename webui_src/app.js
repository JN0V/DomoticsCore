class DomoticsApp {
    constructor() {
        this.ws = null;
        this.wsReconnectInterval = null;
        this.systemStartTime = Date.now();
        this.uiSchema = [];

        // Enum mappings from C++ backend
        this.WebUILocation = { Dashboard: 0, ComponentDetail: 1, HeaderStatus: 2, QuickControls: 3, Settings: 4 };
        this.WebUIFieldType = { Text: 0, Number: 1, Float: 2, Boolean: 3, Select: 4, Slider: 5, Color: 6, Button: 7, Display: 8, Chart: 9, Status: 10, Progress: 11 };

        this.init();
    }

    async init() {
        this.setupEventListeners();
        this.setupWebSocket();
        this.updateDateTime();
        setInterval(() => this.updateDateTime(), 1000);
        await this.loadUISchema();
        this.renderUI();
    }

    async loadUISchema() {
        try {
            const response = await fetch('/api/ui/schema');
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            this.uiSchema = await response.json();
            console.log('UI Schema loaded:', this.uiSchema);
        } catch (error) {
            console.error('Failed to load UI schema:', error);
            // You could render an error message to the user here
        }
    }

    renderUI() {
        this.renderSection(this.WebUILocation.Dashboard, 'dashboardGrid');
        this.renderSection(this.WebUILocation.ComponentDetail, 'componentsGrid'); // Using ComponentDetail for the 'Components' tab for now
        this.renderSection(this.WebUILocation.Settings, 'settingsGrid');
    }

    renderSection(location, gridId) {
        const grid = document.getElementById(gridId);
        if (!grid) return;

        const contexts = this.uiSchema.filter(ctx => ctx.location === location);
        contexts.sort((a, b) => (b.priority || 0) - (a.priority || 0));

        if (contexts.length > 0) {
            grid.innerHTML = '';
            contexts.forEach(context => {
                const card = this.createContextCard(context);
                grid.appendChild(card);
            });
        } else {
            grid.innerHTML = '<div class="loading-message">No components for this section.</div>';
        }
    }

    createContextCard(context) {
        const card = document.createElement('div');
        card.className = 'card';
        card.dataset.contextId = context.contextId;

        let fieldsHtml = '';
        context.fields.forEach(field => {
            fieldsHtml += this.renderField(field);
        });

        card.innerHTML = `
            <div class="card-header">
                <h3 class="card-title">${context.title}</h3>
            </div>
            <div class="card-content">${fieldsHtml}</div>
        `;

        this.attachFieldEventListeners(card, context);
        return card;
    }

    renderField(field) {
        let fieldHtml = '';
        switch (field.type) {
            case this.WebUIFieldType.Boolean:
                fieldHtml = `
                    <label class="toggle-switch">
                        <input type="checkbox" id="${field.name}" ${field.value === 'true' ? 'checked' : ''} ${field.readOnly ? 'disabled' : ''}>
                        <span class="slider"></span>
                    </label>`;
                break;
            case this.WebUIFieldType.Slider:
                 fieldHtml = `<input type="range" id="${field.name}" min="${field.minValue}" max="${field.maxValue}" value="${field.value}" ${field.readOnly ? 'disabled' : ''}>`;
                 break;
            case this.WebUIFieldType.Display:
            default:
                // Add a data-attribute to uniquely identify the value span for WS updates
                fieldHtml = `<span class="field-value" data-field-name="${field.name}">${field.value} ${field.unit || ''}</span>`;
                break;
        }

        return `
            <div class="field-row">
                <span class="field-label">${field.label}:</span>
                ${fieldHtml}
            </div>`;
    }

    setupEventListeners() {
        const menuToggle = document.getElementById('menuToggle');
        const sidebar = document.getElementById('sidebar');
        const navLinks = document.querySelectorAll('.nav-link');

        if (menuToggle && sidebar) {
            menuToggle.addEventListener('click', () => sidebar.classList.toggle('open'));
        }

        navLinks.forEach(link => {
            link.addEventListener('click', e => {
                e.preventDefault();
                this.showSection(link.dataset.section);
                navLinks.forEach(l => l.classList.remove('active'));
                link.classList.add('active');
            });
        });

        document.addEventListener('click', e => {
            if (sidebar && menuToggle && !sidebar.contains(e.target) && !menuToggle.contains(e.target)) {
                sidebar.classList.remove('open');
            }
        });
    }

    setupWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
        const wsUrl = `${protocol}://${window.location.host}/ws`;
        this.ws = new WebSocket(wsUrl);

        this.ws.onopen = () => {
            console.log('WebSocket connected');
            this.updateConnectionStatus(true);
            this.clearReconnectInterval();
        };

        this.ws.onmessage = event => {
            try {
                const data = JSON.parse(event.data);
                this.handleWebSocketMessage(data);
            } catch (e) {
                console.error('Failed to parse WebSocket message:', e);
            }
        };

        this.ws.onclose = () => {
            console.log('WebSocket disconnected');
            this.updateConnectionStatus(false);
            this.scheduleReconnect();
        };

        this.ws.onerror = error => console.error('WebSocket error:', error);
    }

    scheduleReconnect() {
        if (this.wsReconnectInterval) return;
        this.wsReconnectInterval = setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.CLOSED) {
                this.setupWebSocket();
            }
        }, 5000);
    }

    clearReconnectInterval() {
        if (this.wsReconnectInterval) {
            clearInterval(this.wsReconnectInterval);
            this.wsReconnectInterval = null;
        }
    }

    handleWebSocketMessage(data) {
        if (data.system) {
            this.updateSystemInfo(data.system);
        }
        if (data.contexts) {
            this.updateDashboard(data.contexts);
        }
    }

    updateSystemInfo(system) {
        if (typeof system.uptime === 'number') {
            this.systemStartTime = Date.now() - system.uptime;
        }
        const deviceNameEl = document.getElementById('deviceName');
        if (deviceNameEl && system.device_name) deviceNameEl.textContent = system.device_name;

        const footerTextEl = document.getElementById('footerText');
        if (footerTextEl && system.manufacturer && system.version) {
            footerTextEl.textContent = `${system.manufacturer} ${system.version} | ESP32 Framework`;
        }
    }

    updateDashboard(contexts) {
        Object.entries(contexts).forEach(([contextId, data]) => {
            const contextSchema = this.uiSchema.find(ctx => ctx.contextId === contextId);
            if (!contextSchema) return;

            if (contextSchema.location === this.WebUILocation.HeaderStatus) {
                this.updateHeaderStatus(contextId, data);
            } else {
                const card = document.querySelector(`.card[data-context-id='${contextId}']`);
                if (card) {
                    Object.entries(data).forEach(([fieldName, value]) => {
                        const fieldSchema = contextSchema.fields.find(f => f.name === fieldName);
                        // Try to find an input element first (for toggles, sliders, etc.)
                        const inputEl = card.querySelector(`#${fieldName}`);
                        if (inputEl) {
                            if (inputEl.type === 'checkbox') {
                                inputEl.checked = (value === 'true' || value === true);
                            } else {
                                inputEl.value = value;
                            }
                        } else {
                            // Otherwise, find a display span
                            const displayEl = card.querySelector(`[data-field-name='${fieldName}']`);
                            if (displayEl) {
                                displayEl.textContent = this.formatValue(value) + (fieldSchema.unit || '');
                            }
                        }
                    });
                }
            }
        });
    }

    updateHeaderStatus(contextId, data) {
        const statusContainer = document.querySelector('.status-indicators');
        if (!statusContainer) return;

        let indicator = statusContainer.querySelector(`[data-context-id='${contextId}']`);
        if (!indicator) {
            const contextSchema = this.uiSchema.find(ctx => ctx.contextId === contextId);
            if (!contextSchema) return;

            indicator = document.createElement('span');
            indicator.className = 'status-indicator';
            indicator.dataset.contextId = contextId;
            indicator.innerHTML = `<svg class="icon" viewBox="0 0 24 24"><use href="#dc-components"/></svg>`; // Generic icon
            statusContainer.appendChild(indicator);
        }

        // Update status based on the first field's value
        const firstField = Object.keys(data)[0];
        if (firstField) {
            const value = data[firstField];
            indicator.classList.toggle('active', value === 'ON' || value === 'true' || value === true);
            indicator.title = `${contextId}: ${value}`;
        }
    }
    
    formatValue(value) {
        if (typeof value === 'boolean') return value ? 'On' : 'Off';
        if (typeof value === 'number' && value > 1_000_000) return (value / 1_000_000).toFixed(1) + 'M';
        if (typeof value === 'number' && value > 1_000) return (value / 1_000).toFixed(1) + 'K';
        return String(value);
    }

    updateConnectionStatus(connected) {
        const wsStatus = document.getElementById('wsStatus');
        if (wsStatus) wsStatus.classList.toggle('connected', connected);
    }

    updateDateTime() {
        const now = Date.now();
        const uptime = Math.floor((now - this.systemStartTime) / 1000);
        const hours = Math.floor(uptime / 3600);
        const minutes = Math.floor((uptime % 3600) / 60);
        const seconds = uptime % 60;
        const uptimeStr = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        
        const dt = document.getElementById('datetime');
        if (dt) dt.textContent = `Uptime: ${uptimeStr}`;
    }

    showSection(sectionName) {
        document.querySelectorAll('.content-section').forEach(section => section.classList.remove('active'));
        const targetSection = document.getElementById(`${sectionName}-section`);
        if (targetSection) targetSection.classList.add('active');

        if (sectionName === 'components') {
            this.loadComponents();
        }
    }

    async loadComponents() {
        const grid = document.getElementById('componentsGrid');
        if (!grid) return;
        grid.innerHTML = '<div class="loading-message">Loading components...</div>';

        try {
            const response = await fetch('/api/components');
            const data = await response.json();
            
            if (data.components && data.components.length > 0) {
                grid.innerHTML = '';
                const card = document.createElement('div');
                card.className = 'card';
                let fieldsHtml = '';
                data.components.forEach(comp => {
                    fieldsHtml += `
                        <div class="field-row">
                            <span class="field-label">${comp.name} (v${comp.version}):</span>
                            <span class="field-value status-${comp.status.toLowerCase()}">${comp.status}</span>
                        </div>`;
                });
                card.innerHTML = `
                    <div class="card-header">
                        <h3 class="card-title">Registered Components</h3>
                    </div>
                    <div class="card-content">${fieldsHtml}</div>
                `;
                grid.appendChild(card);
            } else {
                grid.innerHTML = '<div class="loading-message">No components registered.</div>';
            }
        } catch (error) {
            console.error('Failed to load components:', error);
            grid.innerHTML = '<div class="loading-message">Error loading components.</div>';
        }
    }

    attachFieldEventListeners(card, context) {
        context.fields.forEach(field => {
            if (field.readOnly) return;

            const el = card.querySelector(`#${field.name}`);
            if (!el) return;

            const eventType = (field.type === this.WebUIFieldType.Slider) ? 'input' : 'change';

            el.addEventListener(eventType, (e) => {
                let value = e.target.type === 'checkbox' ? e.target.checked : e.target.value;
                this.sendUICommand(context.contextId, field.name, value);
            });
        });
    }

    sendUICommand(contextId, fieldName, value) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            const command = {
                type: 'ui_action',
                contextId: contextId,
                field: fieldName,
                value: value
            };
            this.ws.send(JSON.stringify(command));
        } else {
            console.warn('WebSocket not connected. UI command not sent.');
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
  new DomoticsApp();
});
