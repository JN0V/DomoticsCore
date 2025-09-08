#include <DomoticsCore/DomoticsCore.h>
#include <DomoticsCore/Config.h>
#include <DomoticsCore/SystemUtils.h>
#include <ESPmDNS.h>

DomoticsCore::DomoticsCore(const CoreConfig& cfg)
  : cfg(cfg),
    server(cfg.webServerPort),
    ledManager(cfg.ledPin),
    webConfig(&server, &preferences, cfg.deviceName, cfg.manufacturer, FIRMWARE_VERSION),
    otaManager(&server, &webConfig),
    homeAssistant(&mqttClient, cfg.deviceName, cfg.deviceName, cfg.manufacturer, FIRMWARE_VERSION),
    mqttClient(wifiClient) {}

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

  // Banner
  Serial.println(String("=== JNOV ESP32 Domotics Core v") + SystemUtils::getFullFirmwareVersion() + " ===");
  Serial.println("Manufacturer: " MANUFACTURER);
  Serial.println("System starting...");

  // Starting LED sequence
  ledManager.runSequence(WIFI_STARTING, 3000);

  // System info
  SystemUtils::displaySystemInfo();

  // WiFiManager config
  // WiFi timeouts (WiFiManager expects seconds for config portal timeout and connect timeout)
  wifiManager.setConfigPortalTimeout((int)(cfg.wifiConfigPortalTimeoutMs / 1000UL));
  wifiManager.setConnectTimeout((int)cfg.wifiConnectTimeoutSec);

  wifiManager.setAPCallback([this](WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println("AP IP address: " + WiFi.softAPIP().toString());
    Serial.println("Connect to: " + String(myWiFiManager->getConfigPortalSSID()));
    ledManager.setStatus(WIFI_AP_MODE);
  });

  wifiManager.setSaveConfigCallback([this]() {
    Serial.println("WiFi config saved, will restart");
    shouldReboot = true;
  });

  // WiFi connect
  Serial.println("\nStarting WiFi configuration...");
  ledManager.runSequence(WIFI_CONNECTING, 1000);

  if (!wifiManager.autoConnect(cfg.deviceName.c_str())) {
    Serial.println("Failed to connect and hit timeout");
    Serial.println("Starting in AP mode for configuration");
    ledManager.setStatus(WIFI_FAILED);
  } else {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Connect to: http://%s\n", WiFi.localIP().toString().c_str());
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
      Serial.println("mDNS start failed");
    } else {
      MDNS.addService("http", "tcp", cfg.webServerPort);
      Serial.println("mDNS: http://" + host + ".local");
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
      Serial.println("[HA] Home Assistant settings updated and reinitialized");
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
    Serial.println("Home Assistant auto-discovery enabled with prefix: " + discoveryPrefix);
  }

  // Initialize MQTT if enabled (check both config and web preferences)
  bool mqttEnabled = cfg.mqttEnabled || webConfig.isMQTTEnabled();
  String mqttServer = webConfig.isMQTTEnabled() ? webConfig.getMQTTServer() : cfg.mqttServer;
  
  if (mqttEnabled && !mqttServer.isEmpty()) {
    initializeMQTT();
  }

  // Start server
  server.begin();
  Serial.println("Web interface available at: http://" + WiFi.localIP().toString());
  Serial.println("OTA ready at http://" + WiFi.localIP().toString() + "/update");
  
  // Not found handler for easier diagnostics
  server.onNotFound([this](AsyncWebServerRequest *request){
    Serial.printf("[HTTP] 404 Not Found: %s\n", request->url().c_str());
    request->send(404, "text/plain", "Not found: " + request->url());
  });

  Serial.println("=== System Ready ===");
  Serial.println("Web interface available at: http://" + WiFi.localIP().toString());
  Serial.println("Also available at: http://" + cfg.mdnsHostname + ".local");
  
}

void DomoticsCore::loop() {
  // Handle reboot request
  if (shouldReboot) {
    Serial.println("Rebooting in 3 seconds...");
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
      Serial.println("WiFi connection lost, attempting reconnection...");
      ledManager.setStatus(WIFI_RECONNECTING);
    }

    if (reconnectAttempts >= cfg.wifiMaxReconnectAttempts) {
      Serial.println("Max WiFi reconnection attempts reached, restarting...");
      delay(1000);
      ESP.restart();
    }

    if (millis() - wifiLostTime > cfg.wifiReconnectTimeoutMs) {
      Serial.println("WiFi reconnection timeout, restarting...");
      delay(1000);
      ESP.restart();
    }

    static unsigned long lastReconnectAttempt = 0;
    unsigned long backoffDelay = cfg.wifiReconnectIntervalMs * (1 << min((unsigned int)reconnectAttempts, 3U));
    if (millis() - lastReconnectAttempt > backoffDelay) {
      lastReconnectAttempt = millis();
      reconnectAttempts++;
      Serial.printf("WiFi reconnection attempt %d/%d (delay: %lums)\n",
                    reconnectAttempts, cfg.wifiMaxReconnectAttempts, backoffDelay);
      WiFi.reconnect();
    }
  } else {
    if (wifiReconnecting) {
      wifiReconnecting = false;
      reconnectAttempts = 0;
      Serial.println("WiFi reconnected successfully!");
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

  // Periodic log
  static unsigned long lastPrint = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastPrint >= SYSTEM_LOG_INTERVAL) {
    lastPrint = currentTime;

    String logMessage;
    logMessage.reserve(128);
    logMessage = "System active - Free heap: ";
    logMessage += String(ESP.getFreeHeap());
    logMessage += " bytes - IP: ";
    logMessage += WiFi.localIP().toString();
    Serial.println(SystemUtils::getFormattedLog(logMessage));
  }

  delay(LOOP_DELAY);
}

void DomoticsCore::initializeMQTT() {
  Serial.println("Initializing MQTT...");
  
  // Load MQTT settings from web config or fallback to config defaults
  String mqttServer = webConfig.isMQTTEnabled() ? webConfig.getMQTTServer() : cfg.mqttServer;
  int mqttPort = webConfig.isMQTTEnabled() ? webConfig.getMQTTPort() : cfg.mqttPort;
  String mqttUser = webConfig.isMQTTEnabled() ? webConfig.getMQTTUser() : cfg.mqttUser;
  String mqttPassword = webConfig.isMQTTEnabled() ? webConfig.getMQTTPassword() : cfg.mqttPassword;
  String mqttClientId = webConfig.isMQTTEnabled() ? webConfig.getMQTTClientId() : cfg.mqttClientId;
  
  if (mqttServer.isEmpty()) {
    Serial.println("[MQTT] Server not configured, skipping initialization");
    return;
  }
  
  Serial.printf("[MQTT] Initializing - Server: %s:%d, Client ID: %s\n", 
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
    Serial.printf("[MQTT] Received: %s => %s\n", topic, message.c_str());
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
        Serial.printf("[MQTT] Connection failed - cannot reach %s:%d\n", mqttServerBuffer, mqttPort);
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
        Serial.println("[MQTT] Connected successfully!");
        
        // Subscribe to default topics
        String baseTopic = "jnov/" + String(mqttClientBuffer);
        mqttClient.subscribe((baseTopic + "/cmd").c_str());
        mqttClient.subscribe((baseTopic + "/set").c_str());
        
        // Publish online status
        mqttClient.publish((baseTopic + "/status").c_str(), "online", true);
        Serial.printf("[MQTT] Ready - Topics: %s/{cmd,set,status}\n", baseTopic.c_str());
        
        mqttConnected = true;
      } else {
        int state = mqttClient.state();
        Serial.printf("[MQTT] Connection failed, rc=%d", state);
        switch(state) {
          case -4: Serial.println(" (MQTT_CONNECTION_TIMEOUT)"); break;
          case -3: Serial.println(" (MQTT_CONNECTION_LOST)"); break;
          case -2: Serial.println(" (MQTT_CONNECT_FAILED)"); break;
          case -1: Serial.println(" (MQTT_DISCONNECTED)"); break;
          case 1: Serial.println(" (MQTT_CONNECT_BAD_PROTOCOL)"); break;
          case 2: Serial.println(" (MQTT_CONNECT_BAD_CLIENT_ID)"); break;
          case 3: Serial.println(" (MQTT_CONNECT_UNAVAILABLE)"); break;
          case 4: Serial.println(" (MQTT_CONNECT_BAD_CREDENTIALS)"); break;
          case 5: Serial.println(" (MQTT_CONNECT_UNAUTHORIZED)"); break;
          default: Serial.println(" (UNKNOWN)"); break;
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
  Serial.println("[MQTT] Forcing reconnection with new settings...");
}

void DomoticsCore::onMQTTMessage(const String& topic, const String& message) {
  // Default MQTT message handler - can be overridden by user
  Serial.printf("[MQTT] Default handler: %s => %s\n", topic.c_str(), message.c_str());
  
  // Handle basic system commands
  if (topic.endsWith("/cmd")) {
    if (message == "restart" || message == "reboot") {
      Serial.println("[MQTT] Restart command received");
      delay(1000);
      ESP.restart();
    } else if (message == "status") {
      String baseTopic = "jnov/" + String(mqttClientBuffer);
      String statusMsg = "{\"status\":\"online\",\"uptime\":" + String(millis()/1000) + ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
      mqttClient.publish((baseTopic + "/status").c_str(), statusMsg.c_str());
    }
  }
}
