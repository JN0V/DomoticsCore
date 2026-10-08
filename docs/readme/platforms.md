<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/HAL/]
-->

# Platform Compatibility

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
