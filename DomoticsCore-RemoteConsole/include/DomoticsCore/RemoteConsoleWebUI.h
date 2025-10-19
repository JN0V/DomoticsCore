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
        return console ? console->getName() : String("RemoteConsole"); 
    }
    
    String getWebUIVersion() const override { return String("1.0.0"); }
    
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> ctxs;
        if (!console) return ctxs;
        
        // Minimal component card - just shows it exists
        ctxs.push_back(WebUIContext{
            "console_component", 
            "Remote Console", 
            "dc-plug",  // Using plug icon (connection icon)
            WebUILocation::ComponentDetail, 
            WebUIPresentation::Card
        }
        .withField(WebUIField("status", "Status", WebUIFieldType::Display, "Active", "", true))
        .withField(WebUIField("port", "Port", WebUIFieldType::Display, "23 (Telnet)", "", true)));
        
        return ctxs;
    }
    
    String getWebUIData(const String& contextId) override {
        if (!console || contextId != "console_component") return "{}";
        
        JsonDocument doc;
        doc["status"] = "Active";
        doc["port"] = "23 (Telnet)";
        
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
