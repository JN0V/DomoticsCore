#pragma once

#include "IWebUIProvider.h"

namespace DomoticsCore {
namespace Components {

/**
 * Helper macros for conditional WebUI compilation
 * Use these in your components to provide optional WebUI support
 */

// Macro to conditionally inherit from IWebUIProvider
#if DOMOTICSCORE_WEBUI_ENABLED
    #define WEBUI_PROVIDER_INHERITANCE , public IWebUIProvider
    #define WEBUI_OVERRIDE override
#else
    #define WEBUI_PROVIDER_INHERITANCE
    #define WEBUI_OVERRIDE
#endif

// Macro to conditionally implement WebUI methods
#if DOMOTICSCORE_WEBUI_ENABLED
    #define WEBUI_SECTION_METHOD(name, title, icon, category) \
        WebUISection getWebUISection() override { \
            return WebUISection(name, title, icon, category); \
        }
    
    #define WEBUI_REQUEST_METHOD() \
        String handleWebUIRequest(const String& endpoint, const String& method, \
                                const std::map<String, String>& params) override
    
    #define WEBUI_DATA_METHOD() \
        String getWebUIData() override
    
    #define WEBUI_ENABLED_METHOD(enabled) \
        bool isWebUIEnabled() override { return enabled; }
#else
    #define WEBUI_SECTION_METHOD(name, title, icon, category)
    #define WEBUI_REQUEST_METHOD() String handleWebUIRequest_disabled()
    #define WEBUI_DATA_METHOD() String getWebUIData_disabled()
    #define WEBUI_ENABLED_METHOD(enabled)
#endif

// Macro to register component with WebUI (only when enabled)
#if DOMOTICSCORE_WEBUI_ENABLED
    #define WEBUI_REGISTER_PROVIDER(webui_ptr, component_ptr) \
        if (webui_ptr && component_ptr) { \
            webui_ptr->registerProvider(component_ptr); \
        }
#else
    #define WEBUI_REGISTER_PROVIDER(webui_ptr, component_ptr) \
        // WebUI disabled - no registration
#endif

} // namespace Components
} // namespace DomoticsCore
