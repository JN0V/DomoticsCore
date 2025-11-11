#!/bin/bash

# Fast test of all examples

EXAMPLES=(
    "DomoticsCore-Core/examples/01-CoreOnly"
    "DomoticsCore-Core/examples/02-CoreWithDummyComponent"
    "DomoticsCore-HomeAssistant/examples/BasicHA"
    "DomoticsCore-HomeAssistant/examples/HAWithWebUI"
    "DomoticsCore-LED/examples/BasicLED"
    "DomoticsCore-LED/examples/LEDWithWebUI"
    "DomoticsCore-MQTT/examples/BasicMQTT"
    "DomoticsCore-MQTT/examples/MQTTWithWebUI"
    "DomoticsCore-MQTT/examples/MQTTWifiWithWebUI"
    "DomoticsCore-NTP/examples/BasicNTP"
    "DomoticsCore-NTP/examples/NTPWithWebUI"
    "DomoticsCore-OTA/examples/OTAWithWebUI"
    "DomoticsCore-RemoteConsole/examples/BasicRemoteConsole"
    "DomoticsCore-Storage/examples/BasicStorage"
    "DomoticsCore-SystemInfo/examples/BasicSystemInfo"
    "DomoticsCore-System/examples/FullStack"
    "DomoticsCore-Wifi/examples/BasicWifi"
    "DomoticsCore-WebUI/examples/BasicWebUI"
)

echo "Testing ${#EXAMPLES[@]} examples..."

for example in "${EXAMPLES[@]}"; do
    echo -n "$example ... "
    if pio run -d "$example" > /dev/null 2>&1; then
        echo "✓"
    else
        echo "✗ FAILED"
    fi
done

echo "Done!"
