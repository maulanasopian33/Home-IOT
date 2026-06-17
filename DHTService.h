#pragma once

#include <DHT.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "AppConfig.h"

// Struktur data untuk menyimpan status terakhir setiap sensor
struct DHTStruct {
  DHT* dhtPointer;
  int pin;
  String idSensor;
  float lastTemp;
  float lastHum;
  bool isAvailable;
  int failedReads;
  unsigned long pauseUntilMs; // Jika > 0, sensor di-skip sampai millis() melewati nilai ini
  
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
  float getAverage(float* array, int size);
  void triggerUpdate(int index, float t, float h);

public:
  void begin();
  void onDataChange(void (*callback)(String, float, float));
  String getSensorsStatusJSON();
  void handle();
};