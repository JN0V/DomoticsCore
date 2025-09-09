#include <DomoticsCore/DomoticsCore.h>

#define SENSOR_PIN A0

DomoticsCore* core = nullptr;
float sensorValue = 0.0;

void setup() {
  // Configure device
  CoreConfig config;
  config.deviceName = "BasicExample";
  config.firmwareVersion = "1.0.0";
  
  core = new DomoticsCore(config);
  
  // Add REST API endpoint
  core->webServer().on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"sensor_value\":" + String(sensorValue) + "}";
    request->send(200, "application/json", json);
  });
  
  core->begin();
}

void loop() {
  core->loop();
  
  // Read sensor every 5 seconds
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 5000) {
    sensorValue = (analogRead(SENSOR_PIN) / 4095.0) * 100.0;
    lastRead = millis();
  }
}
