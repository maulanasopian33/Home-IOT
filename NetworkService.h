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

  void initOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    
    ArduinoOTA.onStart([]() { Serial.println("[OTA] Proses update dimulai..."); });
    ArduinoOTA.onEnd([]() { Serial.println("\n[OTA] Update Selesai!"); });
    ArduinoOTA.onError([](ota_error_t error) { Serial.printf("[OTA] Error[%u]\n", error); });

    ArduinoOTA.begin();
    otaReady = true;
    Serial.println("[OTA] Layanan siap menerima update.");
  }
        
public: // <--- Semua fungsi di bawah ini sekarang bisa diakses dari Main.ino

  // Fungsi pengirim tunggal (Sekarang sudah PUBLIC)
  void sendDataToAPI(const char* statusAplikasi, String idSensor = "", float suhu = -999.0, float hum = -999.0) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(API_URL);
      http.addHeader("Content-Type", "application/json");

      // Format dasar JSON (Kirim IP, Hostname, Status)
      String payload = "{";
      payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
      payload += "\"hostname\":\"" + String(OTA_HOSTNAME) + "\",";
      payload += "\"status\":\"" + String(statusAplikasi) + "\"";

      // Jika ada data sensor, sisipkan ke payload
      if (idSensor != "" && suhu != -999.0 && hum != -999.0) {
        payload += ",\"idsensor\":\"" + idSensor + "\",";
        payload += "\"suhu\":" + String(suhu, 1) + ",";
        payload += "\"kelembapan\":" + String(hum, 1);
      }

      payload += "}";

      int httpCode = http.POST(payload);
      if (httpCode > 0) {
        Serial.printf("[API] Sukses terkirim (%s). Kode: %d\n", statusAplikasi, httpCode);
      } else {
        Serial.printf("[API] Gagal mengirim data. Error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }

  void begin() {
    wm.setConfigPortalBlocking(false);
    
    Serial.println("[Network] Memulai layanan...");
    if(wm.autoConnect(AP_NAME)) {
      Serial.println("[Network] Mencoba koneksi tersimpan...");
    } else {
      Serial.println("[Network] Menunggu konfigurasi via Captive Portal...");
    }
  }

  void handle() {
    wm.process(); 

    if (WiFi.status() == WL_CONNECTED) {
      if (!isConnected) {
        isConnected = true;
        Serial.println("\n[Network] Terhubung ke WiFi!");
        Serial.print("[Network] IP Address: ");
        Serial.println(WiFi.localIP());

        initOTA();
        
        // KORREKSI: Mengubah sendStatusToAPI menjadi sendDataToAPI
        sendDataToAPI("ONLINE_STARTUP");
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
