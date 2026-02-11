# DomoticsCore Examples

The examples for DomoticsCore are organized by component to keep them relevant and focused.

## "All-in-One" Starter Kits

For complete, ready-to-use examples that combine WiFi, MQTT, WebUI, and more, please refer to the **System** component examples:

**[DomoticsCore-System Examples](../DomoticsCore-System/examples/)**

- **[Minimal](../DomoticsCore-System/examples/Minimal/)**: The simplest way to get started.
- **[Standard](../DomoticsCore-System/examples/Standard/)**: A standard setup with common features enabled.
- **[FullStack](../DomoticsCore-System/examples/FullStack/)**: A production-ready example with all features (HA, OTA, etc.).

## Component-Specific Examples

If you want to use a specific component in isolation (e.g., just the WiFi manager or just the WebUI), check the `examples/` folder inside each component's directory:

### Core
- **[01-CoreOnly](../DomoticsCore-Core/examples/01-CoreOnly/)** - Bare minimum framework
- **[02-CoreWithDummyComponent](../DomoticsCore-Core/examples/02-CoreWithDummyComponent/)** - Custom component example
- **[03-EventBusBasics](../DomoticsCore-Core/examples/03-EventBusBasics/)** - Publish/subscribe messaging
- **[04-EventBusCoordinators](../DomoticsCore-Core/examples/04-EventBusCoordinators/)** - Event coordination patterns

### WiFi
- **[BasicWifi](../DomoticsCore-Wifi/examples/BasicWifi/)** - Simple WiFi connection
- **[WifiWithWebUI](../DomoticsCore-Wifi/examples/WifiWithWebUI/)** - WiFi with web interface

### WebUI
- **[HeadlessAPI](../DomoticsCore-WebUI/examples/HeadlessAPI/)** - API-only mode
- **[WebUIOnly](../DomoticsCore-WebUI/examples/WebUIOnly/)** - Standalone web interface

### MQTT
- **[BasicMQTT](../DomoticsCore-MQTT/examples/BasicMQTT/)** - Basic MQTT connection
- **[MQTTWithWebUI](../DomoticsCore-MQTT/examples/MQTTWithWebUI/)** - MQTT with web interface
- **[MQTTWifiWithWebUI](../DomoticsCore-MQTT/examples/MQTTWifiWithWebUI/)** - MQTT + WiFi + WebUI

### HomeAssistant
- **[BasicHA](../DomoticsCore-HomeAssistant/examples/BasicHA/)** - Basic HA discovery
- **[HAWithWebUI](../DomoticsCore-HomeAssistant/examples/HAWithWebUI/)** - HA with web interface

### NTP
- **[BasicNTP](../DomoticsCore-NTP/examples/BasicNTP/)** - Basic time sync
- **[NTPWithWebUI](../DomoticsCore-NTP/examples/NTPWithWebUI/)** - NTP with web interface

### OTA
- **[BasicOTA](../DomoticsCore-OTA/examples/BasicOTA/)** - Basic OTA updates
- **[OTAWithWebUI](../DomoticsCore-OTA/examples/OTAWithWebUI/)** - OTA with web interface

### LED
- **[BasicLED](../DomoticsCore-LED/examples/BasicLED/)** - LED effects demo
- **[LEDWithWebUI](../DomoticsCore-LED/examples/LEDWithWebUI/)** - LED with web interface

### Storage
- **[BasicStorage](../DomoticsCore-Storage/examples/BasicStorage/)** - Key-value storage
- **[NamespaceDemo](../DomoticsCore-Storage/examples/NamespaceDemo/)** - Namespace isolation
- **[StorageWithWebUI](../DomoticsCore-Storage/examples/StorageWithWebUI/)** - Storage with web interface

### RemoteConsole
- **[BasicRemoteConsole](../DomoticsCore-RemoteConsole/examples/BasicRemoteConsole/)** - Telnet console
- **[RemoteConsoleWithWebUI](../DomoticsCore-RemoteConsole/examples/RemoteConsoleWithWebUI/)** - Console with web interface

### SystemInfo
- **[BasicSystemInfo](../DomoticsCore-SystemInfo/examples/BasicSystemInfo/)** - System metrics
- **[SystemInfoWithWebUI](../DomoticsCore-SystemInfo/examples/SystemInfoWithWebUI/)** - SystemInfo with web interface
