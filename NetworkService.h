#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <time.h>
#include "Config.h"

class NetworkService {
private:
  WiFiManager wm;
  bool isConnected = false;
  bool otaReady = false;

  // Variabel untuk antrean offline
  const char* offlineQueueFile = "/offline.jsonl";
  unsigned long lastSyncMillis = 0;
  const long SYNC_INTERVAL = 2000; // Kirim satu data offline per 2 detik


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

  // Fungsi untuk menyimpan payload ke LittleFS
  void saveOfflineData(String payload) {
    File file = LittleFS.open(offlineQueueFile, FILE_APPEND);
    if (!file) {
      Serial.println("[Storage] Gagal membuka file antrean offline untuk ditulis!");
      return;
    }

    // Cek ukuran agar tidak penuh (Misal batas 50KB / ~500 baris)
    if (file.size() > 50000) {
      file.close();
      LittleFS.remove(offlineQueueFile); // Reset antrean jika terlalu besar
      file = LittleFS.open(offlineQueueFile, FILE_WRITE);
    }
    
    file.println(payload);
    file.close();
    Serial.println("[Offline] Data disimpan ke antrean lokal.");
  }

    // Fungsi helper untuk mendapatkan waktu saat ini
    String getTimeString() {
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        return ""; // Gagal mendapat waktu
      }
      char timeStr[25];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
      return String(timeStr);
    }

  int getOfflineQueueCount() {
    if (!LittleFS.exists(offlineQueueFile)) return 0;
    File file = LittleFS.open(offlineQueueFile, FILE_READ);
    if (!file) return 0;
    int count = 0;
    while (file.available()) {
      if (file.read() == '\n') count++;
    }
    file.close();
    return count;
  }

  void sendHealthCheck(String sensorStatusJSON) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(API_HEALTH_URL);
      http.addHeader("Content-Type", "application/json");

      String payload = "{";
      payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
      payload += "\"hostname\":\"" + String(OTA_HOSTNAME) + "\",";
      payload += "\"status\":\"HEALTH_CHECK\",";
      
      String waktu = getTimeString();
      if (waktu != "") {
        payload += "\"tanggal\":\"" + waktu + "\",";
      }

      payload += "\"uptime_sec\":" + String(millis() / 1000) + ",";
      payload += "\"free_heap_kb\":" + String(ESP.getFreeHeap() / 1024) + ",";
      payload += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
      payload += "\"pending_queue_count\":" + String(getOfflineQueueCount()) + ",";
      payload += "\"sensors\":" + sensorStatusJSON;
      payload += "}";

      int httpCode = http.POST(payload);
      if (httpCode > 0) {
        Serial.printf("[Health] Berhasil mengirim heartbeat. Kode: %d\n", httpCode);
      } else {
        Serial.printf("[Health] Gagal mengirim heartbeat. Error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }

  // Fungsi pengirim tunggal (Sekarang sudah PUBLIC)
  void sendDataToAPI(const char* statusAplikasi, String idSensor = "", float suhu = -999.0, float hum = -999.0) {
    // Format dasar JSON (Kirim IP, Hostname, Status)
    String payload = "{";
    payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    payload += "\"hostname\":\"" + String(OTA_HOSTNAME) + "\",";
    payload += "\"status\":\"" + String(statusAplikasi) + "\"";

    // Sisipkan timestamp jika waktu tersedia
    String waktu = getTimeString();
    if (waktu != "") {
      payload += ",\"tanggal\":\"" + waktu + "\"";
    }

    // Jika ada data sensor, sisipkan ke payload
    if (idSensor != "" && suhu != -999.0 && hum != -999.0) {
      payload += ",\"idsensor\":\"" + idSensor + "\",";
      payload += "\"suhu\":" + String(suhu, 1) + ",";
      payload += "\"kelembapan\":" + String(hum, 1);
    }

    payload += "}";

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(API_URL);
      http.addHeader("Content-Type", "application/json");

      int httpCode = http.POST(payload);
      if (httpCode > 0) {
        Serial.printf("[API] Sukses terkirim (%s). Kode: %d\n", statusAplikasi, httpCode);
      } else {
        Serial.printf("[API] Gagal mengirim data. Error: %s\n", http.errorToString(httpCode).c_str());
        saveOfflineData(payload); // Simpan ke offline queue jika server down walau WiFi connect
      }
      http.end();
    } else {
      // Jika tidak ada WiFi, masuk ke antrean
      saveOfflineData(payload);
    }
  }

  void syncOfflineData() {
    if (!LittleFS.exists(offlineQueueFile)) return;

    File inFile = LittleFS.open(offlineQueueFile, FILE_READ);
    if (!inFile || inFile.size() == 0) {
      if (inFile) inFile.close();
      LittleFS.remove(offlineQueueFile);
      return;
    }

    String firstLine = inFile.readStringUntil('\n');
    firstLine.trim();

    if (firstLine.length() == 0) {
      inFile.close();
      return;
    }

    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(firstLine);
    bool success = (httpCode > 0);
    http.end();

    if (success) {
      Serial.printf("[Sync] Berhasil sync 1 data offline. Kode: %d\n", httpCode);
      
      // Tulis sisa file ke temporary file
      File outFile = LittleFS.open("/temp.jsonl", FILE_WRITE);
      if (outFile) {
        while (inFile.available()) {
          String line = inFile.readStringUntil('\n');
          line.trim();
          if(line.length() > 0) outFile.println(line);
        }
        outFile.close();
      }
      inFile.close();

      // Replace old queue dengan queue baru yang sudah dikurangi 1 baris
      LittleFS.remove(offlineQueueFile);
      LittleFS.rename("/temp.jsonl", offlineQueueFile);
    } else {
      Serial.printf("[Sync] Gagal sync data offline. Error: %d\n", httpCode);
      inFile.close();
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

        // Sinkronisasi Waktu
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        Serial.println("[Network] Meminta waktu dari NTP...");

        initOTA();
        
        // KORREKSI: Mengubah sendStatusToAPI menjadi sendDataToAPI
        sendDataToAPI("ONLINE_STARTUP");
      }

      if (otaReady) {
        ArduinoOTA.handle();
      }

      // Lakukan sinkronisasi data bertahap tiap SYNC_INTERVAL
      unsigned long currentMillis = millis();
      if (currentMillis - lastSyncMillis >= SYNC_INTERVAL) {
        lastSyncMillis = currentMillis;
        syncOfflineData();
      }

    } else {
      if (isConnected) {
        isConnected = false;
        Serial.println("[Network] Peringatan: Koneksi WiFi Terputus!");
      }
    }
  }
};
