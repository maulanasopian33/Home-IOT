#include "LEDService.h"

LEDService::LEDService(int ledPin) {
  pin = ledPin;
  previousMillis = 0;
  ledState = LOW;
}

void LEDService::begin() {
  pinMode(pin, OUTPUT);
}

void LEDService::handle(bool isWifiConnected, int queueCount) {
  unsigned long currentMillis = millis();
  
  // Status 0: Disconnected (Fast blink 200ms)
  // Status 1: Normal (Heartbeat: 100ms ON, 2000ms OFF)
  // Status 2: Error/Queue Pending (Blink 500ms)
  
  int state = 0;
  if (!isWifiConnected) {
    state = 0;
  } else if (queueCount > 0) {
    state = 2;
  } else {
    state = 1;
  }

  if (state == 0) {
    // Fast blink (WiFi Terputus / Mode AP)
    if (currentMillis - previousMillis >= 200) {
      previousMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(pin, ledState);
    }
  } else if (state == 2) {
    // Syncing / Antrean Tertahan
    if (currentMillis - previousMillis >= 500) {
      previousMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(pin, ledState);
    }
  } else if (state == 1) {
    // Normal / Heartbeat (Standby Hemat Daya)
    if (ledState == LOW && currentMillis - previousMillis >= 2000) {
      previousMillis = currentMillis;
      ledState = HIGH;
      digitalWrite(pin, HIGH);
    } else if (ledState == HIGH && currentMillis - previousMillis >= 100) {
      previousMillis = currentMillis;
      ledState = LOW;
      digitalWrite(pin, LOW);
    }
  }
}
