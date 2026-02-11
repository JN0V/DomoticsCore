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
    struct ProviderInfo {
        IComponent* component = nullptr;
        bool enabled = true;
    };
    std::map<IWebUIProvider*, ProviderInfo> providerInfo_;
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
            contextProviders[context.getContextIdCStr()] = provider;
            DLOG_I(LOG_WEB, "Registered provider for context: %s", context.getContextIdCStr());
            contextCount++;
            return true; // continue iteration
        });

        if (contextCount == 0) {
            DLOG_W(LOG_WEB, "Provider has no contexts to register.");
            return;
        }

        // Default to enabled if not already tracked
        if (providerInfo_.find(provider) == providerInfo_.end()) {
            providerInfo_[provider] = ProviderInfo();
        }
    }

    /**
     * @brief Register a provider and remember the owning component for lifecycle callbacks.
     */
    void registerProviderWithComponent(IWebUIProvider* provider, IComponent* component) {
        registerProvider(provider);
        if (provider) {
            providerInfo_[provider].component = component;
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
    static constexpr uint32_t MIN_HEAP_FOR_DISCOVERY = 2048;

    void discoverProviders(const Components::ComponentRegistry& registry) {
        auto comps = registry.getAllComponents();
        DLOG_I(LOG_WEB, "discoverProviders: %d components", (int)comps.size());
        int registered = 0;
        int skipped = 0;
        for (auto* comp : comps) {
            if (!comp) continue;

            // Heap guard: stop discovery if heap is critically low
            uint32_t freeHeap = HAL::Platform::getFreeHeap();
            if (freeHeap < MIN_HEAP_FOR_DISCOVERY) {
                DLOG_W(LOG_WEB, "Low heap (%u bytes), stopping provider discovery", (unsigned)freeHeap);
                skipped = (int)comps.size() - registered;
                break;
            }

            DLOG_D(LOG_WEB, "Checking component: %s", comp->metadata.name);
            IWebUIProvider* provider = comp->getWebUIProvider();
            if (provider) {
                DLOG_D(LOG_WEB, "Component %s has provider", comp->metadata.name);
                // Avoid duplicate registration
                bool already = false;
                for (const auto& pair : contextProviders) {
                    if (pair.second == provider) { already = true; break; }
                }
                if (!already) {
                    registerProviderWithComponent(provider, comp);
                    registered++;
                } else {
                    DLOG_W(LOG_WEB, "Provider already registered for %s", comp->metadata.name);
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
                        registered++;
                    }
                }
            }
        }
        DLOG_I(LOG_WEB, "Discovery done: %d providers registered, %d skipped (heap: %u)",
               registered, skipped, (unsigned)HAL::Platform::getFreeHeap());
    }

    // Logic for API /api/components
    void getComponentsList(JsonDocument& doc) {
        JsonArray comps = doc["components"].to<JsonArray>();

        // Track names we've already added to avoid duplicates
        std::vector<String> addedNames;

        // Build a unique list from providerEnabled to include disabled providers as well
        std::vector<IWebUIProvider*> providers;
        providers.reserve(providerInfo_.size());
        for (const auto& kv : providerInfo_) {
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
            auto infoIt = providerInfo_.find(provider);
            bool enabled = (infoIt != providerInfo_.end()) ? infoIt->second.enabled : true;
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
        for (const auto& kv : providerInfo_) {
            IWebUIProvider* prov = kv.first;
            if (prov && prov->getWebUIName() == name) {
                if (std::find(matched.begin(), matched.end(), prov) == matched.end()) {
                    matched.push_back(prov);
                }
            }
        }

        for (IWebUIProvider* provider : matched) {
            providerInfo_[provider].enabled = enabled;
            result.found = true;

            // Lifecycle callbacks
            auto infoIt = providerInfo_.find(provider);
            if (infoIt != providerInfo_.end() && infoIt->second.component) {
                if (!enabled) {
                    infoIt->second.component->shutdown();
                } else {
                    infoIt->second.component->begin();
                }
            }

            // Sync registry
            if (!enabled) {
                unregisterProvider(provider);
            } else {
                auto pi = providerInfo_.find(provider);
                registerProviderWithComponent(provider, (pi != providerInfo_.end()) ? pi->second.component : nullptr);
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

        // Current context being serialized - POINTER to cached context (zero-copy)
        // WARNING: Do NOT call invalidateContextCache() during active HTTP request!
        // The pointer references the provider's cache which must remain valid.
        const WebUIContext* currentContextPtr = nullptr;
        StreamingContextSerializer serializer;
        bool serializingContext = false;
    };

    std::shared_ptr<SchemaChunkState> prepareSchemaGeneration() {
        auto state = std::make_shared<SchemaChunkState>();

        // Build unique provider list
        std::vector<IWebUIProvider*> providers;
        providers.reserve(providerInfo_.size() + contextProviders.size());
        for (const auto& kv : providerInfo_) {
            if (kv.first && std::find(providers.begin(), providers.end(), kv.first) == providers.end()) {
                providers.push_back(kv.first);
            }
        }
        for (const auto& pair : contextProviders) {
            if (pair.second && std::find(providers.begin(), providers.end(), pair.second) == providers.end()) {
                providers.push_back(pair.second);
            }
        }
        state->providers = std::move(providers);

        DLOG_D(LOG_WEB, "Schema: %u providers, heap: %u",
               (unsigned)state->providers.size(), HAL::getFreeHeap());

        return state;
    }

    void handleComponentRemoved(IComponent* comp) {
        if (!comp) return;
        std::vector<IWebUIProvider*> toRemove;
        for (const auto& kv : providerInfo_) {
            if (kv.second.component == comp) {
                toRemove.push_back(kv.first);
            }
        }
        for (auto* prov : toRemove) {
            for (auto it = contextProviders.begin(); it != contextProviders.end(); ) {
                if (it->second == prov) it = contextProviders.erase(it); else ++it;
            }
            providerInfo_.erase(prov);
        }
    }

    const std::map<String, IWebUIProvider*>& getContextProviders() const {
        return contextProviders;
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
