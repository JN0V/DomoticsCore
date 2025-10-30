#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/ComponentRegistry.h"

namespace DomoticsCore {
namespace Components {

DomoticsCore::Core* IComponent::getCore() const {
    if (!__dc_core && __dc_registry) {
        // Lazy injection: get Core from registry on first access
        __dc_core = const_cast<DomoticsCore::Core*>(__dc_registry->getCore());
    }
    return __dc_core;
}

} // namespace Components
} // namespace DomoticsCore
