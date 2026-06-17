#include <AsyncUDP.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h> 
#include <DHT.h>           // <-- TAMBAHKAN LIBRARY SENSOR DHT
#include <LittleFS.h>      // <-- TAMBAHKAN LIBRARY PENYIMPANAN OFFLINE

#include "Config.h"
#include "NetworkService.h"
#include "DHTService.h"    // <-- TAMBAHKAN FILE MODUL SENSOR

// Inisialisasi layanan jaringan dan modul sensor
NetworkService network;
DHTService dhtModule;      // <-- INITIALISASI MODUL DHT

// Variabel Aplikasi
unsigned long previousMillis = 0;
unsigned long previousHeartbeatMillis = 0;
int ledState = LOW;

// --- JEMBATAN EVENT (CALLBACK) ---
// Fungsi ini otomatis berjalan saat modul DHT mendeteksi sensor dicolok atau suhu berubah
void onSensorChange(String id, float temp, float hum) {
  // Mengirim riwayat sensor ke api.php dengan status "DHT_UPDATE"
  network.sendDataToAPI("DHT_UPDATE", id, temp, hum);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Inisialisasi File System (LittleFS)
  if (!LittleFS.begin(true)) {
    Serial.println("[Storage] Error: Gagal me-mount LittleFS!");
  } else {
    Serial.println("[Storage] LittleFS berhasil di-mount.");
  }

  // 1. Jalankan infrastruktur jaringan di background
  network.begin();

  // 2. Jalankan modul multi-sensor DHT dinamis
  dhtModule.begin();

  // 3. Daftarkan fungsi jembatan ke dalam modul DHT
  dhtModule.onDataChange(onSensorChange);
  
  Serial.println("Sistem Aplikasi Utama Dimulai!");
}

void loop() {
  // 1. WAJIB: Biarkan layanan jaringan beroperasi
  network.handle();

  // 2. WAJIB: Biarkan modul DHT memantau pin secara non-blocking
  dhtModule.handle();

  // 3. Logika Aplikasi Anda (Hardware / Sensor)
  jalankanBlink();
  jalankanHeartbeat();
}

// --- FUNGSI APLIKASI ---
void jalankanBlink() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= BLINK_INTERVAL) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}

void jalankanHeartbeat() {
  unsigned long currentMillis = millis();
  // Tunggu 10 detik pertama agar WiFi dan NTP siap, setelah itu ikuti interval heartbeat
  if (currentMillis - previousHeartbeatMillis >= HEARTBEAT_INTERVAL || (previousHeartbeatMillis == 0 && currentMillis > 10000)) {
    previousHeartbeatMillis = currentMillis;
    
    // Dapatkan status dari semua sensor DHT
    String sensorStatus = dhtModule.getSensorsStatusJSON();
    
    // Minta network module merakit dan mengirimkan laporan health
    network.sendHealthCheck(sensorStatus);
  }
}
