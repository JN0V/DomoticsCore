<!-- workline
sources: [library.json]
-->

# Project Structure

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
