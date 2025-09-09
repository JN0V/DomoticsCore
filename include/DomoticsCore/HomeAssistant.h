/**
 * @file HomeAssistant.h
 * @brief Home Assistant MQTT Auto-Discovery integration for ESP32 domotics system
 */

#ifndef DOMOTICS_CORE_HOME_ASSISTANT_H
#define DOMOTICS_CORE_HOME_ASSISTANT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/**
 * @brief Home Assistant Auto-Discovery integration
 * 
 * Provides MQTT-based auto-discovery for Home Assistant integration.
 * Supports sensors, switches, binary sensors, and device information.
 */
class HomeAssistantDiscovery {
private:
  PubSubClient* mqttClient;
  String deviceId;
  String deviceName;
  String manufacturer;
  String firmwareVersion;
  String discoveryPrefix;
  bool enabled;

  // Helper methods
  String getDeviceConfigJson() const;
  String getTopicPrefix(const String& component, const String& objectId) const;
  bool publishDiscoveryMessage(const String& component, const String& objectId, const String& config);

public:
  /**
   * @brief Constructor
   * @param client MQTT client instance
   * @param devId Unique device identifier (MAC-based)
   * @param devName Device name
   * @param mfg Manufacturer name
   * @param version Firmware version
   */
  HomeAssistantDiscovery(PubSubClient* client, const String& devId, 
                        const String& devName, const String& mfg, const String& version);

  /**
   * @brief Initialize Home Assistant discovery
   * @param prefix Discovery prefix (default: "homeassistant")
   */
  void begin(const String& prefix = "homeassistant");

  /**
   * @brief Enable/disable Home Assistant integration
   */
  void setEnabled(bool enable) { enabled = enable; }
  bool isEnabled() const { return enabled; }

  /**
   * @brief Publish device information to Home Assistant
   */
  void publishDevice();

  /**
   * @brief Publish sensor entity
   * @param name Entity name (e.g., "temperature")
   * @param friendlyName Human-readable name
   * @param unit Unit of measurement (e.g., "°C")
   * @param deviceClass Device class (e.g., "temperature")
   * @param stateTopic MQTT topic for state updates
   */
  void publishSensor(const String& name, const String& friendlyName, 
                    const String& unit = "", const String& deviceClass = "",
                    const String& stateTopic = "");

  /**
   * @brief Publish switch entity
   * @param name Entity name (e.g., "relay1")
   * @param friendlyName Human-readable name
   * @param commandTopic MQTT topic for commands
   * @param stateTopic MQTT topic for state updates
   */
  void publishSwitch(const String& name, const String& friendlyName,
                    const String& commandTopic = "", const String& stateTopic = "");

  /**
   * @brief Publish binary sensor entity
   * @param name Entity name (e.g., "motion")
   * @param friendlyName Human-readable name
   * @param deviceClass Device class (e.g., "motion")
   * @param stateTopic MQTT topic for state updates
   */
  void publishBinarySensor(const String& name, const String& friendlyName,
                          const String& deviceClass = "", const String& stateTopic = "");

  /**
   * @brief Remove entity from Home Assistant
   * @param component Component type (sensor, switch, binary_sensor)
   * @param name Entity name
   */
  void removeEntity(const String& component, const String& name);

  /**
   * @brief Remove all entities for this device
   */
  void removeAllEntities();

  /**
   * @brief Get default state topic for entity
   */
  String getDefaultStateTopic(const String& entityName) const;

  /**
   * @brief Get default command topic for entity
   */
  String getDefaultCommandTopic(const String& entityName) const;
};

#endif // DOMOTICS_CORE_HOME_ASSISTANT_H
