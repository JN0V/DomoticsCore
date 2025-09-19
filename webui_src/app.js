class DomoticsApp {
  constructor() {
    this.ws = null;
    this.wsReconnectInterval = null;
    this.systemStartTime = Date.now();
    this.init();
  }

  init() {
    this.setupEventListeners();
    this.setupWebSocket();
    this.updateDateTime();
    setInterval(() => this.updateDateTime(), 1000);
  }

  setupEventListeners() {
    const menuToggle = document.getElementById('menuToggle');
    const sidebar = document.getElementById('sidebar');
    const navLinks = document.querySelectorAll('.nav-link');

    if (menuToggle && sidebar) {
      menuToggle.addEventListener('click', () => {
        sidebar.classList.toggle('open');
      });
    }

    navLinks.forEach((link) => {
      link.addEventListener('click', (e) => {
        e.preventDefault();
        const section = link.dataset.section;
        this.showSection(section);
        navLinks.forEach((l) => l.classList.remove('active'));
        link.classList.add('active');
      });
    });

    document.addEventListener('click', (e) => {
      if (sidebar && menuToggle) {
        if (!sidebar.contains(e.target) && !menuToggle.contains(e.target)) {
          sidebar.classList.remove('open');
        }
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

    this.ws.onmessage = (event) => {
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

    this.ws.onerror = (error) => {
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
    if (data.system) {
      this.updateSystemInfo(data.system);
    }
    if (data.contexts) {
      this.updateContexts(data.contexts);
    }
  }

  updateSystemInfo(system) {
    if (typeof system.uptime === 'number') {
      this.systemStartTime = Date.now() - system.uptime;
    }
    if (system.device_name) {
      const el = document.getElementById('deviceName');
      if (el) el.textContent = system.device_name;
    }
    if (system.manufacturer && system.version) {
      const footerText = document.getElementById('footerText');
      if (footerText) {
        footerText.textContent = `${system.manufacturer} ${system.version} | ESP32 Framework`;
      }
    }
  }

  updateContexts(contexts) {
    const dashboardGrid = document.getElementById('dashboardGrid');
    if (!dashboardGrid) return;
    dashboardGrid.innerHTML = '';

    Object.entries(contexts).forEach(([id, context]) => {
      const card = this.createContextCard(id, context);
      dashboardGrid.appendChild(card);
    });
  }

  createContextCard(id, context) {
    const card = document.createElement('div');
    card.className = 'card';

    let content = '';
    if (context && typeof context === 'object') {
      Object.entries(context).forEach(([key, value]) => {
        if (key !== 'name' && key !== 'id') {
          content += `
            <div class="field-row">
              <span class="field-label">${this.formatLabel(key)}:</span>
              <span class="field-value">${this.formatValue(value)}</span>
            </div>`;
        }
      });
    } else {
      content = `
        <div class="field-row">
          <span class="field-value">${String(context)}</span>
        </div>`;
    }

    card.innerHTML = `
      <div class="card-header">
        <h3 class="card-title">${this.formatTitle(id)}</h3>
        <span class="card-status status-success">Active</span>
      </div>
      <div class="card-content">${content}</div>`;

    return card;
  }

  formatTitle(id) {
    return id.replace(/_/g, ' ').replace(/\b\w/g, (l) => l.toUpperCase());
  }

  formatLabel(key) {
    return key.replace(/_/g, ' ').replace(/\b\w/g, (l) => l.toUpperCase());
  }

  formatValue(value) {
    if (typeof value === 'boolean') return value ? 'On' : 'Off';
    if (typeof value === 'number' && value > 1_000_000) return (value / 1_000_000).toFixed(1) + 'M';
    if (typeof value === 'number' && value > 1_000) return (value / 1_000).toFixed(1) + 'K';
    return String(value);
  }

  updateConnectionStatus(connected) {
    const wsStatus = document.getElementById('wsStatus');
    if (wsStatus) {
      wsStatus.classList.toggle('connected', connected);
    }
  }

  updateDateTime() {
    const now = Date.now();
    const uptime = Math.floor((now - this.systemStartTime) / 1000);
    const hours = Math.floor(uptime / 3600);
    const minutes = Math.floor((uptime % 3600) / 60);
    const seconds = uptime % 60;
    const uptimeStr = `${hours.toString().padStart(2, '0')}:${minutes
      .toString()
      .padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;

    const dt = document.getElementById('datetime');
    if (dt) dt.textContent = `Uptime: ${uptimeStr}`;
  }

  showSection(sectionName) {
    document.querySelectorAll('.content-section').forEach((section) => {
      section.classList.remove('active');
    });
    const targetSection = document.getElementById(`${sectionName}-section`);
    if (targetSection) targetSection.classList.add('active');

    if (sectionName === 'components') this.loadComponents();
    else if (sectionName === 'settings') this.loadSettings();
  }

  loadComponents() {
    const grid = document.getElementById('componentsGrid');
    if (!grid) return;
    grid.innerHTML = `
      <div class="card">
        <div class="card-header">
          <h3 class="card-title">System Components</h3>
        </div>
        <div class="card-content">
          <div class="field-row"><span class="field-label">WebUI Component:</span><span class="field-value">Active</span></div>
          <div class="field-row"><span class="field-label">Demo LED Controller:</span><span class="field-value">Active</span></div>
          <div class="field-row"><span class="field-label">Demo System Info:</span><span class="field-value">Active</span></div>
        </div>
      </div>`;
  }

  loadSettings() {
    const grid = document.getElementById('settingsGrid');
    if (!grid) return;
    grid.innerHTML = `
      <div class="card">
        <div class="card-header">
          <h3 class="card-title">System Information</h3>
        </div>
        <div class="card-content">
          <div class="field-row"><span class="field-label">Device:</span><span class="field-value">ESP32</span></div>
          <div class="field-row"><span class="field-label">Framework:</span><span class="field-value">DomoticsCore v2.0</span></div>
          <div class="field-row"><span class="field-label">WebUI Port:</span><span class="field-value">80</span></div>
        </div>
      </div>`;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  new DomoticsApp();
});
