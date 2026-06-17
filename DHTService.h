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
  int failedReads;
  
  // Array untuk menyimpan sampel pembacaan (Moving Average)
  float tempHistory[5];
  float humHistory[5];
  int sampleIndex;
};

class DHTService {
private:
  DHTStruct sensors[3];
  unsigned long previousMillis = 0;

  // Callback function untuk mengirim data ke NetworkService
  void (*onDataChangeCallback)(String id, float temp, float hum) = nullptr;

  // Fungsi pembantu untuk menghitung rata-rata array
  float getAverage(float* array, int size) {
    float sum = 0;
    for (int i = 0; i < size; i++) {
      sum += array[i];
    }
    return sum / size;
  }

public:
  void begin() {
    for (int i = 0; i < 3; i++) {
      sensors[i].pin = DHT_PINS[i];
      sensors[i].idSensor = "DHT11_PIN_" + String(DHT_PINS[i]);
      sensors[i].lastTemp = 0.0;
      sensors[i].lastHum = 0.0;
      sensors[i].isAvailable = false;
      sensors[i].sampleIndex = 0;
      sensors[i].failedReads = 0;

      // Inisialisasi awal array history dengan 0
      for(int j = 0; j < 5; j++) {
        sensors[i].tempHistory[j] = 0.0;
        sensors[i].humHistory[j] = 0.0;
      }

      // Inisialisasi objek DHT secara dinamis
      sensors[i].dhtPointer = new DHT(sensors[i].pin, DHT11);
      sensors[i].dhtPointer->begin();
    }
    Serial.println("[DHT] Sistem Multi-Sensor Siap dengan Filter Moving Average.");
  }

  void onDataChange(void (*callback)(String, float, float)) {
    onDataChangeCallback = callback;
  }

  String getSensorsStatusJSON() {
    String json = "[";
    for (int i = 0; i < 3; i++) {
      json += "{";
      json += "\"id\":\"" + sensors[i].idSensor + "\",";
      json += "\"is_available\":" + String(sensors[i].isAvailable ? "true" : "false") + ",";
      json += "\"failed_reads\":" + String(sensors[i].failedReads);
      json += "}";
      if (i < 2) json += ",";
    }
    json += "]";
    return json;
  }

  void handle() {
    unsigned long currentMillis = millis();
    
    // PENTING: Pastikan DHT_INTERVAL di Config.h minimal bernilai 2000 (2 detik)
    if (currentMillis - previousMillis < DHT_INTERVAL) return;
    previousMillis = currentMillis;

    for (int i = 0; i < 3; i++) {
      float rawH = sensors[i].dhtPointer->readHumidity();
      float rawT = sensors[i].dhtPointer->readTemperature();

      // Tambahkan offset kalibrasi jika pembacaan berhasil
      if (!isnan(rawT)) {
        rawT += TEMP_OFFSET;
      }

      // PERBAIKAN 1: Filter anti-glitch. Buang jika NaN atau bernilai 0 (seperti di log Anda)
      if (isnan(rawH) || isnan(rawT) || rawH <= 1.0 || rawT <= 1.0) {
        sensors[i].failedReads++;
        if (sensors[i].failedReads >= 3) { // Toleransi gagal baca 3 kali beruntun
          if (sensors[i].isAvailable) {
            sensors[i].isAvailable = false;
            Serial.printf("[DHT] Sensor %s TERPUTUS atau DATA INVALID!\n", sensors[i].idSensor.c_str());
          }
        }
        continue; 
      }
      sensors[i].failedReads = 0; // Reset jika berhasil

      // PERBAIKAN 2: Masukkan data ke filter Rata-rata Bergerak (Moving Average)
      if (sensors[i].tempHistory[0] == 0.0 && sensors[i].tempHistory[1] == 0.0) {
        // Inisialisasi awal agar rata-rata tidak mulai dari 0
        for (int j = 0; j < 5; j++) {
          sensors[i].tempHistory[j] = rawT;
          sensors[i].humHistory[j] = rawH;
        }
      } else {
        int idx = sensors[i].sampleIndex;
        sensors[i].tempHistory[idx] = rawT;
        sensors[i].humHistory[idx] = rawH;
        sensors[i].sampleIndex = (idx + 1) % 5; // Berputar dari indeks 0 - 4
      }

      // Hitung hasil rata-rata dari 5 data terakhir
      float filteredT = getAverage(sensors[i].tempHistory, 5);
      float filteredH = getAverage(sensors[i].humHistory, 5);

      // Jika sensor baru online
      if (!sensors[i].isAvailable) {
        sensors[i].isAvailable = true;
        Serial.printf("[DHT] Sensor %s ONLINE!\n", sensors[i].idSensor.c_str());
        triggerUpdate(i, filteredT, filteredH);
      } 
      // PERBAIKAN 3: Threshold disesuaikan. Suhu berubah >= 1.0 atau Kelembapan >= 2.0 baru kirim API
      else if (abs(sensors[i].lastTemp - filteredT) >= 1.0 || abs(sensors[i].lastHum - filteredH) >= 2.0) {
        Serial.printf("[DHT] Perubahan data stabil terdeteksi pada %s\n", sensors[i].idSensor.c_str());
        triggerUpdate(i, filteredT, filteredH);
      }
    }
  }

private:
  void triggerUpdate(int index, float t, float h) {
    sensors[index].lastTemp = t;
    sensors[index].lastHum = h;

    if (onDataChangeCallback != nullptr) {
      onDataChangeCallback(sensors[index].idSensor, t, h);
    }
  }
};