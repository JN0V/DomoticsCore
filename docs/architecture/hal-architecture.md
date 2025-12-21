# HAL Architecture: Routing Header + Platform Files

## Overview

DomoticsCore uses a Hardware Abstraction Layer (HAL) architecture that separates platform-specific code into dedicated files per platform. This enables:

- Clean separation of concerns
- Easy addition of new platforms (ESP32-C3, ESP32-H2, etc.)
- Better testability
- Reduced `#if` block complexity in individual files

## File Structure Pattern

```
ComponentName_HAL.h       → Routing header (platform detection + include)
ComponentName_ESP32.h     → ESP32 implementation
ComponentName_ESP8266.h   → ESP8266 implementation
ComponentName_ESP32C3.h   → Future: ESP32-C3
ComponentName_ESP32H2.h   → Future: ESP32-H2
ComponentName_Stub.h      → Fallback for unsupported platforms
```

## Routing Header Example

```cpp
// SystemInfo_HAL.h
#pragma once
#include "DomoticsCore/Platform_HAL.h"

#if DOMOTICS_PLATFORM_ESP32
    #include "SystemInfo_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "SystemInfo_ESP8266.h"
#elif DOMOTICS_PLATFORM_ESP32C3
    #include "SystemInfo_ESP32C3.h"
#elif DOMOTICS_PLATFORM_ESP32H2
    #include "SystemInfo_ESP32H2.h"
#else
    #include "SystemInfo_Stub.h"
#endif
```

## Platform Implementation Guidelines

Each `*_Platform.h` file MUST:

1. **Be self-contained**: Include all necessary platform-specific headers
2. **Follow the same namespace**: `DomoticsCore::HAL::ComponentName`
3. **Implement the same interface**: All platforms expose identical functions
4. **Stay under 800 lines**: Per constitution file size limits
5. **Use inline functions**: For header-only compatibility

## Adding a New Platform

1. Create `ComponentName_NewPlatform.h` with platform-specific implementation
2. Add platform detection to `Platform_HAL.h`
3. Update routing headers to include the new platform file
4. Add tests for the new platform

## Current HAL Components

| Component | ESP32 | ESP8266 | Stub |
|-----------|-------|---------|------|
| Platform | ✅ | ✅ | ✅ |
| Storage | ✅ | ✅ | ✅ |
| Wifi | ✅ | ✅ | ✅ |
| NTP | ✅ | ✅ | ✅ |
| SystemInfo | ✅ | ✅ | ✅ |

## Constitution Compliance

This architecture follows:
- **Principle IX (HAL)**: Hardware code in `*_HAL` files only
- **Principle VII (File Size)**: Each file < 800 lines
- **Principle I (SOLID/SRP)**: One platform per file
