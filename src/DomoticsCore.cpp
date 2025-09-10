#include <DomoticsCore/DomoticsCore.h>
#include <DomoticsCore/Config.h>
#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/Logger.h>
#include <ESPmDNS.h>

DomoticsCore::DomoticsCore(const CoreConfig& cfg)
  : cfg(cfg),
    server(cfg.webServerPort),
    ledManager(cfg.ledPin),
    webConfig(&server, &preferences, cfg.deviceName, cfg.manufacturer, cfg.firmwareVersion),
    otaManager(&server, &webConfig),
    homeAssistant(&mqttClient, cfg.deviceName, cfg.deviceName, cfg.manufacturer, cfg.firmwareVersion),
    mqttClient(wifiClient),
    storageManager() {}

void DomoticsCore::begin() {
  // Initialize serial if not already
  if (!Serial) {
    Serial.begin(115200);
    delay(1000);
  }

  // LED init
  ledManager.begin();

  // Preferences
  preferences.begin("esp32-config", false);

  // Initialize storage system
  storageManager.setSystemPreferences(&preferences);
  storageManager.begin();

  // Banner
  DLOG_I(LOG_CORE, "=== %s v%s ===", cfg.deviceName.c_str(), cfg.firmwareVersion.c_str());
  DLOG_I(LOG_CORE, "Library: DomoticsCore v%s", FIRMWARE_VERSION);
  DLOG_I(LOG_CORE, "Manufacturer: %s", MANUFACTURER);
  DLOG_I(LOG_CORE, "System starting...");

  // Starting LED sequence
  ledManager.runSequence(WIFI_STARTING, 3000);

  // System info
  SystemUtils::displaySystemInfo();

  // WiFiManager config
  // WiFi timeouts (WiFiManager expects seconds for config portal timeout and connect timeout)
  wifiManager.setConfigPortalTimeout((int)(cfg.wifiConfigPortalTimeoutMs / 1000UL));
  wifiManager.setConnectTimeout((int)cfg.wifiConnectTimeoutSec);
  
  // Disable WiFiManager debug output to use our unified logging
  wifiManager.setDebugOutput(false);

  wifiManager.setAPCallback([this](WiFiManager *myWiFiManager) {
    DLOG_I(LOG_WIFI, "Entered config mode");
    DLOG_I(LOG_WIFI, "AP IP address: %s", WiFi.softAPIP().toString().c_str());
    DLOG_I(LOG_WIFI, "Connect to: %s", myWiFiManager->getConfigPortalSSID());
    ledManager.setStatus(WIFI_AP_MODE);
  });

  wifiManager.setSaveConfigCallback([this]() {
    DLOG_I(LOG_WIFI, "WiFi config saved, will restart");
    shouldReboot = true;
  });

  // WiFi connect
  DLOG_I(LOG_WIFI, "Starting WiFi configuration...");
  ledManager.runSequence(WIFI_CONNECTING, 1000);

  if (!wifiManager.autoConnect(cfg.deviceName.c_str())) {
    DLOG_E(LOG_WIFI, "Failed to connect and hit timeout");
    DLOG_W(LOG_WIFI, "Starting in AP mode for configuration");
    ledManager.setStatus(WIFI_FAILED);
  } else {
    DLOG_I(LOG_WIFI, "AutoConnect: SUCCESS");
    DLOG_I(LOG_WIFI, "STA IP Address: %s", WiFi.localIP().toString().c_str());
    DLOG_I(LOG_WIFI, "Connect to: http://%s", WiFi.localIP().toString().c_str());
    // brief confirmation
    ledManager.runSequence(WIFI_CONNECTED, 1000);
  }

  // If not strict, we can enter NORMAL OP before NTP to avoid delaying readiness
  if (!cfg.strictNtpBeforeNormalOp) {
    ledManager.setStatus(WIFI_NORMAL_OPERATION);
  }

  // NTP
  SystemUtils::initializeNTP();

  // If strict, only enter NORMAL OP after NTP init attempt
  if (cfg.strictNtpBeforeNormalOp) {
    ledManager.setStatus(WIFI_NORMAL_OPERATION);
  }

  // mDNS
  if (cfg.mdnsEnabled) {
    String host = cfg.mdnsHostname;
    host.toLowerCase();
    host.replace(" ", "-");
    if (!MDNS.begin(host.c_str())) {
      DLOG_E(LOG_SYSTEM, "mDNS start failed");
    } else {
      MDNS.addService("http", "tcp", cfg.webServerPort);
      DLOG_I(LOG_SYSTEM, "mDNS: http://%s.local", host.c_str());
    }
  }

  // Initialize web configuration
  webConfig.setDefaultMQTT(
    cfg.mqttEnabled,
    cfg.mqttServer,
    (int)cfg.mqttPort,
    cfg.mqttUser,
    cfg.mqttPassword,
    cfg.mqttClientId
  );
  webConfig.begin();
  
  // Set up MQTT change callback to trigger reconnection
  webConfig.setMQTTChangeCallback([this]() {
    bool mqttEnabled = cfg.mqttEnabled || webConfig.isMQTTEnabled();
    if (mqttEnabled && mqttInitialized) {
      reconnectMQTT();
    }
  });
  
  // Set up Home Assistant change callback to reinitialize discovery
  webConfig.setHomeAssistantChangeCallback([this]() {
    if (webConfig.isHomeAssistantEnabled()) {
      homeAssistant.begin(webConfig.getHomeAssistantDiscoveryPrefix());
      DLOG_I(LOG_HA, "Home Assistant settings updated and reinitialized");
    }
  });
  
  otaManager.begin();
  
  // Initialize Home Assistant if enabled (check both config and web preferences)
  bool haEnabled = cfg.homeAssistantEnabled || webConfig.isHomeAssistantEnabled();
  if (haEnabled) {
    String discoveryPrefix = webConfig.isHomeAssistantEnabled() ? 
                            webConfig.getHomeAssistantDiscoveryPrefix() : 
                            cfg.homeAssistantDiscoveryPrefix;
    homeAssistant.begin(discoveryPrefix);
    DLOG_I(LOG_HA, "Home Assistant auto-discovery enabled with prefix: %s", discoveryPrefix.c_str());
  }

  // Initialize MQTT if enabled (check both config and web preferences)
  bool mqttEnabled = cfg.mqttEnabled || webConfig.isMQTTEnabled();
  String mqttServer = webConfig.isMQTTEnabled() ? webConfig.getMQTTServer() : cfg.mqttServer;
  
  if (mqttEnabled && !mqttServer.isEmpty()) {
    initializeMQTT();
  }

  // Start server
  server.begin();
  
  // Not found handler for easier diagnostics
  server.onNotFound([this](AsyncWebServerRequest *request){
    DLOG_W(LOG_HTTP, "404 Not Found: %s", request->url().c_str());
    request->send(404, "text/plain", "Not found: " + request->url());
  });

  DLOG_I(LOG_HTTP, "Web interface available at: http://%s", WiFi.localIP().toString().c_str());
  DLOG_I(LOG_HTTP, "Also available at: http://%s.local", cfg.mdnsHostname.c_str());
  DLOG_I(LOG_OTA, "OTA ready at http://%s/update", WiFi.localIP().toString().c_str());
  DLOG_I(LOG_CORE, "=== System Ready ===");
  
}

void DomoticsCore::loop() {
  // Handle reboot request
  if (shouldReboot) {
    DLOG_I(LOG_CORE, "Rebooting in 3 seconds...");
    delay(3000);
    ESP.restart();
  }

  // WiFiManager process
  wifiManager.process();

  // Reconnection management
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiReconnecting) {
      wifiReconnecting = true;
      wifiLostTime = millis();
      reconnectAttempts = 0;
      DLOG_W(LOG_WIFI, "WiFi connection lost, attempting reconnection...");
      ledManager.setStatus(WIFI_RECONNECTING);
    }

    if (reconnectAttempts >= cfg.wifiMaxReconnectAttempts) {
      DLOG_E(LOG_WIFI, "Max WiFi reconnection attempts reached, restarting...");
      delay(1000);
      ESP.restart();
    }

    if (millis() - wifiLostTime > cfg.wifiReconnectTimeoutMs) {
      DLOG_E(LOG_WIFI, "WiFi reconnection timeout, restarting...");
      delay(1000);
      ESP.restart();
    }

    static unsigned long lastReconnectAttempt = 0;
    unsigned long backoffDelay = cfg.wifiReconnectIntervalMs * (1 << min((unsigned int)reconnectAttempts, 3U));
    if (millis() - lastReconnectAttempt > backoffDelay) {
      lastReconnectAttempt = millis();
      reconnectAttempts++;
      DLOG_I(LOG_WIFI, "WiFi reconnection attempt %d/%d (delay: %lums)",
                    reconnectAttempts, cfg.wifiMaxReconnectAttempts, backoffDelay);
      WiFi.reconnect();
    }
  } else {
    if (wifiReconnecting) {
      wifiReconnecting = false;
      reconnectAttempts = 0;
      DLOG_I(LOG_WIFI, "WiFi reconnected successfully!");
      ledManager.setStatus(WIFI_NORMAL_OPERATION);
    }
  }

  // MQTT handling (check both config and web preferences)
  bool mqttEnabled = cfg.mqttEnabled || webConfig.isMQTTEnabled();
  String mqttServer = webConfig.isMQTTEnabled() ? webConfig.getMQTTServer() : cfg.mqttServer;
  
  if (mqttEnabled && !mqttServer.isEmpty()) {
    handleMQTT();
  }

  // LED
  ledManager.update();

  // Periodic log and non-blocking delay
  static unsigned long lastLoopTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastLoopTime >= LOOP_DELAY) {
    lastLoopTime = currentTime;

    static unsigned long lastPrint = 0;
    if (currentTime - lastPrint >= SYSTEM_LOG_INTERVAL) {
      lastPrint = currentTime;
      char logBuffer[128];
      snprintf(logBuffer, sizeof(logBuffer), "System active - Free heap: %d bytes - IP: %s", ESP.getFreeHeap(), WiFi.localIP().toString().c_str());
      DLOG_I(LOG_SYSTEM, "%s", logBuffer);
    }
  }
}

void DomoticsCore::initializeMQTT() {
  DLOG_I(LOG_MQTT, "Initializing MQTT...");
  
  // Load MQTT settings from web config or fallback to config defaults
  String mqttServer = webConfig.isMQTTEnabled() ? webConfig.getMQTTServer() : cfg.mqttServer;
  int mqttPort = webConfig.isMQTTEnabled() ? webConfig.getMQTTPort() : cfg.mqttPort;
  String mqttUser = webConfig.isMQTTEnabled() ? webConfig.getMQTTUser() : cfg.mqttUser;
  String mqttPassword = webConfig.isMQTTEnabled() ? webConfig.getMQTTPassword() : cfg.mqttPassword;
  String mqttClientId = webConfig.isMQTTEnabled() ? webConfig.getMQTTClientId() : cfg.mqttClientId;
  
  if (mqttServer.isEmpty()) {
    DLOG_W(LOG_MQTT, "Server not configured, skipping initialization");
    return;
  }
  
  DLOG_I(LOG_MQTT, "Initializing - Server: %s:%d, Client ID: %s", 
                mqttServer.c_str(), mqttPort, mqttClientId.c_str());
  
  // Set MQTT timeouts
  mqttClient.setSocketTimeout(15); // 15 seconds socket timeout
  mqttClient.setKeepAlive(60);     // 60 seconds keep alive
  
  // Copy all MQTT credentials to static buffers to prevent dangling pointers
  strncpy(mqttServerBuffer, mqttServer.c_str(), sizeof(mqttServerBuffer) - 1);
  mqttServerBuffer[sizeof(mqttServerBuffer) - 1] = '\0';

  strncpy(mqttClientBuffer, mqttClientId.c_str(), sizeof(mqttClientBuffer) - 1);
  mqttClientBuffer[sizeof(mqttClientBuffer) - 1] = '\0';

  strncpy(mqttUserBuffer, mqttUser.c_str(), sizeof(mqttUserBuffer) - 1);
  mqttUserBuffer[sizeof(mqttUserBuffer) - 1] = '\0';

  strncpy(mqttPassBuffer, mqttPassword.c_str(), sizeof(mqttPassBuffer) - 1);
  mqttPassBuffer[sizeof(mqttPassBuffer) - 1] = '\0';

  // Prefer IPAddress to avoid DNS resolution when server is an IP literal
  mqttServerIsIP = mqttServerIp.fromString(mqttServerBuffer);
  if (mqttServerIsIP) {
    mqttClient.setServer(mqttServerIp, mqttPort);
  } else {
    mqttClient.setServer(mqttServerBuffer, mqttPort);
  }
  mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    DLOG_D(LOG_MQTT, "Received: %s => %s", topic, message.c_str());
    onMQTTMessage(String(topic), message);
  });
  
  mqttInitialized = true;
}

void DomoticsCore::handleMQTT() {
  if (!mqttInitialized || !WiFi.isConnected()) {
    return;
  }
  
  // Handle MQTT connection
  if (!mqttClient.connected()) {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();
    
    if (now - lastReconnectAttempt > 5000) { // Try reconnect every 5 seconds
      lastReconnectAttempt = now;
      
      int mqttPort = webConfig.isMQTTEnabled() ? webConfig.getMQTTPort() : cfg.mqttPort;

      // Test network connectivity first, avoiding DNS if server is an IP literal
      WiFiClient testClient;
      bool reachable = false;
      if (mqttServerIsIP) {
        reachable = testClient.connect(mqttServerIp, mqttPort);
      } else {
        reachable = testClient.connect(mqttServerBuffer, mqttPort);
      }
      if (!reachable) {
        DLOG_E(LOG_MQTT, "Connection failed - cannot reach %s:%d", mqttServerBuffer, mqttPort);
        testClient.stop();
        return;
      }
      testClient.stop();

      bool connected = false;
      if (strlen(mqttUserBuffer) > 0) {
        connected = mqttClient.connect(mqttClientBuffer, mqttUserBuffer, mqttPassBuffer);
      } else {
        connected = mqttClient.connect(mqttClientBuffer);
      }
      
      if (connected) {
        DLOG_I(LOG_MQTT, "Connected successfully!");
        
        // Subscribe to default topics
        String baseTopic = "jnov/" + String(mqttClientBuffer);
        mqttClient.subscribe((baseTopic + "/cmd").c_str());
        mqttClient.subscribe((baseTopic + "/set").c_str());
        
        // Publish online status
        mqttClient.publish((baseTopic + "/status").c_str(), "online", true);
        DLOG_I(LOG_MQTT, "Ready - Topics: %s/{cmd,set,status}", baseTopic.c_str());
        
        mqttConnected = true;
      } else {
        int state = mqttClient.state();
        DLOG_E(LOG_MQTT, "Connection failed, rc=%d", state);
        switch(state) {
          case -4: DLOG_E(LOG_MQTT, " (MQTT_CONNECTION_TIMEOUT)"); break;
          case -3: DLOG_E(LOG_MQTT, " (MQTT_CONNECTION_LOST)"); break;
          case -2: DLOG_E(LOG_MQTT, " (MQTT_CONNECT_FAILED)"); break;
          case -1: DLOG_E(LOG_MQTT, " (MQTT_DISCONNECTED)"); break;
          case  1: DLOG_E(LOG_MQTT, " (MQTT_CONNECT_BAD_PROTOCOL)"); break;
          case  2: DLOG_E(LOG_MQTT, " (MQTT_CONNECT_BAD_CLIENT_ID)"); break;
          case  3: DLOG_E(LOG_MQTT, " (MQTT_CONNECT_UNAVAILABLE)"); break;
          case  4: DLOG_E(LOG_MQTT, " (MQTT_CONNECT_BAD_CREDENTIALS)"); break;
          case  5: DLOG_E(LOG_MQTT, " (MQTT_CONNECT_UNAUTHORIZED)"); break;
          default: DLOG_E(LOG_MQTT, " (UNKNOWN)"); break;
        }
        mqttConnected = false;
      }
    }
  } else {
    mqttConnected = true;
  }
  
  // Process MQTT messages
  mqttClient.loop();
}

void DomoticsCore::reconnectMQTT() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  mqttConnected = false;
  DLOG_I(LOG_MQTT, "Forcing reconnection with new settings...");
}

void DomoticsCore::onMQTTMessage(const String& topic, const String& message) {
  // Default MQTT message handler - can be overridden by user
  DLOG_D(LOG_MQTT, "Default handler: %s => %s", topic.c_str(), message.c_str());
  
  // Handle basic system commands
  if (topic.endsWith("/cmd")) {
    if (message == "restart" || message == "reboot") {
      DLOG_I(LOG_MQTT, "Restart command received");
      delay(1000);
      ESP.restart();
    } else if (message == "status") {
      String baseTopic = "jnov/" + String(mqttClientBuffer);
      String statusMsg = "{\"status\":\"online\",\"uptime\":" + String(millis()/1000) + ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
      mqttClient.publish((baseTopic + "/status").c_str(), statusMsg.c_str());
    }
  }
}
