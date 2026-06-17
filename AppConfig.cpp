#include "AppConfig.h"

AppConfig appConfig;

AppConfig::AppConfig() {
    // Default values
    strcpy(apiUrl, "https://your-domain.com/api.php");
    strcpy(apiHealthUrl, "https://your-domain.com/api_health.php");
    strcpy(tempOffset, "-5.0");
}

void AppConfig::load() {
    if (LittleFS.exists("/config.json")) {
        File file = LittleFS.open("/config.json", FILE_READ);
        if (file) {
            // Menggunakan ukuran memori yang aman
            #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
            #else
            DynamicJsonDocument doc(1024);
            #endif

            DeserializationError error = deserializeJson(doc, file);
            if (!error) {
                if (doc.containsKey("apiUrl")) strncpy(apiUrl, doc["apiUrl"], sizeof(apiUrl));
                if (doc.containsKey("apiHealthUrl")) strncpy(apiHealthUrl, doc["apiHealthUrl"], sizeof(apiHealthUrl));
                if (doc.containsKey("tempOffset")) strncpy(tempOffset, doc["tempOffset"], sizeof(tempOffset));
                Serial.println("[Config] Parameter dimuat dari LittleFS.");
            } else {
                Serial.println("[Config] Gagal parsing config.json.");
            }
            file.close();
        }
    } else {
        Serial.println("[Config] File config.json tidak ditemukan, menggunakan default.");
    }
}

void AppConfig::save() {
    File file = LittleFS.open("/config.json", FILE_WRITE);
    if (file) {
        #if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
        #else
        DynamicJsonDocument doc(1024);
        #endif

        doc["apiUrl"] = apiUrl;
        doc["apiHealthUrl"] = apiHealthUrl;
        doc["tempOffset"] = tempOffset;
        
        serializeJson(doc, file);
        file.close();
        Serial.println("[Config] Parameter disimpan ke LittleFS.");
    }
}

float AppConfig::getTempOffsetFloat() {
    return String(tempOffset).toFloat();
}
