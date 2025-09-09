#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/Logger.h>
#include <WiFi.h>

HomeAssistantDiscovery::HomeAssistantDiscovery(PubSubClient* client, const String& devId, 
                                              const String& devName, const String& mfg, const String& version)
  : mqttClient(client), deviceId(devId), deviceName(devName), 
    manufacturer(mfg), firmwareVersion(version), enabled(false) {
}

void HomeAssistantDiscovery::begin(const String& prefix) {
  discoveryPrefix = prefix;
  enabled = true;
}

String HomeAssistantDiscovery::getDeviceConfigJson() const {
  DynamicJsonDocument doc(512);
  
  JsonArray identifiers = doc.createNestedArray("identifiers");
  identifiers.add(deviceId);
  
  doc["name"] = deviceName;
  doc["manufacturer"] = manufacturer;
  doc["model"] = "ESP32 Domotics";
  doc["sw_version"] = firmwareVersion;
  
  // Add network info if available
  if (WiFi.status() == WL_CONNECTED) {
    JsonObject connections = doc.createNestedObject("connections");
    JsonArray mac = connections.createNestedArray("mac");
    mac.add(WiFi.macAddress());
  }
  
  String result;
  serializeJson(doc, result);
  return result;
}

String HomeAssistantDiscovery::getTopicPrefix(const String& component, const String& objectId) const {
  return discoveryPrefix + "/" + component + "/" + deviceId + "/" + objectId;
}

bool HomeAssistantDiscovery::publishDiscoveryMessage(const String& component, const String& objectId, const String& config) {
  if (!enabled || !mqttClient || !mqttClient->connected()) {
    return false;
  }
  
  String topic = getTopicPrefix(component, objectId) + "/config";
  bool result = mqttClient->publish(topic.c_str(), config.c_str(), true); // Retain message
  
  if (result) {
    DLOG_I(LOG_HA, "Published %s entity: %s", component.c_str(), objectId.c_str());
  } else {
    DLOG_E(LOG_HA, "Failed to publish %s entity: %s", component.c_str(), objectId.c_str());
  }
  
  return result;
}

void HomeAssistantDiscovery::publishDevice() {
  if (!enabled) return;
  
  DLOG_I(LOG_HA, "Publishing device information");
  // Device info is included in each entity, no separate device topic needed
}

void HomeAssistantDiscovery::publishSensor(const String& name, const String& friendlyName, 
                                          const String& unit, const String& deviceClass,
                                          const String& stateTopic) {
  if (!enabled) return;
  
  DynamicJsonDocument doc(1024);
  
  // Basic entity info
  doc["name"] = friendlyName.isEmpty() ? name : friendlyName;
  doc["unique_id"] = deviceId + "_" + name;
  doc["object_id"] = deviceId + "_" + name;
  
  // State topic
  String topic = stateTopic.isEmpty() ? getDefaultStateTopic(name) : stateTopic;
  doc["state_topic"] = topic;
  
  // Optional attributes
  if (!unit.isEmpty()) {
    doc["unit_of_measurement"] = unit;
  }
  if (!deviceClass.isEmpty()) {
    doc["device_class"] = deviceClass;
  }
  
  // Device info
  doc["device"] = serialized(getDeviceConfigJson());
  
  String config;
  serializeJson(doc, config);
  publishDiscoveryMessage("sensor", name, config);
}

void HomeAssistantDiscovery::publishSwitch(const String& name, const String& friendlyName,
                                          const String& commandTopic, const String& stateTopic) {
  if (!enabled) return;
  
  DynamicJsonDocument doc(1024);
  
  // Basic entity info
  doc["name"] = friendlyName.isEmpty() ? name : friendlyName;
  doc["unique_id"] = deviceId + "_" + name;
  doc["object_id"] = deviceId + "_" + name;
  
  // Topics
  String cmdTopic = commandTopic.isEmpty() ? getDefaultCommandTopic(name) : commandTopic;
  String stTopic = stateTopic.isEmpty() ? getDefaultStateTopic(name) : stateTopic;
  
  doc["command_topic"] = cmdTopic;
  doc["state_topic"] = stTopic;
  
  // Payloads
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  doc["state_on"] = "ON";
  doc["state_off"] = "OFF";
  
  // Device info
  doc["device"] = serialized(getDeviceConfigJson());
  
  String config;
  serializeJson(doc, config);
  publishDiscoveryMessage("switch", name, config);
}

void HomeAssistantDiscovery::publishBinarySensor(const String& name, const String& friendlyName,
                                                const String& deviceClass, const String& stateTopic) {
  if (!enabled) return;
  
  DynamicJsonDocument doc(1024);
  
  // Basic entity info
  doc["name"] = friendlyName.isEmpty() ? name : friendlyName;
  doc["unique_id"] = deviceId + "_" + name;
  doc["object_id"] = deviceId + "_" + name;
  
  // State topic
  String topic = stateTopic.isEmpty() ? getDefaultStateTopic(name) : stateTopic;
  doc["state_topic"] = topic;
  
  // Payloads
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  
  // Optional device class
  if (!deviceClass.isEmpty()) {
    doc["device_class"] = deviceClass;
  }
  
  // Device info
  doc["device"] = serialized(getDeviceConfigJson());
  
  String config;
  serializeJson(doc, config);
  publishDiscoveryMessage("binary_sensor", name, config);
}

void HomeAssistantDiscovery::removeEntity(const String& component, const String& name) {
  if (!enabled || !mqttClient || !mqttClient->connected()) {
    return;
  }
  
  String topic = getTopicPrefix(component, name) + "/config";
  mqttClient->publish(topic.c_str(), "", true); // Empty retained message removes entity
  
  DLOG_I(LOG_HA, "Removed %s entity: %s", component.c_str(), name.c_str());
}

void HomeAssistantDiscovery::removeAllEntities() {
  // Note: This is a simplified approach. In practice, you'd need to track all published entities
  DLOG_W(LOG_HA, "Removing all entities (manual cleanup required)");
}

String HomeAssistantDiscovery::getDefaultStateTopic(const String& entityName) const {
  return "jnov/" + deviceId + "/" + entityName + "/state";
}

String HomeAssistantDiscovery::getDefaultCommandTopic(const String& entityName) const {
  return "jnov/" + deviceId + "/" + entityName + "/cmd";
}
