/**
 * @file WebConfig.h
 * @brief Web configuration interface for ESP32 domotics system (integrated under DomoticsCore)
 */

#ifndef DOMOTICS_CORE_WEB_CONFIG_H
#define DOMOTICS_CORE_WEB_CONFIG_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <DomoticsCore/Config.h>

class WebConfig {
private:
  AsyncWebServer* server;
  Preferences* preferences;
  String deviceName;
  String manufacturer;
  String firmwareVersion;

  // MQTT Configuration
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
  String mqttClientId;
  bool mqttEnabled;

  // mDNS Configuration
  bool mdnsEnabled;
  String mdnsHostname;
  
  // Home Assistant Configuration
  bool haEnabled;
  String haDiscoveryPrefix;
  
  // Admin authentication (Basic-Auth)
  String adminUser;
  String adminPass;
  
  // Rate limiting for auth endpoints
  struct AuthAttempt {
    unsigned long timestamp;
    String ip;
  };
  static const int MAX_AUTH_ATTEMPTS = 5;
  static const unsigned long AUTH_LOCKOUT_TIME = 300000; // 5 minutes
  AuthAttempt authAttempts[MAX_AUTH_ATTEMPTS];
  int authAttemptCount;
  
  // Callback for MQTT settings change
  std::function<void()> mqttChangeCallback;
  
  // Callback for Home Assistant settings change
  std::function<void()> haChangeCallback;

public:
  WebConfig(AsyncWebServer* srv, Preferences* prefs, const String& device,
            const String& mfg, const String& version);

  void begin();
  void loadMQTTSettings();
  void loadHomeAssistantSettings();
  void loadMDNSSettings();
  void loadAdminAuth();
  void setupRoutes();

  String getHTMLHeader(const String& title);
  String getHTMLFooter();

  // Authentication helper for protected endpoints
  bool authenticate(AsyncWebServerRequest* request);
  
  // Rate limiting helpers
  bool isRateLimited(const String& clientIP);
  void recordAuthAttempt(const String& clientIP);

  String getMQTTServer() const { return mqttServer; }
  int getMQTTPort() const { return mqttPort; }
  String getMQTTUser() const { return mqttUser; }
  String getMQTTPassword() const { return mqttPassword; }
  String getMQTTClientId() const { return mqttClientId; }
  bool isMQTTEnabled() const { return mqttEnabled; }

  void setDefaultMQTT(bool enabled, const String& server, int port,
                      const String& user, const String& password,
                      const String& clientId) {
    mqttEnabled = enabled;
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    mqttClientId = clientId;
  }
  
  // Callback for MQTT settings change
  void setMQTTChangeCallback(std::function<void()> callback) { mqttChangeCallback = callback; }
  
  // Callback for Home Assistant settings change
  void setHomeAssistantChangeCallback(std::function<void()> callback) { haChangeCallback = callback; }
  
  void setDefaultMDNS(bool enabled, const String& hostname);
  void setDefaultHomeAssistant(bool enabled, const String& discoveryPrefix);
  
  // Getters for new config options
  bool isMDNSEnabled() const { return mdnsEnabled; }
  String getMDNSHostname() const { return mdnsHostname; }
  bool isHomeAssistantEnabled() const { return haEnabled; }
  String getHomeAssistantDiscoveryPrefix() const { return haDiscoveryPrefix; }
};

#endif // DOMOTICS_CORE_WEB_CONFIG_H
