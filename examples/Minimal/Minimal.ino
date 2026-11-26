// DomoticsCore - Minimal: WiFi (AP fallback), LED and RemoteConsole.
// Leave the SSID empty to start in AP mode and configure via the web page.
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

#define LOG_APP "APP"

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);

    SystemConfig config = SystemConfig::minimal();
    config.deviceName = "MinimalDevice";
    config.firmwareVersion = "1.0.0";
    config.wifiSSID = "";
    config.wifiPassword = "";

    domotics = new System(config);
    if (!domotics->begin()) {
        DLOG_E(LOG_APP, "System initialization failed!");
    } else {
        DLOG_I(LOG_APP, "System initialized successfully!");
    }
}

void loop() {
    domotics->loop();
}
