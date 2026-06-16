#pragma once

#include <DHT.h>
#include "Config.h"

// Struktur data untuk menyimpan status terakhir setiap sensor
struct DHTStruct {
  DHT* dhtPointer;
  int pin;
  String idSensor;
  float lastTemp;
  float lastHum;
  bool isAvailable;
};

class DHTService {
private:
  DHTStruct sensors[3];
  unsigned long previousMillis = 0;

  // Callback function untuk mengirim data ke NetworkService
  void (*onDataChangeCallback)(String id, float temp, float hum) = nullptr;

public:
  void begin() {
    for (int i = 0; i < 3; i++) {
      sensors[i].pin = DHT_PINS[i];
      sensors[i].idSensor = "DHT11_PIN_" + String(DHT_PINS[i]);
      sensors[i].lastTemp = 0.0;
      sensors[i].lastHum = 0.0;
      sensors[i].isAvailable = false;

      // Inisialisasi objek DHT secara dinamis
      sensors[i].dhtPointer = new DHT(sensors[i].pin, DHT11);
      sensors[i].dhtPointer->begin();
    }
    Serial.println("[DHT] Sistem Multi-Sensor Dinamis Siap.");
  }

  // Mendaftarkan fungsi pengiriman API dari luar class
  void onDataChange(void (*callback)(String, float, float)) {
    onDataChangeCallback = callback;
  }

  void handle() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis < DHT_INTERVAL) return;
    previousMillis = currentMillis;

    for (int i = 0; i < 3; i++) {
      float h = sensors[i].dhtPointer->readHumidity();
      float t = sensors[i].dhtPointer->readTemperature();

      // Cek apakah sensor tercolok (jika error/NaN berarti dicabut)
      if (isnan(h) || isnan(t)) {
        if (sensors[i].isAvailable) {
          sensors[i].isAvailable = false;
          Serial.printf("[DHT] Sensor %s TERPUTUS!\n", sensors[i].idSensor.c_str());
        }
        continue; // Lewati sensor ini karena tidak tersedia
      }

      // Jika sensor baru dicolok kembali
      if (!sensors[i].isAvailable) {
        sensors[i].isAvailable = true;
        Serial.printf("[DHT] Sensor %s TERDETEKSI ONLINE!\n", sensors[i].idSensor.c_str());
        
        // Kirim data pertama kali saat dicolok
        triggerUpdate(i, t, h);
      } 
      // Jika sensor memang aktif, cek apakah ada perubahan nilai suhu/kelembapan yang signifikan
      else if (abs(sensors[i].lastTemp - t) >= 0.2 || abs(sensors[i].lastHum - h) >= 1.0) {
        Serial.printf("[DHT] Perubahan data terdeteksi pada %s\n", sensors[i].idSensor.c_str());
        triggerUpdate(i, t, h);
      }
    }
  }

private:
  void triggerUpdate(int index, float t, float h) {
    sensors[index].lastTemp = t;
    sensors[index].lastHum = h;

    // Panggil callback untuk kirim data ke API jika fungsi sudah didaftarkan
    if (onDataChangeCallback != nullptr) {
      onDataChangeCallback(sensors[index].idSensor, t, h);
    }
  }
};
