# DomoticsCore Test Mocks

This directory contains mock implementations for isolated unit testing.

## Purpose

Mocks allow testing components in complete isolation without:
- Real network connections (WiFi, MQTT brokers)
- Real hardware (GPIO, sensors)
- Real time dependencies (NTP servers)

## Available Mocks

| Mock | Replaces | Use Case |
|------|----------|----------|
| `MockWifiHAL` | `Wifi_HAL.h` | Test components needing network status |
| `MockMQTTClient` | `PubSubClient` | Test MQTT publish/subscribe logic |
| `MockEventBus` | `EventBus.h` | Test event emission/subscription |
| `MockStorage` | `StorageComponent` | Test persistence logic |

### Library mocks (`libraries/`)

These carry the real library's file name, so an unchanged `#include <X.h>` finds
the mock instead of the package. A project opts in by putting the directory on
its include path; nothing else sees them.

| Mock | Replaces | Use Case |
|------|----------|----------|
| `libraries/ESPAsyncWebServer.h` | `ESPAsyncWebServer` | Run route and upload handlers on the host |
| `libraries/AsyncEventSource.h` | its SSE half | Assert what a handler pushed |
| `libraries/FS.h` | `FS.h` | Compile headers that name `fs::FS` |
| `libraries/pgmspace.h` | `pgmspace.h` | Compile headers using `PROGMEM` |

## Usage Pattern

```cpp
#include "tests/mocks/MockWifiHAL.h"

void test_component_handles_wifi_disconnect() {
    using namespace DomoticsCore::Mocks;
    
    // Setup
    MockWifiHAL::reset();
    MockWifiHAL::simulateConnect();
    
    // Create component under test
    MyComponent component;
    component.begin();
    
    // Simulate disconnect
    MockWifiHAL::simulateDisconnect();
    component.loop();
    
    // Verify behavior
    TEST_ASSERT_TRUE(component.isInOfflineMode());
}
```

## Design Principles

1. **Static members**: Mocks use static members for easy access without instance management
2. **Reset method**: Every mock has `reset()` to clear state between tests
3. **Simulation methods**: `simulate*()` methods trigger state changes
4. **Verification helpers**: `was*()` and `get*Count()` methods for assertions

## Adding New Mocks

1. Create `Mock<Name>.h` in this directory
2. Use static members for state
3. Implement `reset()` for test isolation
4. Add verification helpers
5. Document in this README
