#include <Arduino.h>
#include <DomoticsCore/DomoticsCore.h>
#include <ESPAsyncWebServer.h>

// Demonstrate runtime override via CoreConfig
CoreConfig cfg; // defaults come from firmware-config.h
DomoticsCore* gCore = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Example overrides (optional). Keep BasicApp minimal, no MQTT demo here.
  cfg.deviceName = "JNOV-EXAMPLE";        // custom AP/hostname
  cfg.firmwareVersion = "1.0.0";          // BasicApp firmware version
  cfg.strictNtpBeforeNormalOp = true;      // keep LED solid only after NTP
  // cfg.webServerPort = 8080;             // uncomment to change HTTP port
  // cfg.ledPin = 2;                        // override LED pin if needed

  gCore = new DomoticsCore(cfg); // construct after overrides

  // Register custom routes BEFORE starting the server
  gCore->webServer().on("/api/ping", HTTP_GET, [](AsyncWebServerRequest* request){
    String body;
    body.reserve(64);
    body = "pong: ";
    body += String(millis()/1000);
    body += "s";
    request->send(200, "text/plain", body);
  });

  // Now start the core (starts server, WiFi, NTP, etc.)
  gCore->begin();
}

void loop() {
  if (gCore) gCore->loop();
}
