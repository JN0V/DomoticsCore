<!-- workline
sources: []
-->

# Roadmap

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
