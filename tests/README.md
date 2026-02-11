# DomoticsCore Tests

Unit tests and integration tests for DomoticsCore framework features.

## Test Architecture

DomoticsCore uses two test approaches:

1. **Component-level isolated tests** (`DomoticsCore-*/test/`) - Run on native platform with mocks, no hardware needed
2. **Framework-level unit tests** (`tests/unit/`) - Feature-specific tests for core framework behavior

## Component Isolated Tests

Each component has its own `test/` directory with tests that run on the native platform using mock infrastructure.

### Core (`DomoticsCore-Core/test/`)
| Test | Description |
|------|-------------|
| `test_component_registry` | Component registration, dependency resolution, lifecycle |
| `test_eventbus` | EventBus publish/subscribe, sticky events, unsubscribe |
| `test_heap_tracker` | HeapTracker memory leak detection |
| `test_lifecycle_events` | Component lifecycle event ordering |
| `test_system_ready` | System ready event propagation |
| `test_timer` | Non-blocking timer functionality |

### WiFi (`DomoticsCore-Wifi/test/`)
| Test | Description |
|------|-------------|
| `test_wifi_component` | WiFi component initialization and state |
| `test_wifi_webui` | WiFi WebUI provider schema and data |

### MQTT (`DomoticsCore-MQTT/test/`)
| Test | Description |
|------|-------------|
| `test_mqtt_component` | MQTT connection, publish, subscribe |

### NTP (`DomoticsCore-NTP/test/`)
| Test | Description |
|------|-------------|
| `test_ntp_component` | NTP sync, timezone, formatted time |

### HomeAssistant (`DomoticsCore-HomeAssistant/test/`)
| Test | Description |
|------|-------------|
| `test_ha_component` | HA discovery, entity registration, availability |

### OTA (`DomoticsCore-OTA/test/`)
| Test | Description |
|------|-------------|
| `test_ota_component` | OTA update lifecycle and version checking |

### WebUI (`DomoticsCore-WebUI/test/`)
| Test | Description |
|------|-------------|
| `test_webui_component` | WebUI initialization, provider registration |
| `test_schema_memory` | Schema serialization memory usage |
| `test_streaming_serializer` | Chunked streaming serializer |
| `test_heap_esp8266` | ESP8266 heap usage validation |

### Storage (`DomoticsCore-Storage/test/`)
| Test | Description |
|------|-------------|
| `test_storage_api` | Storage get/set/remove operations |
| `test_storage_events` | Storage EventBus integration |
| `test_heap_esp8266` | ESP8266 heap usage validation |

### SystemInfo (`DomoticsCore-SystemInfo/test/`)
| Test | Description |
|------|-------------|
| `test_systeminfo_api` | SystemInfo API and metrics |
| `test_systeminfo_boot` | Boot diagnostics capture |
| `test_systeminfo_metrics` | System metrics collection |

### RemoteConsole (`DomoticsCore-RemoteConsole/test/`)
| Test | Description |
|------|-------------|
| `test_remoteconsole_component` | RemoteConsole initialization and commands |

### LED (`DomoticsCore-LED/test/`)
| Test | Description |
|------|-------------|
| `test_led_types` | LED effect types and configuration |

## Framework Unit Tests (`tests/unit/`)

```
tests/unit/
├── 01-optional-dependencies/    # v1.0.3: Optional dependency support
├── 02-lifecycle-callback/       # v1.1: afterAllComponentsReady() lifecycle
├── 05-storage-namespace/        # Storage namespace isolation
└── 06-webui-refactor/           # WebUI refactoring tests
```

**Note**: Tests 03 and 04 (early-init bug reproductions) were deleted in v1.2.x as the early-init anti-pattern was eliminated from the codebase.

## Mock Infrastructure (`tests/mocks/`)

Mock implementations for isolated unit testing without hardware or network dependencies.

| Mock | Description |
|------|-------------|
| `MockWifiHAL.h` | WiFi HAL simulation |
| `MockMQTTClient.h` | MQTT client simulation |
| `MockEventBus.h` | EventBus mock for isolated testing |
| `MockStorage.h` | Storage mock (in-memory key-value) |
| `MockNTPClient.h` | NTP client simulation |
| `MockAsyncWebServer.h` | AsyncWebServer mock for WebUI tests |

See [`tests/mocks/README.md`](mocks/README.md) for usage details.

## Running Tests

### Isolated component tests (recommended)

```bash
# Run all tests for a component
cd DomoticsCore-Core && pio test -e native

# Run a specific test
cd DomoticsCore-MQTT && pio test -e native -f test_mqtt_component

# Run all tests via CI script
./tools/local_ci.sh
```

### Framework unit tests (require ESP32 hardware)

```bash
cd tests/unit/01-optional-dependencies
pio run -t upload -t monitor
```

## Adding New Tests

### Component test (isolated, native platform)
1. Create `DomoticsCore-{Component}/test/test_{name}/test_{name}.cpp`
2. Use Unity test framework and existing mocks
3. Ensure it runs on native platform without hardware

### Framework test
1. Create `tests/unit/XX-test-name/`
2. Add `platformio.ini` with Core dependency
3. Create `src/main.cpp` with test logic
4. Update this README

## CI Integration

The `tools/local_ci.sh` script runs all isolated tests automatically on native platform. It can be integrated into GitHub Actions:

```yaml
- name: Run Isolated Tests
  run: ./tools/local_ci.sh
```
