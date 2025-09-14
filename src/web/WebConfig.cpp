#include <DomoticsCore/WebConfig.h>
#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/Logger.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "WebConfigPages.h"

WebConfig::WebConfig(AsyncWebServer* srv, Preferences* prefs, const String& device, 
                     const String& mfg, const String& version) 
  : server(srv), preferences(prefs), deviceName(device), 
    manufacturer(mfg), firmwareVersion(version) {
  mqttPort = 1883;
  mqttEnabled = false;
  authAttemptCount = 0;
  // Initialize auth attempts array
  for (int i = 0; i < MAX_AUTH_ATTEMPTS; i++) {
    authAttempts[i].timestamp = 0;
    authAttempts[i].ip = "";
  }
}

void WebConfig::begin() {
  loadMQTTSettings();
  loadHomeAssistantSettings();
  loadMDNSSettings();
  loadAdminAuth();
  setupRoutes();
}

void WebConfig::loadMQTTSettings() {
  mqttEnabled = preferences->getBool("mqtt_enabled", false);
  mqttServer = preferences->getString("mqtt_server", "");
  mqttPort = preferences->getInt("mqtt_port", 1883);
  mqttUser = preferences->getString("mqtt_user", "");
  mqttPassword = preferences->getString("mqtt_password", "");
  mqttClientId = preferences->getString("mqtt_clientid", "jnov-esp32-domotics");
}

void WebConfig::setMQTTChangeCallback(ChangeCallback callback) {
  mqttChangeCallback = callback;
}

void WebConfig::setHomeAssistantChangeCallback(ChangeCallback callback) {
  haChangeCallback = callback;
}

void WebConfig::setWiFiConnectedCallback(WiFiConnectedCallback callback) {
  wifiConnectedCallback = callback;
}

void WebConfig::setAPModeStatusCallback(APModeStatusCallback callback) {
  apModeStatusCallback = callback;
}

void WebConfig::loadHomeAssistantSettings() {
  haEnabled = preferences->getBool("ha_enabled", false);
  haDiscoveryPrefix = preferences->getString("ha_prefix", "homeassistant");
}

void WebConfig::loadMDNSSettings() {
  mdnsEnabled = preferences->getBool("mdns_enabled", true);
  mdnsHostname = preferences->getString("mdns_hostname", "esp32-domotics");
}

void WebConfig::loadAdminAuth() {
  // Load admin credentials (defaults should already be set by setDefaultAdmin)
  adminUser = preferences->getString("admin_user", "admin");
  adminPass = preferences->getString("admin_pass", "admin");
  
  // Log warning if using default credentials
  if (adminUser == "admin" && adminPass == "admin") {
    DLOG_W(LOG_SECURITY, "Using default admin credentials (admin/admin). Change them immediately via /admin!");
  }
}

bool WebConfig::authenticate(AsyncWebServerRequest* request) {
  String clientIP = request->client()->remoteIP().toString();
  
  // Check rate limiting first
  if (isRateLimited(clientIP)) {
    request->send(429, "text/plain", "Too many authentication attempts. Try again later.");
    return false;
  }
  
  if (!request->authenticate(adminUser.c_str(), adminPass.c_str())) {
    recordAuthAttempt(clientIP);
    request->requestAuthentication("DomoticsCore");
    return false;
  }
  return true;
}

String WebConfig::getHTMLHeader(const String& title) {
  String html;
  html.reserve(2048);
  char buffer[2048];
  snprintf_P(buffer, sizeof(buffer), HTML_HEADER, title.c_str());
  html = buffer;
  return html;
}

String WebConfig::getHTMLFooter() {
  return String(FPSTR(HTML_FOOTER));
}

void WebConfig::setupRoutes() {
  // Main page
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
    String html = getHTMLHeader("JNOV ESP32 Domotics");
    html += "<div class='container'><h1>JNOV ESP32 Domotics Control Panel</h1>";

    // System information
    html += "<div class='info'><h3>System Information</h3>";
    html += "<p><strong>Manufacturer:</strong> " + manufacturer + "</p>";
    html += "<p><strong>Device:</strong> " + deviceName + "</p>";
    html += "<p><strong>Firmware:</strong> v" + firmwareVersion + "</p>";
    html += "<p><strong>Library:</strong> DomoticsCore v" + String(DOMOTICSCORE_VERSION) + "</p>";
    html += "<p><strong>Build:</strong> " + String((uint64_t)BUILD_NUMBER_NUM) + "</p>";
    html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>";
    html += "<p><strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
    html += "<p><strong>Uptime:</strong> " + String(millis()/1000) + " seconds</p>";

    if (SystemUtils::isTimeInitialized()) {
      String timeStr = SystemUtils::getCurrentTimeString();
      if (timeStr.length() > 0) {
        html += "<p><strong>Current Time:</strong> " + timeStr + "</p>";
      }
    } else {
      html += "<p><strong>Time:</strong> Not synchronized</p>";
    }
    html += "</div>";

    // Control buttons
    html += "<h3>Configuration</h3>";
    html += "<a href='/wifi' class='button'>WiFi Settings</a>";
    html += "<a href='/mqtt' class='button'>MQTT Settings</a>";
    html += "<a href='/update' class='button'>OTA Update</a>";
    html += "<a href='/version' class='button'>Version JSON</a>";
    html += "<a href='/admin' class='button'>Admin Settings</a>";
    html += "<a href='/reboot' class='button' onclick='return confirm(\"Reboot device?\")'>Reboot</a>";
    html += "<a href='/reset' class='button' onclick='return confirm(\"Reset WiFi settings?\")'>Reset WiFi</a>";

    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });

  // Version JSON endpoint
  server->on("/version", HTTP_GET, [this](AsyncWebServerRequest *request){
    ArduinoJson::JsonDocument doc;
    String firmwareFull = firmwareVersion + "+build." + String((uint64_t)BUILD_NUMBER_NUM);
    doc["version"] = firmwareVersion;
    doc["build"] = String((uint64_t)BUILD_NUMBER_NUM);
    doc["firmware_full"] = firmwareFull;
    doc["library_version"] = String(DOMOTICSCORE_VERSION);
    doc["device"] = deviceName;
    doc["manufacturer"] = manufacturer;
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["uptime_s"] = (uint32_t)(millis() / 1000);
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // MQTT Configuration page
  server->on("/mqtt", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    String html = getHTMLHeader("MQTT & Home Assistant Configuration");
    html += "<div class='container'><h1>MQTT & Home Assistant Configuration</h1>";

    html += "<div class='info'><h3>Current Status</h3>";
    html += "<p><strong>MQTT Enabled:</strong> " + String(mqttEnabled ? "Yes" : "No") + "</p>";
    if (mqttEnabled) {
      html += "<p><strong>Server:</strong> " + mqttServer + "</p>";
      html += "<p><strong>Port:</strong> " + String(mqttPort) + "</p>";
      html += "<p><strong>Client ID:</strong> " + mqttClientId + "</p>";
    }
    html += "<p><strong>Home Assistant Discovery:</strong> " + String(haEnabled ? "Enabled" : "Disabled") + "</p>";
    if (haEnabled) {
      html += "<p><strong>Discovery Prefix:</strong> " + haDiscoveryPrefix + "</p>";
    }

    // mDNS configuration section
    html += "<div class='section'>";
    html += "<h3>mDNS Configuration</h3>";
    html += "<div class='form-group'>";
    html += "<label for='mdns_enabled'>Enable mDNS:</label>";
    html += "<input type='checkbox' id='mdns_enabled' name='mdns_enabled'" + String(mdnsEnabled ? " checked" : "") + ">";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mdns_hostname'>mDNS Hostname:</label>";
    html += "<input type='text' id='mdns_hostname' name='mdns_hostname' value='" + mdnsHostname + "' placeholder='device-name'>";
    html += "<small>Device will be accessible at: [hostname].local</small>";
    html += "</div>";
    html += "</div>";


    html += "<form method='POST' action='/mqtt'>";
    html += "<label><input type='checkbox' name='enabled' " + String(mqttEnabled ? "checked" : "") + "> Enable MQTT</label>";
    html += "<label>MQTT Server:</label><input type='text' name='server' value='" + mqttServer + "' placeholder='mqtt.example.com'>";
    html += "<label>Port:</label><input type='number' name='port' value='" + String(mqttPort) + "' min='1' max='65535'>";
    html += "<label>Username (optional):</label><input type='text' name='user' value='" + mqttUser + "'>";
    // Do not echo stored password; leave blank unless user wants to change it
    html += "<label>Password (optional):</label><input type='password' name='password' value='' placeholder='(unchanged)'>";
    html += "<label>Client ID:</label><input type='text' name='clientid' value='" + mqttClientId + "'>";
    
    // Home Assistant integration (part of MQTT config)
    html += "<h4>Home Assistant Integration</h4>";
    html += "<label><input type='checkbox' name='ha_enabled' " + String(haEnabled ? "checked" : "") + "> Enable Home Assistant Auto-Discovery</label>";
    html += "<label>Discovery Prefix:</label><input type='text' name='ha_discovery_prefix' value='" + haDiscoveryPrefix + "' placeholder='homeassistant'>";
    html += "<small>MQTT topic prefix for Home Assistant discovery</small>";
    html += "<div class='info-box'>";
    html += "<p><strong>Note:</strong> Home Assistant auto-discovery uses MQTT to publish device information.</p>";
    html += "</div>";
    
    html += "<br><br><input type='submit' value='Save Configuration' class='button'>";
    html += "</form>";

    html += "<br><a href='/' class='button'>Back to Main</a>";
    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });

  // MQTT Configuration POST handler
  server->on("/mqtt", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    bool enabled = request->hasParam("enabled", true);

    if (!request->hasParam("server", true) || !request->hasParam("port", true) || !request->hasParam("clientid", true)) {
      String html = getHTMLHeader("Configuration Error");
      html += "<div class='container'><h1>Configuration Error</h1>";
      html += "<div class='error'><p>Missing required parameters. Please try again.</p></div>";
      html += "<a href='/mqtt' class='button'>Back to MQTT Settings</a>";
      html += "</div>" + getHTMLFooter();
      request->send(400, "text/html", html);
      return;
    }

    String server = request->getParam("server", true)->value();
    int port = request->getParam("port", true)->value().toInt();
    String user = request->hasParam("user", true) ? request->getParam("user", true)->value() : "";
    String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    String clientId = request->getParam("clientid", true)->value();

    if (port < 1 || port > 65535) {
      DLOG_W(LOG_WEB, "Invalid MQTT port %d, using default 1883", port);
      port = 1883;
    }

    if (server.length() == 0) {
      String html = getHTMLHeader("Configuration Error");
      html += "<div class='container'><h1>Configuration Error</h1>";
      html += "<div class='error'><p>MQTT server address cannot be empty.</p></div>";
      html += "<a href='/mqtt' class='button'>Back to MQTT Settings</a>";
      html += "</div>" + getHTMLFooter();
      request->send(400, "text/html", html);
      return;
    }

    if (clientId.isEmpty()) {
      clientId = "jnov-esp32-domotics";
    }

    preferences->putBool("mqtt_enabled", enabled);
    preferences->putString("mqtt_server", server);
    preferences->putInt("mqtt_port", port);
    preferences->putString("mqtt_user", user);
    if (password.length() > 0) {
      preferences->putString("mqtt_password", password);
      mqttPassword = password;
    }
    preferences->putString("mqtt_clientid", clientId);

    mqttEnabled = enabled;
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    // Don't overwrite password if it wasn't provided
    if (password.length() > 0) {
      mqttPassword = password;
    }
    mqttClientId = clientId;
    
    // Trigger MQTT reconnection if callback is set
    if (mqttChangeCallback) {
      mqttChangeCallback();
    }

    // Handle mDNS settings
    if (request->hasParam("mdns_enabled", true)) {
      mdnsEnabled = true;
      preferences->putBool("mdns_enabled", true);
    } else {
      mdnsEnabled = false;
      preferences->putBool("mdns_enabled", false);
    }
    
    if (request->hasParam("mdns_hostname", true)) {
      mdnsHostname = request->getParam("mdns_hostname", true)->value();
      preferences->putString("mdns_hostname", mdnsHostname);
    }
    
    // Handle Home Assistant settings
    if (request->hasParam("ha_enabled", true)) {
      haEnabled = true;
      preferences->putBool("ha_enabled", true);
    } else {
      haEnabled = false;
      preferences->putBool("ha_enabled", false);
    }
    
    if (request->hasParam("ha_discovery_prefix", true)) {
      haDiscoveryPrefix = request->getParam("ha_discovery_prefix", true)->value();
      preferences->putString("ha_prefix", haDiscoveryPrefix);
    }
    
    // Trigger Home Assistant reconfiguration if callback is set
    if (haChangeCallback) {
      haChangeCallback();
    }

    String html = getHTMLHeader("MQTT & Home Assistant Configuration Saved");
    html += "<div class='container'><h1>Configuration Saved</h1>";
    html += "<div class='success'><p>MQTT and Home Assistant configuration has been saved successfully!</p></div>";
    html += "<a href='/mqtt' class='button'>Back to MQTT Settings</a>";
    html += "<a href='/' class='button'>Main Menu</a>";
    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });

  // WiFi configuration page
  server->on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    String html = getHTMLHeader("WiFi Configuration");
    html += "<div class='container'><h1>WiFi Configuration</h1>";
    
    // Current WiFi status
    html += "<div class='info'>";
    html += "<h3>Current Connection</h3>";
    
    // Check if we're in AP mode
    bool inAPMode = apModeStatusCallback ? apModeStatusCallback() : false;
    
    if (inAPMode) {
      html += "<p><strong>Mode:</strong> Access Point (Setup Mode)</p>";
      html += "<p><strong>AP SSID:</strong> " + String(WiFi.softAPSSID()) + "</p>";
      html += "<p><strong>AP IP:</strong> " + WiFi.softAPIP().toString() + "</p>";
      html += "<div class='warning'>";
      html += "<p><strong>Note:</strong> Device is in setup mode. Connect to a WiFi network or exit AP mode to enable internet services (NTP, OTA, etc.).</p>";
      html += "</div>";
      html += "<form method='POST' action='/wifi/exit-ap' style='margin: 10px 0;'>";
      html += "<input type='submit' value='Exit AP Mode' class='button' onclick='return confirm(\"Exit AP mode? You will need to connect via the device IP address.\")' style='background-color: #ff6b6b;'>";
      html += "</form>";
    } else {
      html += "<p><strong>Status:</strong> " + String(WiFi.isConnected() ? "Connected" : "Disconnected") + "</p>";
      if (WiFi.isConnected()) {
        html += "<p><strong>SSID:</strong> " + WiFi.SSID() + "</p>";
        html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
        html += "<p><strong>Signal Strength:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
      }
    }
    html += "</div>";
    
    // WiFi scan results
    html += "<h3>Available Networks</h3>";
    html += "<div id='networks'><p>Scanning for networks...</p></div>";
    html += "<button onclick='scanNetworks()' class='button'>Refresh Scan</button>";
    
    // Manual WiFi configuration form
    html += "<h3>Connect to Network</h3>";
    html += "<form method='POST' action='/wifi'>";
    html += "<label>Network SSID:</label>";
    html += "<input type='text' name='ssid' id='ssid' placeholder='Enter network name' required>";
    html += "<label>Password:</label>";
    html += "<input type='password' name='password' placeholder='Enter password'>";
    html += "<br><br>";
    html += "<input type='submit' value='Connect' class='button'>";
    html += "</form>";
    
    html += "<br><a href='/' class='button'>Back to Main</a>";
    html += "<a href='/reset' class='button' onclick='return confirm(\"Reset WiFi settings and reboot?\")'>Reset WiFi</a>";
    
    // JavaScript for network scanning
    html += "<script>";
    html += "function scanNetworks() {";
    html += "  document.getElementById('networks').innerHTML = '<p>Scanning...</p>';";
    html += "  fetch('/wifi/scan').then(r => r.json()).then(data => {";
    html += "    let html = '<table><tr><th>SSID</th><th>Signal</th><th>Security</th><th>Action</th></tr>';";
    html += "    data.networks.forEach(net => {";
    html += "      html += '<tr><td>' + net.ssid + '</td><td>' + net.rssi + ' dBm</td>';";
    html += "      html += '<td>' + (net.secure ? 'Secured' : 'Open') + '</td>';";
    html += "      html += '<td><button onclick=\"selectNetwork(\\'' + net.ssid + '\\')\" class=\"button\">Select</button></td></tr>';";
    html += "    });";
    html += "    html += '</table>';";
    html += "    document.getElementById('networks').innerHTML = html;";
    html += "  });";
    html += "}";
    html += "function selectNetwork(ssid) {";
    html += "  document.getElementById('ssid').value = ssid;";
    html += "}";
    html += "scanNetworks();"; // Auto-scan on page load
    html += "</script>";
    
    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });

  // WiFi scan endpoint
  server->on("/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    
    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonArray networks = doc["networks"].to<ArduinoJson::JsonArray>();
    
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      ArduinoJson::JsonObject network = networks.add<ArduinoJson::JsonObject>();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // WiFi connection handler (POST)
  server->on("/wifi", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    
    if (!request->hasParam("ssid", true)) {
      request->send(400, "text/plain", "SSID is required");
      return;
    }
    
    String ssid = request->getParam("ssid", true)->value();
    String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    
    // Save credentials to preferences
    preferences->putString("wifi_ssid", ssid);
    preferences->putString("wifi_password", password);
    
    // Disconnect from current network
    WiFi.disconnect();
    SystemUtils::watchdogSafeDelay(100);
    
    // Connect to new network
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Wait for connection (up to 10 seconds)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      SystemUtils::watchdogSafeDelay(500);
      attempts++;
    }
    
    String html = getHTMLHeader("WiFi Connection Result");
    html += "<div class='container'><h1>WiFi Connection Result</h1>";
    
    if (WiFi.status() == WL_CONNECTED) {
      html += "<div class='success'>";
      html += "<p>Successfully connected to: " + ssid + "</p>";
      html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
      html += "<p>The device will now operate in station mode.</p>";
      html += "</div>";
      
      // Call WiFi connected callback to exit AP mode
      if (wifiConnectedCallback) {
        wifiConnectedCallback();
      }
    } else {
      html += "<div class='error'>";
      html += "<p>Failed to connect to: " + ssid + "</p>";
      html += "<p>Please check the password and try again.</p>";
      html += "</div>";
    }
    
    html += "<br><a href='/wifi' class='button'>Back to WiFi Settings</a>";
    html += "<a href='/' class='button'>Main Menu</a>";
    html += "</div>" + getHTMLFooter();
    
    request->send(200, "text/html", html);
  });

  // Exit AP Mode endpoint
  server->on("/wifi/exit-ap", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    
    String html = getHTMLHeader("Exit AP Mode");
    html += "<div class='container'><h1>Exit AP Mode</h1>";
    
    // Check if we're actually in AP mode
    bool inAPMode = apModeStatusCallback ? apModeStatusCallback() : false;
    
    if (inAPMode) {
      // Call the WiFi connected callback to exit AP mode
      if (wifiConnectedCallback) {
        wifiConnectedCallback();
      }
      
      html += "<div class='success'>";
      html += "<p>Successfully exited AP mode.</p>";
      html += "<p>Device is now in station mode.</p>";
      html += "<p><strong>Important:</strong> You will need to connect to the device using its IP address from now on.</p>";
      if (WiFi.isConnected()) {
        html += "<p>Device IP: " + WiFi.localIP().toString() + "</p>";
      }
      html += "</div>";
    } else {
      html += "<div class='error'>";
      html += "<p>Device is not currently in AP mode.</p>";
      html += "</div>";
    }
    
    html += "<br><a href='/wifi' class='button'>Back to WiFi Settings</a>";
    html += "<a href='/' class='button'>Main Menu</a>";
    html += "</div>" + getHTMLFooter();
    
    request->send(200, "text/html", html);
  });

  // WiFi reset
  server->on("/reset", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    request->send(200, "text/plain", "Resetting WiFi settings...");
    SystemUtils::watchdogSafeDelay(1000);
    WiFi.disconnect(true);
    ESP.restart();
  });

  // Reboot
  server->on("/reboot", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    request->send(200, "text/plain", "Rebooting...");
    SystemUtils::watchdogSafeDelay(1000);
    ESP.restart();
  });

  // Admin Settings page (GET)
  server->on("/admin", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    String html = getHTMLHeader("Admin Settings");
    html += "<div class='container'><h1>Admin Settings</h1>";
    html += "<div class='info'><p>Change the web interface credentials used for protected pages.</p></div>";
    html += "<form method='POST' action='/admin'>";
    html += "<label>Username:</label><input type='text' name='user' value='" + adminUser + "'>";
    html += "<label>New Password (leave blank to keep current):</label><input type='password' name='pass' value='' placeholder='(unchanged)'>";
    html += "<br><br><input type='submit' value='Save Admin Credentials' class='button'>";
    html += "</form>";
    html += "<br><a href='/' class='button'>Back to Main</a>";
    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });

  // Admin Settings (POST)
  server->on("/admin", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    if (!request->hasParam("user", true)) {
      String html = getHTMLHeader("Admin Error");
      html += "<div class='container'><h1>Admin Error</h1>";
      html += "<div class='error'><p>Username is required.</p></div>";
      html += "<a href='/admin' class='button'>Back to Admin Settings</a>";
      html += "</div>" + getHTMLFooter();
      request->send(400, "text/html", html);
      return;
    }
    String user = request->getParam("user", true)->value();
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
    user.trim();
    if (user.isEmpty()) user = "admin";
    preferences->putString("admin_user", user);
    adminUser = user;
    if (pass.length() > 0) {
      preferences->putString("admin_pass", pass);
      adminPass = pass;
    }
    String html = getHTMLHeader("Admin Saved");
    html += "<div class='container'><h1>Admin Settings Saved</h1>";
    html += "<div class='success'><p>Credentials updated successfully.</p></div>";
    html += "<a href='/admin' class='button'>Back to Admin Settings</a>";
    html += "<a href='/' class='button'>Main Menu</a>";
    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });
}

bool WebConfig::isRateLimited(const String& clientIP) {
  unsigned long currentTime = millis();
  int recentAttempts = 0;
  
  // Count recent attempts from this IP
  for (int i = 0; i < authAttemptCount && i < MAX_AUTH_ATTEMPTS; i++) {
    if (authAttempts[i].ip == clientIP && 
        (currentTime - authAttempts[i].timestamp) < AUTH_LOCKOUT_TIME) {
      recentAttempts++;
    }
  }
  
  return recentAttempts >= MAX_AUTH_ATTEMPTS;
}

void WebConfig::recordAuthAttempt(const String& clientIP) {
  unsigned long currentTime = millis();
  
  // Find oldest slot or reuse existing IP slot
  int slotToUse = 0;
  unsigned long oldestTime = currentTime;
  
  for (int i = 0; i < MAX_AUTH_ATTEMPTS; i++) {
    if (authAttempts[i].ip == clientIP) {
      // Update existing IP attempt
      authAttempts[i].timestamp = currentTime;
      return;
    }
    if (authAttempts[i].timestamp < oldestTime) {
      oldestTime = authAttempts[i].timestamp;
      slotToUse = i;
    }
  }
  
  // Use the oldest slot
  authAttempts[slotToUse].timestamp = currentTime;
  authAttempts[slotToUse].ip = clientIP;
  
  if (authAttemptCount < MAX_AUTH_ATTEMPTS) {
    authAttemptCount++;
  }
  
  DLOG_E(LOG_SECURITY, "Failed auth attempt from %s", clientIP.c_str());
}

void WebConfig::setDefaultMDNS(bool enabled, const String& hostname) {
  mdnsEnabled = enabled;
  mdnsHostname = hostname;
  
  // Save to preferences if they don't exist
  if (!preferences->isKey("mdns_enabled")) {
    preferences->putBool("mdns_enabled", enabled);
  }
  if (!preferences->isKey("mdns_hostname")) {
    preferences->putString("mdns_hostname", hostname);
  }
}

void WebConfig::setDefaultMQTT(bool enabled, const String& server, int port,
                              const String& user, const String& password,
                              const String& clientId) {
  mqttEnabled = enabled;
  mqttServer = server;
  mqttPort = port;
  mqttUser = user;
  mqttPassword = password;
  mqttClientId = clientId;
  
  // Save to preferences if they don't exist
  if (!preferences->isKey("mqtt_enabled")) {
    preferences->putBool("mqtt_enabled", enabled);
  }
  if (!preferences->isKey("mqtt_server")) {
    preferences->putString("mqtt_server", server);
  }
  if (!preferences->isKey("mqtt_port")) {
    preferences->putInt("mqtt_port", port);
  }
  if (!preferences->isKey("mqtt_user")) {
    preferences->putString("mqtt_user", user);
  }
  if (!preferences->isKey("mqtt_password")) {
    preferences->putString("mqtt_password", password);
  }
  if (!preferences->isKey("mqtt_clientid")) {
    preferences->putString("mqtt_clientid", clientId);
  }
}

void WebConfig::setDefaultHomeAssistant(bool enabled, const String& discoveryPrefix) {
  haEnabled = enabled;
  haDiscoveryPrefix = discoveryPrefix;
  
  // Save to preferences if they don't exist
  if (!preferences->isKey("ha_enabled")) {
    preferences->putBool("ha_enabled", enabled);
  }
  if (!preferences->isKey("ha_prefix")) {
    preferences->putString("ha_prefix", discoveryPrefix);
  }
}

void WebConfig::setDefaultAdmin(const String& user, const String& pass) {
  adminUser = user;
  adminPass = pass;
  
  // Save to preferences if they don't exist
  if (!preferences->isKey("admin_user")) {
    preferences->putString("admin_user", user);
  }
  if (!preferences->isKey("admin_pass")) {
    preferences->putString("admin_pass", pass);
  }
}
