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
| `MockAsyncWebServer` | `ESPAsyncWebServer` | Test WebUI routes |

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
