<!-- workline
sources: [DomoticsCore-OTA/include/DomoticsCore/OTA.h, DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h, DomoticsCore-LED/include/DomoticsCore/LED.h, DomoticsCore-System/include/DomoticsCore/SystemPersistence.h]
checked: 500862d
verified: agent:documentalist
judged-in-parts: c9271a1
-->
# Component Configuration Pattern -- Components Needing Full Pattern

Part of the [Component Configuration Pattern](component-configuration-pattern.md).

## ⚠️ Components Needing Full Pattern

| Component | Status | Note |
|-----------|--------|------|
| **RemoteConsole** | Simple config | Port changeable at runtime via `setPort()` |
| **LED** | Has config struct | Needs Storage integration |

