#include <Arduino.h>
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "GitHub-Install-Test";
    config.wifiSSID = "";  // AP mode
    config.wifiPassword = "";
    config.ledPin = 2;
    
    domotics = new System(config);
    
    if (!domotics->begin()) {
        Serial.println("System init failed!");
        while(1) delay(1000);
    }
    
    Serial.println("System ready! Access point or WebUI available.");
}

void loop() {
    domotics->loop();
}
