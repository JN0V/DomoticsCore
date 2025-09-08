#include <Arduino.h>
void setup() {
  Serial.begin(115200);
  log_i("TEST", "Testing Arduino logging");
  log_e("TEST", "Testing error log");
}
void loop() {}
