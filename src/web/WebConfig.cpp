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

void WebConfig::loadHomeAssistantSettings() {
  haEnabled = preferences->getBool("ha_enabled", false);
  haDiscoveryPrefix = preferences->getString("ha_discovery_prefix", "homeassistant");
}

void WebConfig::loadAdminAuth() {
  // Force initial setup if no admin credentials exist
  adminUser = preferences->getString("admin_user", "");
  adminPass = preferences->getString("admin_pass", "");
  
  // If no credentials set, use temporary defaults but log warning
  if (adminUser.isEmpty() || adminPass.isEmpty()) {
    DLOG_W(LOG_SECURITY, "Using default admin credentials (admin/admin). Change them immediately via /admin!");
    adminUser = "admin";
    adminPass = "admin";
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
    html += "<p><strong>Library:</strong> DomoticsCore v" + String(FIRMWARE_VERSION) + "</p>";
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
    String firmwareFull = SystemUtils::getFullFirmwareVersion();
    doc["version"] = firmwareVersion;
    doc["build"] = String((uint64_t)BUILD_NUMBER_NUM);
    doc["firmware_full"] = firmwareVersion;
    doc["library_version"] = String(FIRMWARE_VERSION);
    doc["library_full"] = firmwareFull;
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
      preferences->putString("ha_discovery_prefix", haDiscoveryPrefix);
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
    html += "<p>Click the button below to reconfigure WiFi settings:</p>";
    html += "<a href='/reset' class='button' onclick='return confirm(\"This will reset WiFi settings and reboot the device. Continue?\")'>Reset WiFi Settings</a>";
    html += "<br><br><a href='/' class='button'>Back to Main</a>";
    html += "</div>" + getHTMLFooter();
    request->send(200, "text/html", html);
  });

  // WiFi reset
  server->on("/reset", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    request->send(200, "text/plain", "Resetting WiFi settings...");
    delay(1000);
    WiFi.disconnect(true);
    ESP.restart();
  });

  // Reboot
  server->on("/reboot", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!authenticate(request)) return;
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
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
