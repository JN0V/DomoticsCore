# DomoticsCore-System -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document is the exhaustive API and behavior reference for the `DomoticsCore-System` component (v1.4.1). It covers every public class, struct, enum, method, configuration field, LED pattern, state transition, persistence key, event orchestration rule, and WebUI integration detail.

---

## Table of Contents

1. [System Class](#system-class)
2. [SystemConfig Struct](#systemconfig-struct)
3. [Configuration Presets](#configuration-presets)
4. [SystemState Enum and Transitions](#systemstate-enum-and-transitions)
5. [LED Status Patterns](#led-status-patterns)
6. [Component Orchestration](#component-orchestration)
7. [Optional Component Integration](#optional-component-integration)
8. [Custom Console Commands](#custom-console-commands)
9. [Configuration Persistence (SystemPersistence)](#configuration-persistence)
10. [WebUI Setup (SystemWebUISetup)](#webui-setup)
11. [Boot Diagnostics](#boot-diagnostics)
12. [Heap Guards](#heap-guards)
13. [Built-in Console Commands](#built-in-console-commands)

---

## System Class

**Header:** `<DomoticsCore/System.h>`
**Namespace:** `DomoticsCore`

The `System` class is the top-level orchestrator. It owns a `Core` instance, creates and registers all components based on `SystemConfig`, manages the system lifecycle, and provides accessor methods for runtime interaction.

### Constructor

```cpp
System(const SystemConfig& cfg = SystemConfig());
```

Accepts a `SystemConfig` struct. If none is provided, default values are used (equivalent to `SystemConfig::minimal()` with empty WiFi credentials, which triggers AP mode).

### Destructor

```cpp
~System();
```

Calls `webUIProviders.cleanup()` to free all heap-allocated WebUI provider objects.

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `begin` | `bool begin()` | Initializes the entire system. Registers components, initializes Core, loads persisted configs, sets up event orchestration, initializes boot diagnostics, and transitions to `READY`. Returns `false` on Core initialization failure. Calling `begin()` twice is safe (logs a warning and returns `true`). |
| `loop` | `void loop()` | Delegates to `core.loop()`. Call this in the Arduino `loop()` function. |
| `getCore` | `Core& getCore()` | Returns a reference to the internal `Core` instance. Use this to add custom components or access the event bus. |
| `getState` | `SystemState getState() const` | Returns the current `SystemState`. |
| `getConsole` | `RemoteConsoleComponent* getConsole()` | Returns a pointer to the RemoteConsole component, or `nullptr` if console is disabled. |
| `getWiFi` | `WifiComponent* getWiFi()` | Returns a pointer to the WiFi component. |
| `onStateChange` | `void onStateChange(std::function<void(SystemState, SystemState)> callback)` | Registers a callback invoked on every state transition. Parameters are `(oldState, newState)`. Multiple callbacks are supported. |
| `registerCommand` | `void registerCommand(const String& name, std::function<String(const String&)> handler)` | Registers a custom telnet command. Delegates to `RemoteConsoleComponent::registerCommand()`. No-op if console is disabled. |

### Initialization Sequence (`begin()`)

The `begin()` method follows this exact order:

1. Print startup banner with device name and firmware version.
2. Auto-detect chip model via `HAL::getChipModel()` if `config.model` is empty.
3. Set state to `BOOTING`.
4. Register components in order: LED (early init), Storage, WiFi, RemoteConsole, then optional components (WebUI, NTP, MQTT+HA, OTA, SystemInfo).
5. Call `core.begin()`. If this fails, state transitions to `ERROR` and `begin()` returns `false`.
6. Load all persisted configurations from Storage (heap-guarded, 3072 bytes minimum).
7. Register WebUI providers with persistence callbacks (heap-guarded).
8. Set up event orchestration between components (heap-guarded).
9. Initialize boot diagnostics persistence (heap-guarded).
10. Transition to `READY` and print the ready banner with IP address and service URLs.

---

## SystemConfig Struct

**Header:** `<DomoticsCore/SystemConfig.h>`
**Namespace:** `DomoticsCore`

All fields have sensible defaults. Fields are grouped by functional area.

### Device Identity

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `deviceName` | `String` | `"DomoticsCore"` | Human-readable name shown in banners, WebUI, and HA discovery |
| `manufacturer` | `String` | `"DomoticsCore"` | Manufacturer identifier for HA device registry |
| `model` | `String` | `""` (auto) | Chip model. Auto-detected via `HAL::getChipModel()` if left empty |
| `firmwareVersion` | `String` | `"1.0.0"` | Firmware version reported in status and HA discovery |

### WiFi

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `wifiAutoConfig` | `bool` | `true` | Enables automatic AP mode when no SSID is configured |
| `wifiSSID` | `String` | `""` | Station mode SSID. Empty triggers AP mode if `wifiAutoConfig` is true |
| `wifiPassword` | `String` | `""` | Station mode password |
| `wifiAPSSID` | `String` | `""` | AP SSID. Auto-generated as `{deviceName}-{chipIdHex}` if empty |
| `wifiAPPassword` | `String` | `""` | AP password. Empty creates an open access point |
| `wifiTimeout` | `uint32_t` | `30000` | WiFi connection timeout in milliseconds |

### LED

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableLED` | `bool` | `true` | Enable the status LED component |
| `ledPin` | `uint8_t` | `2` | GPIO pin for the status LED |
| `ledActiveHigh` | `bool` | `true` | Set to `false` for LEDs wired active-low (inverted) |

### RemoteConsole

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableConsole` | `bool` | `true` | Enable the telnet console |
| `consolePort` | `uint16_t` | `23` | TCP port for telnet connections |
| `consoleMaxClients` | `uint8_t` | `3` | Maximum simultaneous telnet clients |

### WebUI

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableWebUI` | `bool` | `false` | Enable the web interface |
| `webUIPort` | `uint16_t` | `80` | HTTP port for the web server |
| `webUIEnableAPI` | `bool` | `true` | Enable REST API endpoints |

### MQTT

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableMQTT` | `bool` | `false` | Enable the MQTT client |
| `mqttBroker` | `String` | `""` | MQTT broker hostname or IP |
| `mqttPort` | `uint16_t` | `1883` | MQTT broker port |
| `mqttUser` | `String` | `""` | MQTT username (optional) |
| `mqttPassword` | `String` | `""` | MQTT password (optional) |
| `mqttClientId` | `String` | `""` | MQTT client ID. Auto-generated if empty |

### Home Assistant

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableHomeAssistant` | `bool` | `false` | Enable HA auto-discovery (requires MQTT) |
| `haDiscoveryPrefix` | `String` | `"homeassistant"` | MQTT discovery prefix |

### NTP

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableNTP` | `bool` | `false` | Enable time synchronization |
| `ntpServer` | `String` | `"pool.ntp.org"` | NTP server address |
| `ntpTimezone` | `String` | `"UTC"` | Timezone string |

### OTA

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableOTA` | `bool` | `false` | Enable over-the-air firmware updates |
| `otaPassword` | `String` | `""` | Mapped to `OTAConfig::bearerToken` during component registration (see `System::registerOTAComponent()`). When set, OTA upload requests must provide this value as a bearer token for authentication. |

### SystemInfo

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableSystemInfo` | `bool` | `false` | Enable the SystemInfo diagnostic component |

### Storage

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enableStorage` | `bool` | `false` | Enable NVS-backed persistent storage |
| `storageNamespace` | `String` | `"domotics"` | NVS namespace for key-value storage |

### Logging

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `defaultLogLevel` | `LogLevel` | `LOG_LEVEL_INFO` | Default log verbosity for the console sink |

---

## Configuration Presets

`SystemConfig` provides three static factory methods that return pre-configured instances.

| Preset | Factory | Components Enabled |
|--------|---------|-------------------|
| **Minimal** | `SystemConfig::minimal()` | LED, Console, WiFi (auto-config) |
| **Standard** | `SystemConfig::standard()` | Minimal + WebUI, NTP, Storage |
| **Full Stack** | `SystemConfig::fullStack()` | Standard + MQTT, HomeAssistant, OTA, SystemInfo |

Each preset builds incrementally: `standard()` calls `minimal()` first, and `fullStack()` calls `standard()` first. You can further customize any field after calling the factory.

---

## SystemState Enum and Transitions

**Header:** `<DomoticsCore/SystemConfig.h>`
**Namespace:** `DomoticsCore`

```cpp
enum class SystemState {
    BOOTING,           // Initial boot
    WIFI_CONNECTING,   // Connecting to WiFi
    WIFI_CONNECTED,    // WiFi established
    SERVICES_STARTING, // Starting services
    READY,             // All services operational
    ERROR,             // Critical error
    OTA_UPDATE,        // Firmware update in progress
    SHUTDOWN           // Graceful shutdown
};
```

### State Transitions

The System class manages transitions via the private `setState()` method. Each transition:

1. Logs the transition: `"State: OLD -> NEW"`.
2. Updates the LED pattern via `updateLEDPattern()`.
3. Invokes all registered `onStateChange` callbacks with `(oldState, newState)`.

Duplicate transitions (setting a state that is already active) are silently ignored.

### String Conversion

```cpp
const char* systemStateToString(SystemState state);
```

Returns a human-readable C-string (e.g., `"BOOTING"`, `"READY"`, `"ERROR"`). Returns `"UNKNOWN"` for values outside the enum range.

### Normal Boot Sequence

```
BOOTING -> READY
```

The intermediate states (`WIFI_CONNECTING`, `WIFI_CONNECTED`, `SERVICES_STARTING`) are available for external code to use via the event bus or by calling `setState()` through custom orchestration, but the default `begin()` transitions directly from `BOOTING` to `READY` (or `ERROR` on failure).

---

## LED Status Patterns

When a state transition occurs, `updateLEDPattern()` maps the new state to an LED effect on the `"status"` LED. The LED component must be enabled (`config.enableLED = true`) for this to take effect.

| SystemState | LED Effect | Speed (ms) | Visual Description |
|-------------|-----------|------------|---------------------|
| `BOOTING` | `Blink` | 200 | Fast blink -- system is starting |
| `WIFI_CONNECTING` | `Blink` | 1000 | Slow blink -- waiting for network |
| `WIFI_CONNECTED` | `Pulse` | 2000 | Heartbeat-style double pulse |
| `SERVICES_STARTING` | `Fade` | 1500 | Smooth sine-wave fade in/out |
| `READY` | `Breathing` | 3000 | Slow cosine breathing -- normal operation |
| `ERROR` | `Blink` | 300 | Alert blink -- something failed |
| `OTA_UPDATE` | Solid White | -- | Constant on (solid white, full brightness) |
| `SHUTDOWN` | Off | 0 | LED turns off completely |

The `"status"` LED is registered during initialization as a single LED on `config.ledPin` with the inversion flag set to `!config.ledActiveHigh`.

---

## Component Orchestration

The `setupEventOrchestration()` method wires inter-component events using the Core `EventBus`. All orchestration is conditional on whether the required components and libraries are compiled in.

### WiFi -> MQTT

When the `WifiEvents::EVENT_STA_CONNECTED` event fires with a `true` payload, the System calls `mqttComp->connect()` to trigger MQTT connection. If WiFi is already connected at orchestration setup time, the MQTT connection is triggered immediately.

### NTP Event Logging

When `NTPEvents::EVENT_SYNCED` fires, the System logs `"NTP time synchronized"`.

### MQTT -> Home Assistant

When `HAEvents::EVENT_DISCOVERY_PUBLISHED` fires, the System logs the number of published HA entities.

---

## Optional Component Integration

All optional components use `__has_include()` compile-time detection. If the corresponding library is installed and the `enable*` flag in `SystemConfig` is `true`, the component is created and registered with Core. If the flag is `true` but the library is not installed, a warning is logged.

| Component | Library Header | Config Flag | Core Registration Name |
|-----------|---------------|-------------|----------------------|
| Storage | `<DomoticsCore/Storage.h>` | `enableStorage` | `"Storage"` |
| WebUI | `<DomoticsCore/WebUI.h>` | `enableWebUI` | `"WebUI"` |
| NTP | `<DomoticsCore/NTP.h>` | `enableNTP` | `"NTP"` |
| MQTT | `<DomoticsCore/MQTT.h>` | `enableMQTT` | `"MQTT"` |
| HomeAssistant | `<DomoticsCore/HomeAssistant.h>` | `enableHomeAssistant` | `"HomeAssistant"` |
| OTA | `<DomoticsCore/OTA.h>` | `enableOTA` | `"OTA"` |
| SystemInfo | `<DomoticsCore/SystemInfo.h>` | `enableSystemInfo` | `"System Info"` |

### Required Components (always registered)

| Component | Core Registration Name | Notes |
|-----------|----------------------|-------|
| LED | `"LED"` | Only if `enableLED` is true; early-initialized before Core |
| Wifi | `"Wifi"` | Always registered; AP mode if SSID is empty |
| RemoteConsole | `"RemoteConsole"` | Only if `enableConsole` is true |

### Home Assistant Node ID Generation

The HA `nodeId` is derived from `config.deviceName`: truncated to 32 characters, lowercased, and spaces replaced with underscores.

---

## Custom Console Commands

```cpp
void registerCommand(const String& name, std::function<String(const String&)> handler);
```

Delegates directly to `RemoteConsoleComponent::registerCommand()`. The handler receives the argument string (everything after the command name) and returns a response string. If the console is disabled (`console == nullptr`), the call is silently ignored.

**Example:**

```cpp
system.registerCommand("relay", [](const String& args) {
    if (args == "on") { digitalWrite(5, HIGH); return String("ON\n"); }
    if (args == "off") { digitalWrite(5, LOW); return String("OFF\n"); }
    return String("Usage: relay on|off\n");
});
```

---

## Configuration Persistence

**Header:** `<DomoticsCore/SystemPersistence.h>`
**Namespace:** `DomoticsCore::SystemHelpers`

The persistence module loads and saves component configurations from/to the Storage component (ESP32 NVS). It is invoked during `System::begin()` after Core initialization.

### Entry Point

```cpp
void loadAllConfigs(Core& core, SystemConfig& config, Components::WifiComponent* wifi);
```

This function:
1. Calls `registerStorageKeys()` to register all known keys with Storage for enumeration.
2. Calls individual loaders: `loadDeviceName`, `loadWifiConfig`, `loadWebUIConfig`, `loadNTPConfig`, `loadMQTTConfig`, `loadHomeAssistantConfig`.

### Registered Storage Keys

Keys are organized by component group:

| Group | Key | Type | Description |
|-------|-----|------|-------------|
| **System** | `device_name` | `s` (string) | Device name |
| **WiFi** | `wifi_ssid` | `s` | WiFi SSID |
| | `wifi_pass` | `s` | WiFi password |
| | `wifi_autocon` | `b` (bool) | Auto-connect enabled |
| | `wifi_ap_en` | `b` | AP mode enabled |
| | `wifi_ap_ssid` | `s` | AP SSID |
| | `wifi_ap_pass` | `s` | AP password |
| **WebUI** | `webui_theme` | `s` | UI theme |
| | `webui_color` | `s` | Primary accent color |
| | `webui_auth` | `b` | Authentication enabled |
| | `webui_user` | `s` | Username |
| | `webui_pass` | `s` | Password |
| **NTP** | `ntp_enabled` | `b` | NTP enabled |
| | `ntp_timezone` | `s` | Timezone string |
| | `ntp_interval` | `i` (int) | Sync interval |
| | `ntp_servers` | `s` | Comma-separated server list |
| **MQTT** | `mqtt_enabled` | `b` | MQTT enabled |
| | `mqtt_broker` | `s` | Broker address |
| | `mqtt_port` | `i` | Broker port |
| | `mqtt_user` | `s` | Username |
| | `mqtt_pass` | `s` | Password |
| | `mqtt_clientid` | `s` | Client ID |
| **HomeAssistant** | `ha_nodeid` | `s` | Node ID |
| | `ha_device_name` | `s` | Device name |
| | `ha_disc_prefix` | `s` | Discovery prefix |
| | `ha_mfg` | `s` | Manufacturer |
| | `ha_model` | `s` | Model |
| | `ha_sw_ver` | `s` | Software version |
| **Boot Diag** | `boot_count` | `i` | Persisted boot counter |
| | `last_reset` | `i` | Last reset reason code |
| | `last_heap` | `i` | Heap at last boot |
| | `last_minheap` | `i` | Minimum heap at last boot |

### WiFi Config Loading Note

When WiFi configuration is loaded from Storage during `begin()`, the WiFi mode update is **deferred** (not called immediately). This avoids heap exhaustion during the initialization phase when the `configSaveCallback` is not yet set.

---

## WebUI Setup

**Header:** `<DomoticsCore/SystemWebUISetup.h>`
**Namespace:** `DomoticsCore::SystemHelpers`

### WebUIProviders Struct

Holds raw pointers to all dynamically allocated WebUI provider instances for cleanup in the `System` destructor.

```cpp
struct WebUIProviders {
    // Conditional members based on __has_include
    WifiWebUI* wifi;
    NTPWebUI* ntp;
    MQTTWebUI* mqtt;
    OTAWebUI* ota;
    SystemInfoWebUI* sysInfo;
    RemoteConsoleWebUI* console;
    HomeAssistantWebUI* ha;

    void cleanup();  // Deletes all non-null pointers
};
```

### Provider Registration

```cpp
void setupWebUIProviders(
    Core& core,
    SystemConfig& config,
    WebUIProviders& providers,
    Components::WifiComponent* wifi,
    Components::RemoteConsoleComponent* console
);
```

For each optional WebUI provider library detected via `__has_include`:

1. Checks available heap against `MIN_HEAP_PER_PROVIDER` (6500 bytes). If heap is too low, skips remaining providers and returns.
2. Creates the provider via `new`.
3. If Storage is available, sets a `configSaveCallback` lambda that persists the component's config to the corresponding NVS keys.
4. Registers the provider with the WebUI component via `registerProviderWithComponent()`.

### Supported WebUI Providers

| Provider | Compile Guard | Component |
|----------|--------------|-----------|
| `WifiWebUI` | `WEBUI_SETUP_HAS_WIFI_WEBUI` | WiFi |
| `NTPWebUI` | `WEBUI_SETUP_HAS_NTP_WEBUI` | NTP |
| `MQTTWebUI` | `WEBUI_SETUP_HAS_MQTT_WEBUI` | MQTT |
| `OTAWebUI` | `WEBUI_SETUP_HAS_OTA_WEBUI` | OTA |
| `SystemInfoWebUI` | `WEBUI_SETUP_HAS_SYSINFO_WEBUI` | SystemInfo |
| `RemoteConsoleWebUI` | `WEBUI_SETUP_HAS_CONSOLE_WEBUI` | RemoteConsole |
| `HomeAssistantWebUI` | `WEBUI_SETUP_HAS_HA_WEBUI` | HomeAssistant |

### NTP Timezone API Endpoint

When the NTP WebUI provider is registered, the System also registers a custom API route on the WebUI component:

| Route | Method | Description |
|-------|--------|-------------|
| `/api/ntp/timezones` | `GET` | Returns a JSON array of all supported timezone options. Each entry has `"value"` (POSIX timezone string) and `"label"` (human-friendly name). The response is streamed from flash via `TIMEZONE_LOOKUP` to avoid heap allocation. |

### Provider `.init()` Calls

The **OTA** and **RemoteConsole** WebUI providers require an additional `.init(webuiComponent)` call after `registerProviderWithComponent()`. This call registers provider-specific API routes on the WebUI web server (e.g., `/api/console/loglevels` for RemoteConsole, OTA upload route for OTA). Other providers do not require this extra step.

### Home Assistant Save Callback Asymmetry

The HA WebUI save callback only persists **3 of the 6** HAConfig fields to Storage: `nodeId`, `deviceName`, and `discoveryPrefix`. The remaining fields (`manufacturer`, `model`, `swVersion`) are **not saved** by the WebUI callback. However, all 6 fields are **loaded** from Storage on boot (see `loadHomeAssistantConfig()` in SystemPersistence.h). This means that `manufacturer`, `model`, and `swVersion` can only be set via direct Storage writes or through initial `SystemConfig` values -- they cannot be changed from the WebUI.

### WebUI Self-Persistence

The WebUI component itself receives a `configCallback` that persists theme, device name, primary color, auth settings, username, and password to Storage. The password is only saved when non-empty.

### WiFi Config Save Callback

The WiFi config save lambda is always set on the `WifiComponent` (via `setConfigSaveCallback`) whenever Storage is available, regardless of whether the WifiWebUI provider is registered. This is critical for the STA fallback auto-save mechanism that prevents boot loops.

---

## Boot Diagnostics

When both Storage and SystemInfo components are enabled, `initBootDiagnosticsPersistence()`:

1. Loads `boot_count` from Storage and increments it.
2. Saves the updated `boot_count` back to Storage.
3. Updates the SystemInfo component with the new boot count via `setBootCount()`.
4. Persists `last_reset`, `last_heap`, and `last_minheap` from the boot diagnostics snapshot.

This data is accessible via the `bootdiag` console command.

---

## Heap Guards

All post-Core-initialization steps in `begin()` are guarded by a minimum heap threshold:

```cpp
static constexpr uint32_t MIN_HEAP_POST_INIT = 3072;
```

If `HAL::getFreeHeap()` falls below this value, the step is skipped with a warning log. This applies to:

- Configuration loading from Storage
- WebUI provider registration
- Event orchestration setup
- Boot diagnostics persistence

WebUI provider registration uses a separate, stricter threshold:

```cpp
static constexpr uint32_t MIN_HEAP_PER_PROVIDER = 6500;
```

This ensures sufficient heap remains for AsyncWebServer to handle concurrent HTTP connections after each provider is registered.

---

## Built-in Console Commands

These commands are automatically registered when the RemoteConsole is enabled:

| Command | Description | Output |
|---------|-------------|--------|
| `status` | System status summary | Device name, version, uptime, free heap, current state |
| `wifi` | WiFi detailed status | Delegates to `WifiComponent::getDetailedStatus()` |
| `storage` | Storage contents dump | Delegates to `StorageComponent::dumpContents()` |
| `bootdiag` | Boot diagnostics | Boot count, reset reason, boot heap, min heap, and persisted history |

These are in addition to commands provided by the RemoteConsole component itself (e.g., `help`, `level`, `info`, `heap`, `reboot`).
