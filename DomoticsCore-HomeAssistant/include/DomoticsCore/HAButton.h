#pragma once

#include "HAEntity.h"
#include <DomoticsCore/Logger.h>

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Home Assistant Button entity
 * 
 * Trigger-only action (restart, calibrate, etc.)
 */
class HAButton : public HAEntity {
public:
    HAButton(const String& id, const String& name,
             const String& icon = "")
        : HAEntity(id, name, "button") {
        this->icon = icon;
    }

    String payloadPress = HAPayload::PRESS;
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        // A button has no state
        doc.remove("stat_t");
        doc.remove("val_tpl");

        char buf[HA_TOPIC_BUF_SIZE];
        // Add button-specific fields
        getCommandTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
        doc["cmd_t"] = buf;
        addUnlessDefault(doc, "pl_prs", payloadPress, HAPayload::PRESS);
    }
    
    /**
     * @brief Handle button press from Home Assistant
     * @param payload Command payload
     * @return true if payload matches payloadPress, false otherwise
     */
    bool handleCommand(const String& payload) override {
        if (payload == payloadPress) {
            DLOG_D(LOG_HA, "Button pressed: %s", id.c_str());
            return true;
        }
        return false;
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
