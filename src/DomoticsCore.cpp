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

  // Modules
  // Apply default MQTT parameters from configuration (can be overridden via UI/preferences)
  webConfig.setDefaultMQTT(
    cfg.mqttEnabled,
    cfg.mqttServer,
    (int)cfg.mqttPort,
    cfg.mqttUser,
    cfg.mqttPassword,
    cfg.mqttClientId
  );
  webConfig.begin();
  otaManager.begin();
  
  // Initialize Home Assistant if enabled
  if (cfg.homeAssistantEnabled) {
    homeAssistant.begin(cfg.homeAssistantDiscoveryPrefix);
    Serial.println("Home Assistant auto-discovery enabled");
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

  Serial.println("\n=== System Ready ===");
  Serial.println("Web interface available at: http://" + WiFi.localIP().toString());
  if (cfg.mdnsEnabled) {
    String host = cfg.mdnsHostname;
    host.toLowerCase();
    host.replace(" ", "-");
    Serial.println("Also available at: http://" + host + ".local");
  }
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
