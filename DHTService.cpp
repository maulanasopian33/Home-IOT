#include "DHTService.h"

float DHTService::getAverage(float* array, int size) {
  float sum = 0;
  for (int i = 0; i < size; i++) {
    sum += array[i];
  }
  return sum / size;
}

// Fungsi pembacaan DHT11 kustom yang TIDAK mematikan interupsi (noInterrupts/InterruptLock).
// Mengeliminasi 100% kemungkinan WDT Panic akibat IPC dari WiFiManager/Dual-Core.
bool readDHT11Safe(int pin, float &temp, float &hum) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(20); // Hold LOW untuk start signal DHT11
  digitalWrite(pin, HIGH);
  delayMicroseconds(30);
  pinMode(pin, INPUT_PULLUP);

  unsigned long timeout;
  
  // Tunggu DHT menarik LOW
  timeout = micros();
  while(digitalRead(pin) == HIGH) {
    if (micros() - timeout > 100) return false;
  }
  
  // Tunggu DHT melepas ke HIGH
  timeout = micros();
  while(digitalRead(pin) == LOW) {
    if (micros() - timeout > 100) return false;
  }
  
  // Tunggu DHT bersiap kirim data (LOW lagi)
  timeout = micros();
  while(digitalRead(pin) == HIGH) {
    if (micros() - timeout > 100) return false;
  }

  // Baca 40 bit data
  for (int i = 0; i < 40; i++) {
    timeout = micros();
    while(digitalRead(pin) == LOW) {
      if (micros() - timeout > 100) return false;
    }

    unsigned long startHigh = micros();
    while(digitalRead(pin) == HIGH) {
      if (micros() - startHigh > 100) return false;
    }
    
    // Jika HIGH lebih dari 40us, bit adalah 1
    if ((micros() - startHigh) > 40) {
      data[i / 8] |= (1 << (7 - (i % 8)));
    }
  }

  // Cek Checksum
  uint8_t sum = data[0] + data[1] + data[2] + data[3];
  if (data[4] == sum) {
    hum = data[0] + (data[1] * 0.1);
    temp = data[2] + (data[3] * 0.1);
    return true;
  }
  return false;
}

void DHTService::triggerUpdate(int index, float t, float h) {
  sensors[index].lastTemp = t;
  sensors[index].lastHum = h;

  if (onDataChangeCallback != nullptr) {
    onDataChangeCallback(sensors[index].idSensor, t, h);
  }
}

void DHTService::begin() {
  for (int i = 0; i < 3; i++) {
    sensors[i].pin = DHT_PINS[i];
    sensors[i].idSensor = "DHT11_PIN_" + String(DHT_PINS[i]);
    sensors[i].lastTemp = 0.0;
    sensors[i].lastHum = 0.0;
    sensors[i].isAvailable = false;
    sensors[i].sampleIndex = 0;
    sensors[i].failedReads = 0;
    sensors[i].pauseUntilMs = 0; // 0 = aktif, tidak dalam periode pause

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
  
  // PENTING: Tunda pembacaan pertama selama DHT_INTERVAL agar tidak bertabrakan 
  // dengan inisialisasi Flash/NVS oleh WiFiManager di Core 0.
  previousMillis = millis();
}

void DHTService::onDataChange(void (*callback)(String, float, float)) {
  onDataChangeCallback = callback;
}

String DHTService::getSensorsStatusJSON() {
  // Menggunakan ArduinoJson untuk mencegah Heap Fragmentation
  #if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
  #else
  DynamicJsonDocument doc(512);
  #endif
  
  for (int i = 0; i < 3; i++) {
    JsonObject sensor = doc.createNestedObject();
    sensor["id"] = sensors[i].idSensor;
    sensor["is_available"] = sensors[i].isAvailable;
    sensor["failed_reads"] = sensors[i].failedReads;
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

void DHTService::handle() {
  unsigned long currentMillis = millis();
  
  // PENTING: Tunda pembacaan sensor selama 15 detik pertama sejak boot.
  // WiFiManager melakukan operasi akses NVS/Flash (Core 0) pada awal boot 
  // yang memicu Inter-Processor Call (IPC). Jika bersamaan dengan 
  // DHT read yang mengunci interupsi di Core 1, akan memicu WDT Panic!
  if (currentMillis < 15000) return;
  
  // PENTING: Pastikan DHT_INTERVAL di Config.h minimal bernilai 2000 (2 detik)
  if (currentMillis - previousMillis < DHT_INTERVAL) return;
  previousMillis = currentMillis;

  // Mengambil offset dari AppConfig dinamis
  float currentOffset = appConfig.getTempOffsetFloat();

  for (int i = 0; i < 3; i++) {
    // --- DYNAMIC SENSOR: Auto-pause & Auto-recovery ---
    if (sensors[i].pauseUntilMs > 0) {
      if (currentMillis < sensors[i].pauseUntilMs) {
        // Sensor sedang di-pause, skip tanpa blocking read
        continue;
      } else {
        // Periode pause habis — coba lagi (auto-recovery jika sensor baru dicolok)
        sensors[i].pauseUntilMs = 0;
        sensors[i].failedReads = 0;
        Serial.printf("[DHT] Mencoba auto-recovery sensor %s...\n", sensors[i].idSensor.c_str());
      }
    }

    // Feed WDT sebelum setiap pembacaan: DHT11 bisa blocking ~1.2 detik/sensor saat pin kosong
    esp_task_wdt_reset();
    // Membaca menggunakan custom safe function alih-alih dhtPointer->read() yang memblokir interupsi
    float rawH = NAN;
    float rawT = NAN;
    
    bool success = readDHT11Safe(sensors[i].pin, rawT, rawH);

    // Tambahkan offset kalibrasi jika pembacaan berhasil
    if (!isnan(rawT)) {
      rawT += currentOffset;
    }

    // Filter anti-glitch. Buang jika NaN atau bernilai 0
    if (isnan(rawH) || isnan(rawT) || rawH <= 1.0 || rawT <= 1.0) {
      sensors[i].failedReads++;
      if (sensors[i].isAvailable) {
        sensors[i].isAvailable = false;
        Serial.printf("[DHT] Sensor %s TERPUTUS atau DATA INVALID!\n", sensors[i].idSensor.c_str());
      }
      // Setelah MAX_SENSOR_FAIL gagal berturut-turut, masuk mode pause
      // Sensor tidak akan di-baca selama SENSOR_RETRY_MS, lalu otomatis dicoba lagi
      if (sensors[i].failedReads >= MAX_SENSOR_FAIL) {
        sensors[i].pauseUntilMs = currentMillis + SENSOR_RETRY_MS;
        sensors[i].failedReads = 0; // Reset counter agar hitungan bersih saat retry
        Serial.printf("[DHT] Sensor %s di-pause %d detik. Akan dicoba lagi otomatis.\n",
          sensors[i].idSensor.c_str(), SENSOR_RETRY_MS / 1000);
      }
      continue; 
    }
    sensors[i].failedReads = 0; // Reset jika berhasil

    // Masukkan data ke filter Rata-rata Bergerak (Moving Average)
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
    // Threshold disesuaikan. Suhu berubah >= 1.0 atau Kelembapan >= 2.0 baru kirim API
    else if (abs(sensors[i].lastTemp - filteredT) >= 1.0 || abs(sensors[i].lastHum - filteredH) >= 2.0) {
      Serial.printf("[DHT] Perubahan data stabil terdeteksi pada %s\n", sensors[i].idSensor.c_str());
      triggerUpdate(i, filteredT, filteredH);
    }

    // PENTING: Berikan jeda (yield) antar pembacaan sensor.
    // Membaca DHT memblokir interupsi sementara. Jeda 50ms ini memberi waktu
    // bagi Core 1 untuk memproses IPC dari Core 0 (seperti aksi simpan config WiFi ke Flash).
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
