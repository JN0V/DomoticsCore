<!-- workline
sources: [library.json, tools/check_versions.py, tools/bump_version.py]
-->

# Versioning and Roadmap

## Versioning

- **Root library**: The top-level `library.json` defines the `DomoticsCore` framework version (`X.Y.Z`).
- **Component libraries**: Each `DomoticsCore-*` sub-library has its own `library.json` `version` and a matching `metadata.version` in its C++ component class.
- **Propagation rules** (Model B):
  - Bumping a component **patch** -> bump the **root patch**.
  - Bumping a component **minor** -> bump the **root minor** (and reset root patch).
  - Bumping a component **major** -> bump the **root major** (and reset root minor/patch).
  - Only the changed component and the root are bumped; other components keep their current versions.

### Versioning tools

- **Consistency check** (used in CI):

  ```bash
  python tools/check_versions.py --verbose
  ```

  This script ensures that, for every `DomoticsCore-*` directory:

  - `library.json.version` matches all `metadata.version = "X.Y.Z"` assignments under `include/` and `src/`.
  - (Optionally) with `--check-tag`, the root `library.json.version` matches the Git tag `vX.Y.Z` when run on a tagged commit.

- **Version bump helper**:

  ```bash
  # Bump MQTT component and propagate the same level to the root DomoticsCore version
  python tools/bump_version.py MQTT minor

  # Equivalent explicit component name
  python tools/bump_version.py DomoticsCore-MQTT patch

  # Bump only the root DomoticsCore version
  python tools/bump_version.py root major

  # Preview changes without modifying files
  python tools/bump_version.py MQTT minor --dry-run --verbose
  ```

The bump script:

- Reads the current component `library.json.version`.
- Computes the new SemVer according to the requested level.
- Updates the component `library.json` and all `metadata.version` assignments inside that component.
- Bumps the root `library.json.version` by the same level whenever a component is bumped.

## 🗺️ Roadmap

### Completed
- ✅ PlatformIO Registry publication
- ✅ ESP32-C3 full support (USB CDC serial)
- ✅ MemoryManager for device-agnostic memory adaptation
- ✅ HeapTracker for memory leak testing (native + hardware)
- ✅ ESP8266 memory optimizations (4 spec phases completed)
- ✅ WebUI SSE dual-mode transport
- ✅ RemoteConsole WebUI integration
- ✅ 37+ isolated unit tests with mock infrastructure

### Current Priorities
- ESP8266 full framework validation
- Additional Home Assistant entity types
- Performance profiling and optimization

### Planned: Display Component (`DomoticsCore-Display`)

OLED/screen support for boards with integrated displays (e.g. ESP32-C3 SuperMini 0.42" OLED).

**Target hardware:** SSD1306-based I2C OLED displays (72x40, 128x64, 128x32)

**Scope:**
- New `DisplayComponent` following the existing HAL pattern (`Display_HAL.h` routing)
- U8g2 library for driver support (only lib with native 72x40 constructor)
- EventBus integration: subscribe to system events (WiFi status, MQTT, heap, uptime) for automatic display
- WebUI provider for display configuration
- Screen rotation via timer for small displays (4-5 text lines on 72x40)
- I2C bus sharing with external sensors (address-based coexistence)

**Constraints:**
- ESP32-C3 single-core (160MHz): display updates must be infrequent to avoid competing with WiFi/MQTT
- GPIO budget: I2C pins (SDA/SCL) are hardwired on integrated boards, reducing available GPIOs
- 72x40 framebuffer is small (~360 bytes), no RAM concern
