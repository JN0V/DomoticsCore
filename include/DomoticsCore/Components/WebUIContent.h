#pragma once

#include <Arduino.h>

namespace DomoticsCore {
namespace Components {

class WebUIContent {
public:
    // PROGMEM-backed: large content kept in flash and streamed when possible
    static String getHTML() { return String(FPSTR(HTML)); }
    static String getCSS()  { return String(FPSTR(CSS)); }
    static String getJS()   { return String(FPSTR(JS)); }

    // Raw PROGMEM pointer accessors (for efficient streaming in WebUI.h)
    static const char* htmlP() { return HTML; }
    static const char* cssP()  { return CSS; }
    static const char* jsP()   { return JS; }

private:
    // Minimal but structurally compatible content to keep demo functional
    static const char HTML[] PROGMEM;
    static const char CSS[] PROGMEM;
    static const char JS[] PROGMEM;
};

} // namespace Components
} // namespace DomoticsCore

// Definitions of PROGMEM content must be outside the class
namespace DomoticsCore { namespace Components {
const char WebUIContent::HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>DomoticsCore</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <header class="header"><h1 id="deviceName">DomoticsCore Device</h1><div id="datetime"></div><div id="wsStatus" class="status-item"></div></header>
  <main class="main-content">
    <section id="dashboard-section" class="content-section active"><div id="componentsOverview"></div></section>
    <section id="components-section" class="content-section"><div id="devicesContainer"></div></section>
    <section id="settings-section" class="content-section"><div id="settingsContainer"></div></section>
  </main>
  <footer class="footer"><span id="manufacturer">DomoticsCore</span> | <span id="version">v1.0.0</span></footer>
  <script src="/app.js"></script>
</body>
</html>
)";

const char WebUIContent::CSS[] PROGMEM = R"(
:root {
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
.content-section.active { display: block; }
)";

const char WebUIContent::JS[] PROGMEM = R"(
document.addEventListener('DOMContentLoaded',()=>{
  const dt=document.getElementById('datetime');
  setInterval(()=>{if(dt){dt.textContent=new Date().toLocaleString();}},1000);
  const wsEl=document.getElementById('wsStatus');
  try{
    const proto=location.protocol==='https:'?'wss':'ws';
    const ws=new WebSocket(proto+'://'+location.host+'/ws');
    ws.onopen=()=>{if(wsEl){wsEl.classList.add('online');wsEl.classList.remove('offline');}};
    ws.onclose=()=>{if(wsEl){wsEl.classList.remove('online');wsEl.classList.add('offline');}};
  }catch(e){}
});)";

}} // namespace DomoticsCore::Components
