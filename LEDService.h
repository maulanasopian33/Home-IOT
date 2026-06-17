#pragma once

#include <Arduino.h>

class LEDService {
private:
  int pin;
  unsigned long previousMillis;
  int ledState;

public:
  LEDService(int ledPin);
  void begin();
  void handle(bool isWifiConnected, int queueCount);
};
