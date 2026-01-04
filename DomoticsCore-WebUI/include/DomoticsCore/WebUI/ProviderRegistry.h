#pragma once

#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <ArduinoJson.h>

#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/ComponentRegistry.h"
#include "DomoticsCore/Logger.h"
#include "DomoticsCore/Platform_HAL.h"
#include "DomoticsCore/WebUI/StreamingContextSerializer.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @class ProviderRegistry
 * @brief Manages WebUI providers, contexts, and schema generation.
 *
 * Simplified version - no lazy loading overhead.
 * All providers are registered directly.
 */
class ProviderRegistry {
private:
    std::map<String, IWebUIProvider*> contextProviders;
    std::map<IWebUIProvider*, bool> providerEnabled;
    std::map<IWebUIProvider*, IComponent*> providerComponent;
    std::map<String, std::function<IWebUIProvider*(IComponent*)>> providerFactories;
    std::vector<std::unique_ptr<IWebUIProvider>> ownedProviders;

public:
    ProviderRegistry() = default;

    /**
     * @brief Register an IWebUIProvider and index all of its contexts.
     * Uses forEachContext() to avoid copying contexts on memory-constrained devices.
     */
    void registerProvider(IWebUIProvider* provider) {
        if (!provider) return;

        int contextCount = 0;
        provider->forEachContext([this, provider, &contextCount](const WebUIContext& context) {
            contextProviders[context.contextId] = provider;
            DLOG_I(LOG_WEB, "Registered provider for context: %s", context.contextId.c_str());
            contextCount++;
            return true; // continue iteration
        });

        if (contextCount == 0) {
            DLOG_W(LOG_WEB, "Provider has no contexts to register.");
            return;
        }

        // Default to enabled if not already tracked
        if (providerEnabled.find(provider) == providerEnabled.end()) {
            providerEnabled[provider] = true;
        }
    }

    /**
     * @brief Register a provider and remember the owning component for lifecycle callbacks.
     */
    void registerProviderWithComponent(IWebUIProvider* provider, IComponent* component) {
        registerProvider(provider);
        if (provider && component) {
            providerComponent[provider] = component;
        }
    }

    /**
     * @brief Remove all contexts contributed by the given provider without deleting it.
     */
    void unregisterProvider(IWebUIProvider* provider) {
        if (!provider) return;
        for (auto it = contextProviders.begin(); it != contextProviders.end(); ) {
            if (it->second == provider) {
                it = contextProviders.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Register a factory that can create providers for components with a matching type key.
     */
    void registerProviderFactory(const String& typeKey, std::function<IWebUIProvider*(IComponent*)> factory) {
        if (!typeKey.isEmpty() && factory) {
            providerFactories[typeKey] = factory;
        }
    }

    /**
     * @brief Iterate through the registry and register providers (direct or via factories).
     */
    void discoverProviders(const Components::ComponentRegistry& registry) {
        auto comps = registry.getAllComponents();
        for (auto* comp : comps) {
            if (!comp) continue;
            IWebUIProvider* provider = comp->getWebUIProvider();
            if (provider) {
                // Avoid duplicate registration
                bool already = false;
                for (const auto& pair : contextProviders) {
                    if (pair.second == provider) { already = true; break; }
                }
                if (!already) {
                    registerProviderWithComponent(provider, comp);
                }
            } else {
                // Try factory by typeKey for composition-based providers
                const char* key = comp->getTypeKey();
                auto it = providerFactories.find(String(key));
                if (it != providerFactories.end()) {
                    // Create and own the provider instance
                    std::unique_ptr<IWebUIProvider> created(it->second(comp));
                    if (created) {
                        IWebUIProvider* raw = created.get();
                        ownedProviders.push_back(std::move(created));
                        registerProviderWithComponent(raw, comp);
                    }
                }
            }
        }
    }

    // Logic for API /api/components
    void getComponentsList(JsonDocument& doc) {
        JsonArray comps = doc["components"].to<JsonArray>();

        // Track names we've already added to avoid duplicates
        std::vector<String> addedNames;

        // Build a unique list from providerEnabled to include disabled providers as well
        std::vector<IWebUIProvider*> providers;
        providers.reserve(providerEnabled.size());
        for (const auto& kv : providerEnabled) {
            if (kv.first && std::find(providers.begin(), providers.end(), kv.first) == providers.end()) {
                providers.push_back(kv.first);
            }
        }
        // Also include any provider currently present in contexts but missing from map (safety)
        for (const auto& pair : contextProviders) {
            IWebUIProvider* prov = pair.second;
            if (prov && std::find(providers.begin(), providers.end(), prov) == providers.end()) {
                providers.push_back(prov);
            }
        }

        // Add providers
        for (IWebUIProvider* provider : providers) {
            JsonObject compObj = comps.add<JsonObject>();
            String name = provider->getWebUIName();
            compObj["name"] = name;
            compObj["version"] = provider->getWebUIVersion();
            bool enabled = (providerEnabled.find(provider) != providerEnabled.end()) ? providerEnabled[provider] : true;
            compObj["status"] = enabled ? "Enabled" : "Disabled";
            compObj["enabled"] = enabled;
            compObj["canDisable"] = (name != "WebUI");
            addedNames.push_back(name);
        }
    }

    // Logic for API /api/components/enable
    struct EnableResult {
        bool success;
        String name;
        bool enabled;
        String warning;
        bool found;
    };

    EnableResult enableComponent(const String& name, bool enabled) {
        EnableResult result;
        result.name = name;
        result.enabled = enabled;
        result.found = false;
        result.success = false;

        // Disallow disabling WebUI
        if (name == "WebUI" && enabled == false) {
            result.warning = "Disabling WebUI may make the UI inaccessible until reboot/reset.";
            return result;
        }

        // Collect matching providers
        std::vector<IWebUIProvider*> matched;
        for (const auto& kv : contextProviders) {
            if (kv.second && kv.second->getWebUIName() == name) {
                if (std::find(matched.begin(), matched.end(), kv.second) == matched.end()) {
                    matched.push_back(kv.second);
                }
            }
        }
        for (const auto& kv : providerEnabled) {
            IWebUIProvider* prov = kv.first;
            if (prov && prov->getWebUIName() == name) {
                if (std::find(matched.begin(), matched.end(), prov) == matched.end()) {
                    matched.push_back(prov);
                }
            }
        }

        for (IWebUIProvider* provider : matched) {
            providerEnabled[provider] = enabled;
            result.found = true;

            // Lifecycle callbacks
            auto itComp = providerComponent.find(provider);
            if (itComp != providerComponent.end() && itComp->second) {
                if (!enabled) {
                    itComp->second->shutdown();
                } else {
                    itComp->second->begin();
                }
            }

            // Sync registry
            if (!enabled) {
                unregisterProvider(provider);
            } else {
                registerProviderWithComponent(provider, (providerComponent.find(provider) != providerComponent.end()) ? providerComponent[provider] : nullptr);
            }
        }

        result.success = result.found;
        if (name == "WebUI" && enabled == false) {
             result.warning = "Disabling WebUI may make the UI inaccessible until reboot/reset.";
        }
        return result;
    }

    // Accessors
    IWebUIProvider* getProviderForContext(const String& contextId) {
        auto it = contextProviders.find(contextId);
        if (it != contextProviders.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Check if a context ID is registered
     */
    bool hasContext(const String& contextId) const {
        return contextProviders.find(contextId) != contextProviders.end();
    }

    // Helper for schema generation state
    struct SchemaChunkState {
        std::vector<IWebUIProvider*> providers;
        size_t providerIndex = 0;
        size_t contextIndexInProvider = 0;  // Index within current provider's contexts
        bool began = false;
        bool finished = false;
        bool needComma = false;

        // Current context being serialized - OWNED COPY (safe against cache invalidation)
        // We make ONE copy when starting to serialize a context, then release it when done.
        // This is safer than pointers which could become dangling if cache is invalidated.
        WebUIContext currentContext;
        bool hasCurrentContext = false;
        StreamingContextSerializer serializer;
        bool serializingContext = false;
    };

    std::shared_ptr<SchemaChunkState> prepareSchemaGeneration() {
        auto state = std::make_shared<SchemaChunkState>();

        // Build unique provider list
        std::vector<IWebUIProvider*> providers;
        for (const auto& kv : providerEnabled) {
            if (kv.first && std::find(providers.begin(), providers.end(), kv.first) == providers.end()) {
                providers.push_back(kv.first);
            }
        }
        for (const auto& pair : contextProviders) {
            if (pair.second && std::find(providers.begin(), providers.end(), pair.second) == providers.end()) {
                providers.push_back(pair.second);
            }
        }
        state->providers = providers;

        DLOG_I(LOG_WEB, "Schema: %u providers, heap: %u",
               (unsigned)providers.size(), HAL::getFreeHeap());

        return state;
    }

    void handleComponentRemoved(IComponent* comp) {
        if (!comp) return;
        std::vector<IWebUIProvider*> toRemove;
        for (const auto& kv : providerComponent) {
            if (kv.second == comp) {
                toRemove.push_back(kv.first);
            }
        }
        for (auto* prov : toRemove) {
            for (auto it = contextProviders.begin(); it != contextProviders.end(); ) {
                if (it->second == prov) it = contextProviders.erase(it); else ++it;
            }
            providerEnabled.erase(prov);
            providerComponent.erase(prov);
        }
    }

    const std::map<String, IWebUIProvider*>& getContextProviders() const {
        return contextProviders;
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
