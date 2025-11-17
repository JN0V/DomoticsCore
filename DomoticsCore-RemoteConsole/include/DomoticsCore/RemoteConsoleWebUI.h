#ifndef DOMOTICS_REMOTE_CONSOLE_WEBUI_H
#define DOMOTICS_REMOTE_CONSOLE_WEBUI_H

#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/RemoteConsole.h>
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief Minimal WebUI provider for RemoteConsole component
 * 
 * Shows RemoteConsole in the component list. No configuration needed.
 */
class RemoteConsoleWebUI : public IWebUIProvider {
private:
    RemoteConsoleComponent* console;
    
public:
    explicit RemoteConsoleWebUI(RemoteConsoleComponent* c) : console(c) {}
    
    String getWebUIName() const override { 
        return console ? console->metadata.name : String("RemoteConsole"); 
    }
    
    String getWebUIVersion() const override { 
        return console ? console->metadata.version : String("1.0.0"); 
    }
    
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> ctxs;
        if (!console) return ctxs;
        
        // Component card
        ctxs.push_back(WebUIContext{
            "console_component", 
            "Remote Console", 
            "dc-plug",
            WebUILocation::ComponentDetail, 
            WebUIPresentation::Card
        }
        .withField(WebUIField("status", "Status", WebUIFieldType::Display, "Active", "", true))
        .withField(WebUIField("port", "Port", WebUIFieldType::Display, "23 (Telnet)", "", true)));
        
        // Settings context
        ctxs.push_back(WebUIContext::settings("console_settings", "Remote Console")
            .withField(WebUIField("port", "Telnet Port", WebUIFieldType::Display, "23"))
            .withField(WebUIField("protocol", "Protocol", WebUIFieldType::Display, "Telnet"))
            .withField(WebUIField("info", "Info", WebUIFieldType::Display, 
                      "Connect via: telnet <device-ip> 23"))
        );
        
        return ctxs;
    }
    
    String getWebUIData(const String& contextId) override {
        if (!console) return "{}";
        
        JsonDocument doc;
        
        if (contextId == "console_component") {
            doc["status"] = "Active";
            doc["port"] = "23 (Telnet)";
        } else if (contextId == "console_settings") {
            doc["port"] = "23";
            doc["protocol"] = "Telnet";
            doc["info"] = "Connect via: telnet <device-ip> 23";
        }
        
        String json;
        serializeJson(doc, json);
        return json;
    }
    
    String handleWebUIRequest(const String& /*contextId*/, const String& /*endpoint*/, 
                             const String& /*method*/, const std::map<String, String>& /*params*/) override {
        return "{\"success\":false}";
    }
    
    bool hasDataChanged(const String& /*contextId*/) override {
        return false;  // Static data, no updates needed
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore

#endif // DOMOTICS_REMOTE_CONSOLE_WEBUI_H
