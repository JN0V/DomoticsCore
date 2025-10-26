class DomoticsApp {
    constructor() {
        this.ws = null;
        this.wsReconnectInterval = null;
        this.uiSchema = [];
        this.isEditingDeviceName = false;

        // Enum mappings from C++ backend
        this.WebUILocation = { Dashboard: 0, ComponentDetail: 1, HeaderStatus: 2, QuickControls: 3, Settings: 4, HeaderInfo: 5 };
        this.WebUIFieldType = { Text: 0, Number: 1, Float: 2, Boolean: 3, Select: 4, Slider: 5, Color: 6, Button: 7, Display: 8, Chart: 9, Status: 10, Progress: 11, Password: 12 };

        this.init();
    }

    async init() {
        this.setupEventListeners();
        this.setupWebSocket();
        await this.loadUISchema();
        // Apply initial theme from schema if available
        this.applyThemeFromSchema();
        this.renderUI();
    }

    async loadUISchema() {
        try {
            const response = await fetch(`/api/ui/schema?t=${Date.now()}`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            this.uiSchema = await response.json();
            console.log('UI Schema loaded:', this.uiSchema);
            
            // Inject custom CSS and JS from components
            this.injectCustomStyles();
            this.injectCustomScripts();
        } catch (error) {
            console.error('Failed to load UI schema:', error);
            // You could render an error message to the user here
        }
    }

    renderUI() {
        this.renderSection(this.WebUILocation.Dashboard, 'dashboardGrid');
        this.renderSection(this.WebUILocation.ComponentDetail, 'componentsGrid'); // Using ComponentDetail for the 'Components' tab for now
        this.renderSection(this.WebUILocation.Settings, 'settingsGrid');
        this.renderHeaderStatus();
        this.renderHeaderInfo();
    }

    applyTheme(theme) {
        // theme: 'dark' | 'light' | 'auto'
        const root = document.documentElement;
        const body = document.body;
        const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
        let mode = theme || 'dark';
        if (mode === 'auto') mode = prefersDark ? 'dark' : 'light';
        body.classList.toggle('light', mode === 'light');
        body.classList.toggle('dark', mode === 'dark');
    }

    applyThemeFromSchema() {
        // Look for webui_settings context theme field default
        const settings = this.uiSchema.find(ctx => ctx.contextId === 'webui_settings');
        if (!settings) return;
        const themeField = settings.fields && settings.fields.find(f => f.name === 'theme');
        if (themeField && themeField.value) {
            this.applyTheme(themeField.value);
        }
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

        // Use custom HTML if provided, otherwise use default structure
        if (context.customHtml) {
            card.innerHTML = context.customHtml;
        } else {
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
        }

        this.attachFieldEventListeners(card, context);
        return card;
    }

    renderHeaderStatus() {
        const container = document.querySelector('.status-indicators');
        if (!container) return;
        // Clear all to match schema exactly (prevents stale badges after disable)
        container.innerHTML = '';
        const statuses = this.uiSchema
            .filter(ctx => ctx.location === this.WebUILocation.HeaderStatus)
            .sort((a, b) => (b.priority || 0) - (a.priority || 0));
        statuses.forEach(ctx => {
            const indicator = document.createElement('span');
            indicator.className = 'status-indicator';
            indicator.dataset.contextId = ctx.contextId;
            if (ctx.customHtml) {
                indicator.innerHTML = ctx.customHtml;
            } else {
                indicator.innerHTML = `<svg class="icon" viewBox="0 0 24 24"><use href="#dc-components"/></svg>`;
            }
            container.appendChild(indicator);
        });
    }

    renderHeaderInfo() {
        const container = document.querySelector('.header-info');
        if (!container) return;
        // Clear all to match schema exactly
        container.innerHTML = '';
        const infos = this.uiSchema
            .filter(ctx => ctx.location === this.WebUILocation.HeaderInfo)
            .sort((a, b) => (b.priority || 0) - (a.priority || 0));
        infos.forEach(ctx => {
            const infoItem = document.createElement('span');
            infoItem.className = 'header-info-item';
            infoItem.dataset.contextId = ctx.contextId;
            
            // Display first field value (time, uptime, etc.)
            const field = ctx.fields && ctx.fields.length > 0 ? ctx.fields[0] : null;
            if (field) {
                infoItem.innerHTML = `
                    <svg class="icon" viewBox="0 0 1024 1024"><use href="#${ctx.icon || 'dc-info'}"/></svg>
                    <span class="header-info-value" data-field="${field.name}">${field.value || '--'}</span>
                `;
            }
            container.appendChild(infoItem);
        });
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
            case this.WebUIFieldType.Text:
                {
                    const inputType = field.name === 'password' ? 'password' : 'text';
                    const val = (field.value != null) ? String(field.value) : '';
                    fieldHtml = `<input type="${inputType}" id="${field.name}" value="${val}" ${field.readOnly ? 'disabled' : ''}>`;
                }
                break;
            case this.WebUIFieldType.Password:
                {
                    const val = (field.value != null) ? String(field.value) : '';
                    fieldHtml = `<input type="password" id="${field.name}" value="${val}" ${field.readOnly ? 'disabled' : ''}>`;
                }
                break;
            case this.WebUIFieldType.Number:
                {
                    const val = (field.value != null) ? String(field.value) : '';
                    fieldHtml = `<input type="number" id="${field.name}" value="${val}" ${field.readOnly ? 'disabled' : ''}>`;
                }
                break;
            case this.WebUIFieldType.Float:
                {
                    const val = (field.value != null) ? String(field.value) : '';
                    fieldHtml = `<input type="number" step="any" id="${field.name}" value="${val}" ${field.readOnly ? 'disabled' : ''}>`;
                }
                break;
            case this.WebUIFieldType.Select:
                {
                    const options = (field.options || []).length ? field.options : (typeof field.unit === 'string' && field.unit.includes(',')) ? field.unit.split(',') : [];
                    const current = String(field.value || '');
                    const labels = field.optionLabels || {};
                    const optsHtml = options.map(opt => {
                        const sel = (String(opt) === current) ? 'selected' : '';
                        const label = labels[opt] || opt;  // Use label if available, fallback to value
                        return `<option value="${opt}" ${sel}>${label}</option>`;
                    }).join('');
                    fieldHtml = `<select id="${field.name}" ${field.readOnly ? 'disabled' : ''}>${optsHtml}</select>`;
                }
                break;
            case this.WebUIFieldType.Slider:
                {
                    const min = (field.minValue !== undefined && field.minValue !== null) ? field.minValue : 0;
                    const max = (field.maxValue !== undefined && field.maxValue !== null) ? field.maxValue : 255;
                    fieldHtml = `<input type="range" id="${field.name}" min="${min}" max="${max}" value="${field.value}" ${field.readOnly ? 'disabled' : ''}>`;
                }
                 break;
            case this.WebUIFieldType.Color:
                {
                    const val = (field.value != null) ? String(field.value) : '#000000';
                    fieldHtml = `<input type="color" id="${field.name}" value="${val}" ${field.readOnly ? 'disabled' : ''}>`;
                }
                break;
            case this.WebUIFieldType.Button:
                fieldHtml = `<button class="btn" id="${field.name}" ${field.readOnly ? 'disabled' : ''}>${field.label}</button>`;
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

        const deviceNameEl = document.getElementById('deviceName');
        if (deviceNameEl) {
            deviceNameEl.addEventListener('click', () => this.makeDeviceNameEditable(deviceNameEl));
        }
    }

    setupWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
        const wsUrl = `${protocol}://${window.location.host}/ws`;
        this.ws = new WebSocket(wsUrl);

        this.ws.onopen = () => {
            console.log('WebSocket connected');
            this.clearReconnectInterval();
            // After reconnect (e.g., AP -> AP+STA), refresh schema and UI to sync badges/cards
            this.loadUISchema().then(() => {
                this.renderUI();
            });
            // no polling/banner
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
            console.warn('WebSocket disconnected');
            this.scheduleReconnect();
        };

        this.ws.onerror = error => {
            console.error('WebSocket error:', error);
        };
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
        // Schema change notification from server: re-fetch schema and re-render
        if (data && data.type === 'schema_changed') {
            this.loadUISchema().then(() => {
                this.renderUI();
                // If the Components tab is currently active, refresh it as well
                const compSection = document.getElementById('components-section');
                if (compSection && compSection.classList.contains('active')) {
                    this.loadComponents();
                }
            });
            return;
        }
        if (data.system) {
            this.updateSystemInfo(data.system);
            this.updateSystemOverviewCard(data.system);
        }
        if (data.contexts) {
            this.updateDashboard(data.contexts);
            this.updateCharts(data.contexts);
        }
    }

    updateSystemOverviewCard(system) {
        const card = document.querySelector(`.card[data-context-id='system_overview']`);
        if (!card) return;

        const uptimeEl = card.querySelector(`[data-field-name='uptime']`);
        if (uptimeEl) {
            const uptime = Math.floor((Date.now() - this.systemStartTime) / 1000);
            const hours = Math.floor(uptime / 3600).toString().padStart(2, '0');
            const minutes = Math.floor((uptime % 3600) / 60).toString().padStart(2, '0');
            const seconds = (uptime % 60).toString().padStart(2, '0');
            uptimeEl.textContent = `${hours}:${minutes}:${seconds}`;
        }

        const heapEl = card.querySelector(`[data-field-name='heap']`);
        if (heapEl && typeof system.heap === 'number') {
            heapEl.textContent = `${(system.heap / 1024).toFixed(1)} KB`;
        }
    }

    updateSystemInfo(system) {
        if (typeof system.uptime === 'number') {
            this.systemStartTime = Date.now() - system.uptime;
        }
        const deviceNameEl = document.getElementById('deviceName');
        if (!this.isEditingDeviceName && deviceNameEl && system.device_name) {
            deviceNameEl.textContent = system.device_name;
        }

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
            } else if (contextSchema.location === this.WebUILocation.HeaderInfo) {
                this.updateHeaderInfo(contextId, data);
            } else {
                const card = document.querySelector(`.card[data-context-id='${contextId}']`);
                if (card) {
                    Object.entries(data).forEach(([fieldName, value]) => {
                        const fieldSchema = contextSchema.fields.find(f => f.name === fieldName);
                        // Try to find an input element first (for toggles, sliders, etc.)
                        const inputEl = card.querySelector(`#${fieldName}`);
                        if (inputEl) {
                            if (inputEl.type === 'checkbox') {
                                const newChecked = (value === 'true' || value === true);
                                if (inputEl.checked !== newChecked) {
                                    inputEl.checked = newChecked;
                                    // Trigger UI update listeners (e.g., custom JS updating bulbs)
                                    inputEl.dispatchEvent(new Event('change', { bubbles: true }));
                                }
                            } else {
                                // Do not overwrite if user is currently editing this input
                                if (document.activeElement === inputEl) return;
                                if (inputEl.value !== String(value)) {
                                    inputEl.value = value;
                                    // For sliders or other inputs, also trigger change to refresh visuals
                                    inputEl.dispatchEvent(new Event('change', { bubbles: true }));
                                }
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

            // Dispatch generic component status update event
            document.dispatchEvent(new CustomEvent('component-status-update', {
                detail: { contextId, data }
            }));
        });
    }

    updateHeaderInfo(contextId, data) {
        const container = document.querySelector('.header-info');
        if (!container) return;

        let infoItem = container.querySelector(`[data-context-id='${contextId}']`);
        if (!infoItem) return; // Item should already exist from renderHeaderInfo

        // Update field values
        Object.entries(data).forEach(([fieldName, value]) => {
            const valueEl = infoItem.querySelector(`[data-field='${fieldName}']`);
            if (valueEl) {
                valueEl.textContent = value || '--';
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
            // Use custom HTML if provided, otherwise use generic icon
            if (contextSchema.customHtml) {
                indicator.innerHTML = contextSchema.customHtml;
            } else {
                indicator.innerHTML = `<svg class="icon" viewBox="0 0 24 24"><use href="#dc-components"/></svg>`; // Generic icon
            }
            indicator.addEventListener('click', () => {
                // Find the settings context for this provider
                const providerId = contextSchema.contextId.split('_')[0];
                const settingsContext = this.uiSchema.find(ctx => 
                    ctx.contextId.startsWith(providerId) && ctx.location === this.WebUILocation.Settings
                );

                if (settingsContext) {
                    this.showSection('settings');
                    // Optional: scroll to the specific card
                    setTimeout(() => {
                        const card = document.querySelector(`.card[data-context-id='${settingsContext.contextId}']`);
                        if (card) card.scrollIntoView({ behavior: 'smooth', block: 'center' });
                    }, 100);
                }
            });
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
                card.className = 'card components-card';
                let fieldsHtml = '<div class="components-list">';
                data.components.forEach(comp => {
                    const isWebUI = comp.name === 'WebUI';
                    const checked = comp.enabled ? 'checked' : '';
                    const statusClass = comp.enabled ? 'status-success' : 'status-warning';
                    fieldsHtml += `
                        <div class="field-row comp-row" data-comp-name="${comp.name}">
                            <div class="comp-col comp-name">${comp.name} <span class="comp-version">v${comp.version}</span></div>
                            <div class="comp-col comp-status"><span class="field-value ${statusClass}">${comp.enabled ? 'Enabled' : 'Disabled'}</span></div>
                            <div class="comp-col comp-toggle">
                                <label class="toggle-switch" title="Enable/Disable">
                                    <input type="checkbox" class="comp-toggle" ${checked}>
                                    <span class="slider"></span>
                                </label>
                            </div>
                        </div>`;
                });
                fieldsHtml += '</div>';
                card.innerHTML = `
                    <div class="card-header">
                        <h3 class="card-title">Registered Components</h3>
                    </div>
                    <div class="card-content">${fieldsHtml}</div>
                `;
                grid.appendChild(card);

                // Attach listeners for component enable/disable toggles
                card.querySelectorAll('.field-row').forEach(row => {
                    const name = row.dataset.compName;
                    const toggle = row.querySelector('.comp-toggle');
                    const valueEl = row.querySelector('.field-value');
                    if (!toggle) return;
                    toggle.addEventListener('change', async (e) => {
                        // Confirmation for disabling WebUI
                        if (name === 'WebUI' && e.target.checked === false) {
                            const ok = window.confirm('Disabling WebUI may make the UI inaccessible until reboot/reset. Continue?');
                            if (!ok) {
                                e.target.checked = true; // revert
                                return;
                            }
                        }
                        // Confirmation for disabling Wifi
                        if (name === 'Wifi' && e.target.checked === false) {
                            const ok = window.confirm('Disabling Wifi will stop network connectivity (except AP if enabled). Continue?');
                            if (!ok) {
                                e.target.checked = true;
                                return;
                            }
                        }
                        const enabled = e.target.checked;
                        try {
                            const body = new URLSearchParams({ name, enabled: String(enabled) }).toString();
                            const resp = await fetch('/api/components/enable', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                                body
                            });
                            const result = await resp.json();
                            if (!result.success) {
                                throw new Error('Operation failed');
                            }
                            // Update row status text
                            valueEl.textContent = enabled ? 'Enabled' : 'Disabled';
                            valueEl.classList.toggle('status-success', enabled);
                            valueEl.classList.toggle('status-warning', !enabled);
                            // Refresh schema and re-render UI to reflect contexts
                            await this.loadUISchema();
                            this.renderUI();
                        } catch (err) {
                            console.error('Failed to change component state:', err);
                            // Revert toggle on failure
                            e.target.checked = !enabled;
                            alert('Failed to change component state');
                        }
                    });
                });
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

            const eventType = (field.type === this.WebUIFieldType.Slider) ? 'input' : (field.type === this.WebUIFieldType.Button ? 'click' : 'change');

            el.addEventListener(eventType, (e) => {
                // Ignore programmatically dispatched events to prevent feedback loops
                if (!e.isTrusted) return;
                let value;
                if (field.type === this.WebUIFieldType.Button) {
                    value = 'clicked';
                } else {
                    value = e.target.type === 'checkbox' ? e.target.checked : e.target.value;
                }
                this.sendUICommand(context.contextId, field.name, value);
                // Apply theme immediately on client side when changed from settings
                if (context.contextId === 'webui_settings' && field.name === 'theme') {
                    this.applyTheme(value);
                }
            });
        });
    }

    makeDeviceNameEditable(element) {
        if (element.querySelector('input')) return; // Already editing

        const currentName = element.textContent;
        element.innerHTML = ''; // Clear the h1

        const input = document.createElement('input');
        input.type = 'text';
        input.value = currentName;
        input.className = 'device-name-input';
        this.isEditingDeviceName = true;
        
        const saveName = () => {
            const newName = input.value.trim();
            if (newName && newName !== currentName) {
                element.textContent = newName;
                // The WebUI component is the provider for its own settings
                this.sendUICommand('webui_settings', 'device_name', newName);
            } else {
                element.textContent = currentName; // Revert if empty or unchanged
            }
            this.isEditingDeviceName = false;
        };

        input.addEventListener('blur', saveName);
        input.addEventListener('keydown', e => {
            if (e.key === 'Enter') {
                input.blur();
            } else if (e.key === 'Escape') {
                element.textContent = currentName;
                input.removeEventListener('blur', saveName); // prevent saving on blur
                this.isEditingDeviceName = false;
            }
        });

        element.appendChild(input);
        input.focus();
        input.select();
    }

    updateCharts(contexts) {
        // Generic chart update logic - works with any chart context
        Object.entries(contexts).forEach(([contextId, contextData]) => {
            // Look for chart data fields (ending with '_data')
            Object.entries(contextData).forEach(([fieldName, fieldValue]) => {
                if (fieldName.endsWith('_data')) {
                    try {
                        const chartData = JSON.parse(fieldValue);
                        if (!window.systemChartData) window.systemChartData = {};
                        
                        // Store chart data using the base field name (remove '_data' suffix)
                        const chartName = fieldName.replace('_data', '');
                        window.systemChartData[chartName] = chartData;
                        
                        // Try to call the chart-specific update function
                        const updateFunctionName = `update${contextId}Chart`;
                        if (typeof window[updateFunctionName] === 'function') {
                            window[updateFunctionName]();
                        }
                    } catch (e) {
                        console.warn(`Failed to parse chart data for ${contextId}.${fieldName}:`, e);
                    }
                }
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

    injectCustomStyles() {
        // Remove any existing custom styles
        const existingStyle = document.getElementById('component-custom-styles');
        if (existingStyle) {
            existingStyle.remove();
        }

        // Collect all custom CSS from components
        let combinedCss = '';
        this.uiSchema.forEach(context => {
            if (context.customCss) {
                combinedCss += `/* ${context.contextId} */\n${context.customCss}\n\n`;
            }
        });

        if (combinedCss) {
            const styleElement = document.createElement('style');
            styleElement.id = 'component-custom-styles';
            styleElement.textContent = combinedCss;
            document.head.appendChild(styleElement);
        }
    }

    injectCustomScripts() {
        // Remove any existing custom scripts
        const existingScript = document.getElementById('component-custom-scripts');
        if (existingScript) {
            existingScript.remove();
        }

        // Collect all custom JS from components
        let combinedJs = '';
        this.uiSchema.forEach(context => {
            if (context.customJs) {
                combinedJs += `/* ${context.contextId} */\n${context.customJs}\n\n`;
            }
        });

        if (combinedJs) {
            const scriptElement = document.createElement('script');
            scriptElement.id = 'component-custom-scripts';
            scriptElement.textContent = combinedJs;
            document.head.appendChild(scriptElement);
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
  new DomoticsApp();
});
