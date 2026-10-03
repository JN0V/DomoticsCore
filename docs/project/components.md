<!-- workline
sources: [DomoticsCore-Core/library.json, DomoticsCore-HomeAssistant/library.json, DomoticsCore-LED/library.json, DomoticsCore-MQTT/library.json, DomoticsCore-NTP/library.json, DomoticsCore-OTA/library.json, DomoticsCore-RemoteConsole/library.json, DomoticsCore-Storage/library.json, DomoticsCore-System/library.json, DomoticsCore-SystemInfo/library.json, DomoticsCore-WebUI/library.json, DomoticsCore-Wifi/library.json]
-->

# Components and Project Layout

## 🧩 Available Components

| Component | Version | Description | Size | Status |
|-----------|---------|-------------|------|--------|
| **Core** | 1.13.1 | Framework, registry, event bus, MemoryManager, HeapTracker | ~50KB | ✅ Stable |
| **System** | 1.10.0 | High-level orchestration (batteries included) | ~100KB | ✅ Stable |
| **WiFi** | 1.7.0 | Network connectivity with AP fallback | ~40KB | ✅ Stable |
| **LED** | 1.6.0 | Visual status indicators (6 effects) | ~20KB | ✅ Stable |
| **Storage** | 1.6.1 | NVS / LittleFS persistent data | ~30KB | ✅ Stable |
| **RemoteConsole** | 1.7.2 | Telnet debugging console with WebUI integration | ~25KB | ✅ Stable |
| **WebUI** | 1.12.0 | Web interface with WebSocket + SSE dual-mode | ~150KB | ✅ Stable |
| **MQTT** | 1.10.0 | Message broker with auto-reconnect | ~40KB | ✅ Stable |
| **NTP** | 1.4.2 | Time synchronization | ~15KB | ✅ Stable |
| **OTA** | 1.11.0 | Over-the-air updates | ~30KB | ✅ Stable |
| **HomeAssistant** | 2.6.0 | Auto-discovery integration | ~20KB | ✅ Stable |
| **SystemInfo** | 1.7.0 | Real-time monitoring with charts | ~25KB | ✅ Stable |

**Total with everything:** ~545KB flash, ~50KB RAM

## 🌐 Platform Compatibility

DomoticsCore includes a **Hardware Abstraction Layer (HAL)** for platform portability.

| Platform | Status | WiFi | Storage | NTP | Full Framework |
|----------|--------|------|---------|-----|----------------|
| **ESP32** | ✅ Full Support | ✅ | ✅ NVS | ✅ SNTP | ✅ |
| **ESP32-C3** | ✅ Full Support | ✅ | ✅ NVS | ✅ SNTP | ✅ (USB CDC) |
| **ESP8266** | ⚠️ Partial | ✅ | ✅ LittleFS | ✅ sntp | ⚠️ (~80KB RAM, optimized) |
| **AVR** | ❌ Not Suitable | ❌ | ❌ | ❌ | ❌ (2KB RAM) |
| **ARM** | 🔬 Experimental | ⚠️ shields | ⚠️ | ⚠️ | ⚠️ |

> **Note:** Arduino UNO (AVR ATmega328P) has only 2KB RAM - not enough for EventBus, JSON, or WebUI. Only LEDComponent could theoretically work.

HAL headers are in `DomoticsCore-Core/include/DomoticsCore/HAL/`:
- `Platform.h` - Platform detection macros
- `WiFi.h` - Unified WiFi interface
- `Storage.h` - Key-value storage abstraction
- `NTP.h` - Time synchronization

## 📁 Project Structure

```
DomoticsCore/                      # Monorepo with 12 component packages
├── DomoticsCore-Core/             # Essential framework, MemoryManager, HeapTracker
├── DomoticsCore-System/           # High-level orchestration (batteries included)
├── DomoticsCore-Wifi/             # Network connectivity
├── DomoticsCore-LED/              # Visual status indicators
├── DomoticsCore-Storage/          # Persistent data (NVS / LittleFS)
├── DomoticsCore-RemoteConsole/    # Telnet debugging console
├── DomoticsCore-WebUI/            # Web interface with WebSocket + SSE
├── DomoticsCore-MQTT/             # Message broker client
├── DomoticsCore-NTP/              # Time synchronization
├── DomoticsCore-OTA/              # Firmware updates
├── DomoticsCore-HomeAssistant/    # Auto-discovery integration
├── DomoticsCore-SystemInfo/       # System monitoring
├── docs/                          # Guides, architecture, reference docs
├── tests/                         # Unit tests and mocks
├── examples/                      # Examples index
├── specs/                         # Feature specifications
└── tools/                         # Version management scripts

Each component has:
├── include/                       # Public headers
├── src/                           # Implementation (if needed)
├── examples/                      # Working examples
├── README.md                      # Component documentation
└── library.json                   # Package metadata
```

## More Examples

### Minimal Core

**Location:** `DomoticsCore-Core/examples/01-CoreOnly/`

Bare minimum:
- ✅ Component registry
- ✅ Logging system
- ✅ Non-blocking timers

**Binary:** ~250KB flash, ~15KB RAM

### LED Status Patterns

**Location:** `DomoticsCore-LED/examples/BasicLED/`

Demonstrates all LED effects:
- Solid on/off
- Blink (configurable speed)
- Fade in/out
- Pulse/heartbeat
- Breathing
- Rainbow cycle

### Component Development

**Location:** `DomoticsCore-Core/examples/02-CoreWithDummyComponent/`

Learn to build custom components:
- Component lifecycle (begin/loop/shutdown)
- Dependency declaration
- Configuration management
- Health monitoring

## 🙏 Acknowledgments

Built on top of excellent ESP32 ecosystem:
- **Arduino Core for ESP32**
- **ESPAsyncWebServer** (3.x)
- **AsyncTCP** (3.x)
- **PubSubClient** (MQTT)
- **ArduinoJson** (7.x)
