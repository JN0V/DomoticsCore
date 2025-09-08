#include <DomoticsCore/LEDManager.h>
#include <DomoticsCore/Logger.h>
#include <Arduino.h>

LEDManager::LEDManager(int pin)
  : ledPin(pin), currentStatus(WIFI_STARTING),
    lastUpdate(0), ledState(false), blinkCount(0),
    startingSequenceStart(0) {
  if (pin < 0 || pin > 39 || pin == 6 || pin == 7 || pin == 8 || pin == 9 || pin == 10 || pin == 11) {
    DLOG_W(LOG_LED, "Invalid LED pin %d, using default pin 2", pin);
    ledPin = 2;
  }
}

void LEDManager::begin() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
}

void LEDManager::setStatus(WiFiStatus status) {
  if (currentStatus != status) {
    currentStatus = status;
    lastUpdate = 0;
    blinkCount = 0;
    startingSequenceStart = 0;
  }
}

WiFiStatus LEDManager::getCurrentStatus() { return currentStatus; }

void LEDManager::runSequence(WiFiStatus status, unsigned long duration) {
  if (duration > 60000) {
    DLOG_W(LOG_LED, "LED sequence duration %lu too long, limiting to 60s", duration);
    duration = 60000;
  }
  setStatus(status);
  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    update();
    delay(50);
  }
}

void LEDManager::update() {
  unsigned long currentTime = millis();
  switch (currentStatus) {
    case WIFI_STARTING: {
      if (startingSequenceStart == 0) {
        startingSequenceStart = currentTime;
      }
      unsigned long elapsed = currentTime - startingSequenceStart;
      if (elapsed < 200) {
        digitalWrite(ledPin, HIGH);
      } else if (elapsed < 400) {
        digitalWrite(ledPin, LOW);
      } else if (elapsed < 600) {
        digitalWrite(ledPin, HIGH);
      } else if (elapsed < 800) {
        digitalWrite(ledPin, LOW);
      } else if (elapsed < 1000) {
        digitalWrite(ledPin, HIGH);
      } else if (elapsed < 1200) {
        digitalWrite(ledPin, LOW);
      } else {
        digitalWrite(ledPin, LOW);
        if (elapsed > 2000) {
          currentStatus = WIFI_AP_MODE;
        }
      }
      break;
    }
    case WIFI_AP_MODE:
      if (currentTime - lastUpdate >= 1000) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        lastUpdate = currentTime;
      }
      break;
    case WIFI_CONNECTING:
      if (currentTime - lastUpdate >= 500) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        lastUpdate = currentTime;
      }
      break;
    case WIFI_CONNECTED: {
      if (lastUpdate == 0) {
        lastUpdate = currentTime;
        blinkCount = 0;
      }
      unsigned long elapsed = currentTime - lastUpdate;
      if (blinkCount < 4) {
        if (elapsed >= 150) {
          ledState = !ledState;
          digitalWrite(ledPin, ledState);
          lastUpdate = currentTime;
          blinkCount++;
        }
      } else {
        digitalWrite(ledPin, LOW);
      }
      break;
    }
    case WIFI_RECONNECTING:
      if (currentTime - lastUpdate >= 200) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        lastUpdate = currentTime;
      }
      break;
    case WIFI_FAILED:
      if (currentTime - lastUpdate >= 100) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        lastUpdate = currentTime;
      }
      break;
    case WIFI_NORMAL_OPERATION:
      digitalWrite(ledPin, HIGH);
      break;
  }
}
