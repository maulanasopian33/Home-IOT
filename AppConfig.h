#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

class AppConfig {
public:
    char apiUrl[128];
    char apiHealthUrl[128];
    char tempOffset[10];

    AppConfig();
    void load();
    void save();
    float getTempOffsetFloat();
};

extern AppConfig appConfig;
