#include <unity.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

#ifdef NATIVE_BUILD
typedef std::string String;
inline unsigned long millis() { static unsigned long t = 0; return t += 10; }

namespace DomoticsCore {
namespace Mocks {

// ============================================================================
// MockMQTTClient for HA tests
// ============================================================================
struct MockMessage {
    std::string topic;
    std::string payload;
    bool retain;
};

class MockMQTTClient {
public:
    static bool connected;
    static std::vector<MockMessage> publishedMessages;
    static std::vector<std::string> subscribedTopics;
    static std::function<void(const char*, const uint8_t*, unsigned int)> callback;
    
    static bool publish(const char* topic, const char* payload, bool retain = false) {
        if (!connected) return false;
        publishedMessages.push_back({topic, payload, retain});
        return true;
    }
    
    static bool subscribe(const char* topic, uint8_t qos = 0) {
        subscribedTopics.push_back(topic);
        return true;
    }
    
    static void simulateConnect() { connected = true; }
    static void simulateDisconnect() { connected = false; }
    static bool isConnected() { return connected; }
    
    static void simulateIncomingMessage(const std::string& topic, const std::string& payload) {
        if (callback) {
            callback(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length());
        }
    }
    
    static void setCallback(std::function<void(const char*, const uint8_t*, unsigned int)> cb) {
        callback = cb;
    }
    
    static void reset() {
        connected = false;
        publishedMessages.clear();
        subscribedTopics.clear();
        callback = nullptr;
    }
    
    static bool wasPublishedTo(const std::string& topicPrefix) {
        for (const auto& m : publishedMessages) {
            if (m.topic.find(topicPrefix) == 0) return true;
        }
        return false;
    }
    
    static std::string getLastPayloadFor(const std::string& topicPrefix) {
        for (auto it = publishedMessages.rbegin(); it != publishedMessages.rend(); ++it) {
            if (it->topic.find(topicPrefix) == 0) return it->payload;
        }
        return "";
    }
    
    static int getPublishCount() { return publishedMessages.size(); }
};

bool MockMQTTClient::connected = false;
std::vector<MockMessage> MockMQTTClient::publishedMessages;
std::vector<std::string> MockMQTTClient::subscribedTopics;
std::function<void(const char*, const uint8_t*, unsigned int)> MockMQTTClient::callback = nullptr;

// ============================================================================
// MockEventBus
// ============================================================================
class MockEventBus {
public:
    static std::vector<std::string> emittedEvents;
    static void emit(const std::string& event) { emittedEvents.push_back(event); }
    static bool wasEmitted(const std::string& event) {
        for (const auto& e : emittedEvents) if (e == event) return true;
        return false;
    }
    static void reset() { emittedEvents.clear(); }
};
std::vector<std::string> MockEventBus::emittedEvents;

} // namespace Mocks
} // namespace DomoticsCore
#endif

using namespace DomoticsCore::Mocks;

// ============================================================================
// HomeAssistant Entity Types (simplified for testing)
// ============================================================================

struct HAEntity {
    std::string id;
    std::string name;
    std::string component;  // sensor, switch, light, etc.
    std::string deviceClass;
    std::string icon;
    std::string unit;
    
    virtual std::string getDiscoveryTopic(const std::string& nodeId, const std::string& prefix) const {
        return prefix + "/" + component + "/" + nodeId + "/" + id + "/config";
    }
    
    virtual std::string getStateTopic(const std::string& nodeId, const std::string& prefix) const {
        return prefix + "/" + component + "/" + nodeId + "/" + id + "/state";
    }
    
    virtual std::string buildDiscoveryPayload(const std::string& nodeId, const std::string& availTopic) const {
        // Simplified JSON - real impl uses ArduinoJson
        std::string json = "{";
        json += "\"name\":\"" + name + "\",";
        json += "\"unique_id\":\"" + nodeId + "_" + id + "\",";
        json += "\"state_topic\":\"" + getStateTopic(nodeId, "homeassistant") + "\",";
        json += "\"availability_topic\":\"" + availTopic + "\"";
        if (!unit.empty()) json += ",\"unit_of_measurement\":\"" + unit + "\"";
        if (!deviceClass.empty()) json += ",\"device_class\":\"" + deviceClass + "\"";
        if (!icon.empty()) json += ",\"icon\":\"" + icon + "\"";
        json += "}";
        return json;
    }
    
    virtual ~HAEntity() = default;
};

// ============================================================================
// HomeAssistant Logic Under Test
// ============================================================================

class HALogicUnderTest {
public:
    std::string nodeId = "esp32_test";
    std::string discoveryPrefix = "homeassistant";
    std::string availabilityTopic;
    bool mqttConnected = false;
    
    std::vector<std::unique_ptr<HAEntity>> entities;
    int discoveryCount = 0;
    int stateUpdateCount = 0;
    
    // Command callbacks
    std::map<std::string, std::function<void(const std::string&)>> commandCallbacks;
    
    HALogicUnderTest() {
        availabilityTopic = discoveryPrefix + "/" + nodeId + "/availability";
    }
    
    void onMQTTConnected() {
        mqttConnected = true;
        setAvailable(true);
        subscribeToCommands();
        if (!entities.empty()) {
            publishDiscovery();
        }
    }
    
    void onMQTTDisconnected() {
        mqttConnected = false;
    }
    
    void addSensor(const std::string& id, const std::string& name, 
                   const std::string& unit = "", const std::string& deviceClass = "") {
        auto entity = std::make_unique<HAEntity>();
        entity->id = id;
        entity->name = name;
        entity->component = "sensor";
        entity->unit = unit;
        entity->deviceClass = deviceClass;
        entities.push_back(std::move(entity));
    }
    
    void addSwitch(const std::string& id, const std::string& name,
                   std::function<void(bool)> callback) {
        auto entity = std::make_unique<HAEntity>();
        entity->id = id;
        entity->name = name;
        entity->component = "switch";
        entities.push_back(std::move(entity));
        
        commandCallbacks[id] = [callback](const std::string& payload) {
            callback(payload == "ON");
        };
    }
    
    void setAvailable(bool available) {
        std::string payload = available ? "online" : "offline";
        MockMQTTClient::publish(availabilityTopic.c_str(), payload.c_str(), true);
    }
    
    void publishDiscovery() {
        for (const auto& entity : entities) {
            std::string topic = entity->getDiscoveryTopic(nodeId, discoveryPrefix);
            std::string payload = entity->buildDiscoveryPayload(nodeId, availabilityTopic);
            MockMQTTClient::publish(topic.c_str(), payload.c_str(), true);
        }
        discoveryCount++;
        MockEventBus::emit("ha/discovery_published");
    }
    
    void publishState(const std::string& entityId, const std::string& state) {
        for (const auto& entity : entities) {
            if (entity->id == entityId) {
                std::string topic = entity->getStateTopic(nodeId, discoveryPrefix);
                MockMQTTClient::publish(topic.c_str(), state.c_str(), false);
                stateUpdateCount++;
                return;
            }
        }
    }
    
    void subscribeToCommands() {
        std::string topic = discoveryPrefix + "/+/" + nodeId + "/+/set";
        MockMQTTClient::subscribe(topic.c_str());
    }
    
    void handleCommand(const std::string& topic, const std::string& payload) {
        // Extract entity ID from topic: homeassistant/switch/esp32_test/relay/set
        size_t lastSlash = topic.rfind('/');
        if (lastSlash == std::string::npos) return;
        
        size_t secondLastSlash = topic.rfind('/', lastSlash - 1);
        if (secondLastSlash == std::string::npos) return;
        
        std::string entityId = topic.substr(secondLastSlash + 1, lastSlash - secondLastSlash - 1);
        
        auto it = commandCallbacks.find(entityId);
        if (it != commandCallbacks.end()) {
            it->second(payload);
        }
    }
    
    int getEntityCount() const { return entities.size(); }
};

// ============================================================================
// Tests
// ============================================================================

HALogicUnderTest* ha = nullptr;

void setUp(void) {
    MockMQTTClient::reset();
    MockEventBus::reset();
    ha = new HALogicUnderTest();
}

void tearDown(void) {
    delete ha;
    ha = nullptr;
}

// T131: Test HA doesn't publish without MQTT
void test_ha_no_publish_without_mqtt(void) {
    ha->addSensor("temp", "Temperature", "°C", "temperature");
    
    // Try to publish discovery (should fail silently)
    ha->publishDiscovery();
    
    TEST_ASSERT_EQUAL(0, MockMQTTClient::getPublishCount());
}

// T132: Test HA discovery message format
void test_ha_discovery_message_format(void) {
    MockMQTTClient::simulateConnect();
    
    ha->addSensor("temperature", "Temperature", "°C", "temperature");
    ha->onMQTTConnected();
    
    // Should have published discovery
    TEST_ASSERT_TRUE(MockMQTTClient::wasPublishedTo("homeassistant/sensor/"));
    
    std::string payload = MockMQTTClient::getLastPayloadFor("homeassistant/sensor/");
    
    // Verify JSON structure contains required fields
    TEST_ASSERT_TRUE(payload.find("\"name\":\"Temperature\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"unique_id\":\"esp32_test_temperature\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"state_topic\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"availability_topic\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"unit_of_measurement\":\"°C\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"device_class\":\"temperature\"") != std::string::npos);
}

// T133: Test HA entity state publishing
void test_ha_entity_state_publishing(void) {
    MockMQTTClient::simulateConnect();
    
    ha->addSensor("humidity", "Humidity", "%");
    ha->onMQTTConnected();
    
    int beforeCount = MockMQTTClient::getPublishCount();
    
    ha->publishState("humidity", "65.5");
    
    TEST_ASSERT_EQUAL(beforeCount + 1, MockMQTTClient::getPublishCount());
    TEST_ASSERT_TRUE(MockMQTTClient::wasPublishedTo("homeassistant/sensor/esp32_test/humidity/state"));
}

// T134: Test HA command handling
void test_ha_command_handling(void) {
    MockMQTTClient::simulateConnect();
    
    bool switchState = false;
    ha->addSwitch("relay", "Relay", [&switchState](bool state) {
        switchState = state;
    });
    
    ha->onMQTTConnected();
    
    // Simulate incoming command
    ha->handleCommand("homeassistant/switch/esp32_test/relay/set", "ON");
    TEST_ASSERT_TRUE(switchState);
    
    ha->handleCommand("homeassistant/switch/esp32_test/relay/set", "OFF");
    TEST_ASSERT_FALSE(switchState);
}

// T135: Test HA availability topic
void test_ha_availability_topic(void) {
    MockMQTTClient::simulateConnect();
    
    ha->onMQTTConnected();
    
    // Should have published "online" to availability topic
    bool found = false;
    for (const auto& msg : MockMQTTClient::publishedMessages) {
        if (msg.topic == "homeassistant/esp32_test/availability" && msg.payload == "online") {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
    
    // Simulate disconnect - should publish offline
    ha->setAvailable(false);
    
    found = false;
    for (const auto& msg : MockMQTTClient::publishedMessages) {
        if (msg.topic == "homeassistant/esp32_test/availability" && msg.payload == "offline") {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

// Additional: Test multiple entities
void test_ha_multiple_entities(void) {
    MockMQTTClient::simulateConnect();
    
    ha->addSensor("temp", "Temperature", "°C");
    ha->addSensor("humidity", "Humidity", "%");
    ha->addSwitch("relay", "Relay", [](bool){});
    
    ha->onMQTTConnected();
    
    TEST_ASSERT_EQUAL(3, ha->getEntityCount());
    // 1 availability + 3 discovery = 4 publishes minimum
    TEST_ASSERT_TRUE(MockMQTTClient::getPublishCount() >= 4);
}

// Additional: Test event emission
void test_ha_emits_discovery_event(void) {
    MockMQTTClient::simulateConnect();
    
    ha->addSensor("temp", "Temperature");
    ha->onMQTTConnected();
    
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("ha/discovery_published"));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ha_no_publish_without_mqtt);
    RUN_TEST(test_ha_discovery_message_format);
    RUN_TEST(test_ha_entity_state_publishing);
    RUN_TEST(test_ha_command_handling);
    RUN_TEST(test_ha_availability_topic);
    RUN_TEST(test_ha_multiple_entities);
    RUN_TEST(test_ha_emits_discovery_event);
    
    return UNITY_END();
}
