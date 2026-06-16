#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "Config.h"

class NetworkService {
private:
  WiFiManager wm;
  bool isConnected = false;
  bool otaReady = false;

  void sendStatusToAPI(const char* statusAplikasi) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(API_URL);
      http.addHeader("Content-Type", "application/json");

      // Construct JSON payload
      String payload = "{\"ip\":\"" + WiFi.localIP().toString() + "\",";
      payload += "\"hostname\":\"" + String(OTA_HOSTNAME) + "\",";
      payload += "\"status\":\"" + String(statusAplikasi) + "\"}";

      int httpCode = http.POST(payload);
      if (httpCode > 0) {
        Serial.printf("[API] Sukses terkirim. Kode: %d\n", httpCode);
      } else {
        Serial.printf("[API] Gagal mengirim data. Error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }

  void initOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    ArduinoOTA.onStart([]() { Serial.println("[OTA] Proses update dimulai..."); });
    ArduinoOTA.onEnd([]() { Serial.println("\n[OTA] Update Selesai!"); });
    ArduinoOTA.onError([](ota_error_t error) { Serial.printf("[OTA] Error[%u]\n", error); });

    ArduinoOTA.begin();
    otaReady = true;
    Serial.println("[OTA] Layanan siap menerima update.");
  }

public:
  void begin() {
    // Mode Non-Blocking aktif
    wm.setConfigPortalBlocking(false);
    
    Serial.println("[Network] Memulai layanan...");
    if(wm.autoConnect(AP_NAME)) {
      Serial.println("[Network] Mencoba koneksi tersimpan...");
    } else {
      Serial.println("[Network] Menunggu konfigurasi via Captive Portal...");
    }
  }

  void handle() {
    // Menjaga WiFiManager tetap berjalan di background
    wm.process(); 

    // State machine sederhana untuk memantau status koneksi
    if (WiFi.status() == WL_CONNECTED) {
      if (!isConnected) {
        isConnected = true;
        Serial.println("\n[Network] Terhubung ke WiFi!");
        Serial.print("[Network] IP Address: ");
        Serial.println(WiFi.localIP());

        initOTA();
        sendStatusToAPI("ONLINE_STARTUP");
      }

      if (otaReady) {
        ArduinoOTA.handle();
      }
    } else {
      if (isConnected) {
        isConnected = false;
        Serial.println("[Network] Peringatan: Koneksi WiFi Terputus!");
      }
    }
  }
};
