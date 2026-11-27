#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/ComponentRegistry.h"

namespace DomoticsCore {
namespace Components {

DomoticsCore::Core* IComponent::getCore() const {
    if (!dc_core && dc_registry) {
        // Lazy injection: get Core from registry on first access
        dc_core = const_cast<DomoticsCore::Core*>(dc_registry->getCore());
    }
    return dc_core;
}

} // namespace Components
} // namespace DomoticsCore
