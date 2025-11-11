#pragma once

#include "DomoticsCore/Storage.h"
#include "DomoticsCore/IWebUIProvider.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

class StorageWebUI : public IWebUIProvider {
private:
    StorageComponent* storage; // non-owning

public:
    explicit StorageWebUI(StorageComponent* comp) : storage(comp) {}

    String getWebUIName() const override { return storage ? storage->metadata.name : String("Storage"); }
    String getWebUIVersion() const override { return storage ? storage->metadata.version : String("1.0.0"); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> ctxs;
        if (!storage) return ctxs;

        // Component detail
        ctxs.push_back(WebUIContext{
            "storage_component", "Storage", "dc-info", WebUILocation::ComponentDetail, WebUIPresentation::Card
        }
        .withField(WebUIField("namespace", "Namespace", WebUIFieldType::Display, storage->getNamespace(), "", true))
        .withField(WebUIField("entries", "Used Entries", WebUIFieldType::Display, String(storage->getEntryCount()), "", true))
        .withField(WebUIField("free_entries", "Free Entries", WebUIFieldType::Display, String(storage->getFreeEntries()), "", true))
        .withRealTime(5000));

        // Settings section (read-only basics for now)
        ctxs.push_back(WebUIContext::settings("storage_settings", "Storage Settings")
            .withField(WebUIField("namespace", "Namespace", WebUIFieldType::Display, storage->getNamespace(), "", true))
        );

        return ctxs;
    }

    String handleWebUIRequest(const String& /*contextId*/, const String& /*endpoint*/, const String& /*method*/, const std::map<String, String>& /*params*/) override {
        return "{\"success\":false}";
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
