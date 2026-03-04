# DomoticsCore-NTP Component

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Overview

DomoticsCore-NTP provides automatic network time synchronization for ESP32 and ESP8266 devices within the DomoticsCore framework. It wraps the platform SNTP client behind a Hardware Abstraction Layer (HAL), offering a unified API for time access, timezone management, and uptime tracking.

## Key Features

- **Multi-server NTP** -- configure up to three NTP servers with automatic fallback.
- **POSIX TZ timezone strings** -- full timezone and DST support via standard POSIX format.
- **Timezone presets** -- built-in constants for UTC, EST, CST, MST, PST, CET, GMT, JST, AEST, IST, NZST.
- **Formatted time output** -- `strftime`-compatible formatting and ISO 8601 strings.
- **Uptime tracking** -- millisecond-precision uptime with human-readable formatting.
- **Sync callbacks and events** -- register callbacks or subscribe to EventBus events (`ntp/synced`, `ntp/sync_failed`).
- **WebUI integration** -- real-time clock header, settings panel, and timezone selector via `NTPWebUI`.
- **Header-only** -- no separate compilation unit required.
- **HAL-isolated** -- platform-specific code confined to `NTP_ESP32.h`, `NTP_ESP8266.h`, and `NTP_Stub.h`.

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>

using namespace DomoticsCore::Components;

Core core;

void setup() {
    Serial.begin(115200);

    NTPConfig cfg;
    cfg.timezone = Timezones::CET;
    cfg.syncInterval = 3600;
    cfg.servers = {"pool.ntp.org", "time.google.com"};

    auto ntp = std::make_unique<NTPComponent>(cfg);
    auto* ntpPtr = ntp.get();

    ntpPtr->onSync([ntpPtr](bool success) {
        if (success) {
            Serial.printf("Time synced: %s\n", ntpPtr->getFormattedTime().c_str());
        }
    });

    core.addComponent(std::move(ntp));
    core.begin();
}

void loop() {
    core.loop();
}
```

## Installation

Add to `platformio.ini`:

```ini
lib_deps =
    DomoticsCore-Core @ >=1.0.0
    DomoticsCore-NTP @ >=1.3.0
```

## Dependencies

| Dependency | Version | Required |
|---|---|---|
| DomoticsCore-Core | >= 1.0.0 | Yes |
| DomoticsCore-WebUI | >= 0.1.0 | No (for WebUI only) |

## Examples

| Example | Description | Path |
|---|---|---|
| BasicNTP | Minimal time sync with formatted output | `DomoticsCore-NTP/examples/BasicNTP/` |
| NTPWithWebUI | Full web interface for configuration and monitoring | `DomoticsCore-NTP/examples/NTPWithWebUI/` |

## Further Reading

- [Technical Reference](technical-reference.md) -- full API documentation, configuration details, and WebUI provider reference.
- [Project Context](project-context.md) -- AI-oriented context file with file inventory, dependency map, and constitution compliance notes.
- [HAL Architecture](../../architecture/hal-architecture.md) -- how the NTP HAL fits into the DomoticsCore platform abstraction model.
- [Component Lifecycle](../../architecture/component-lifecycle.md) -- `begin()` / `loop()` / `shutdown()` lifecycle contract.

## License

MIT License -- see the repository root LICENSE file for details.
