<!-- workline
sources: [DomoticsCore-OTA/include/DomoticsCore/OTA.h, DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h, DomoticsCore-LED/include/DomoticsCore/LED.h, DomoticsCore-System/include/DomoticsCore/SystemPersistence.h]
checked: 500862d
verified: agent:documentalist
-->
# Component Configuration Pattern -- Components Needing Full Pattern

Part of the [Component Configuration Pattern](component-configuration-pattern.md).

## ⚠️ Components Needing Full Pattern

| Component | Status | Note |
|-----------|--------|------|
| **OTA** | Has config struct | Needs `getConfig()` + `setConfig()` + Storage |
| **RemoteConsole** | Simple config | Currently static (port 23) |
| **LED** | Has config struct | Needs Storage integration |

