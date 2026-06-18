#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

#include "Config.h"
#include "AppConfig.h"
#include "NetworkService.h"
#include "DHTService.h"
#include "LEDService.h"

#define WDT_TIMEOUT 30 // 30 Detik toleransi (network.begin() blocking max 10 detik di Core 0)

// Inisialisasi layanan jaringan dan modul sensor
NetworkService network;
DHTService dhtModule;
LEDService ledIndicator(LED_PIN);

// Deklarasi Task
void TaskNetwork(void *pvParameters);
void TaskSensor(void *pvParameters);

// Fungsi ini otomatis berjalan saat modul DHT mendeteksi sensor dicolok atau suhu berubah
void onSensorChange(String id, float temp, float hum) {
  // Hanya push ke queue. TaskNetwork yang akan kirim HTTP secara asinkron.
  network.sendDataToAPI("DHT_UPDATE", id, temp, hum);
}

void setup() {
  Serial.begin(115200);
  ledIndicator.begin();
  
  // 1. Inisialisasi Watchdog Timer (Perlindungan Crash - Dukungan Cross-Version Core v2 dan v3)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  // Reconfigure karena di Core 3.x WDT mungkin sudah diinisialisasi otomatis
  esp_task_wdt_reconfigure(&wdt_config);
#endif

  // 2. Inisialisasi File System & Konfigurasi
  if (!LittleFS.begin(true)) {
    Serial.println("[Storage] Error: Gagal me-mount LittleFS!");
  } else {
    Serial.println("[Storage] LittleFS berhasil di-mount.");
  }
  appConfig.load(); // Load parameter dari LittleFS

  // 3. Daftarkan callback sensor & inisialisasi hardware sensor
  dhtModule.onDataChange(onSensorChange);
  dhtModule.begin();

  // 4. Pembuatan FreeRTOS Tasks (Dual-Core)
  //    PENTING: Tasks dibuat SEBELUM network.begin() agar sensor langsung berjalan
  //    network.begin() (WiFiManager) dipanggil di dalam TaskNetwork sehingga
  //    hanya memblok Core 0, tidak memblok TaskSensor di Core 1.
  xTaskCreatePinnedToCore(
    TaskNetwork,   // Fungsi task
    "TaskNetwork", // Nama task
    8192,          // Stack size (WiFi dan HTTPClient butuh cukup besar)
    NULL,          // Parameter
    1,             // Priority
    NULL,          // Task handle
    0              // Dijalankan di Core 0 (Khusus Jaringan)
  );

  xTaskCreatePinnedToCore(
    TaskSensor,
    "TaskSensor",
    4096,
    NULL,
    1,
    NULL,
    1              // Dijalankan di Core 1 (Khusus Sensor & Hardware)
  );

  Serial.println("[System] FreeRTOS Dual-Core Berhasil Berjalan!");
  Serial.println("[System] Sensor mulai berjalan. WiFi diinisialisasi di latar belakang (Core 0)...");
}

// Loop utama Arduino tidak dipakai agar memori bersih
void loop() {
  vTaskDelete(NULL);
}

// ==========================================
// FREERTOS TASKS
// ==========================================

void TaskNetwork(void *pvParameters) {
  // network.begin() dipanggil di sini, di dalam task Core 0.
  // Ini akan memblok Core 0 max ~10 detik (WiFiManager connect timeout).
  // JANGAN mendaftarkan task ini ke WDT sebelum network.begin() selesai,
  // karena default WDT timeout adalah 5 detik (akan menyebabkan crash).
  network.begin();

  // Setelah selesai inisialisasi jaringan (terlepas dari berhasil atau masuk AP mode),
  // baru daftarkan task ini ke Watchdog Timer.
  esp_task_wdt_add(NULL);

  unsigned long previousHeartbeatMillis = 0;

  for(;;) {
    esp_task_wdt_reset(); // Beri makan WDT agar tidak reboot
    
    // Proses semua infrastruktur jaringan (WiFiManager portal, OTA, Sync Queue, Web Server)
    network.handle();

    // Heartbeat Status API
    unsigned long currentMillis = millis();
    if (currentMillis - previousHeartbeatMillis >= HEARTBEAT_INTERVAL || (previousHeartbeatMillis == 0 && currentMillis > 10000)) {
      previousHeartbeatMillis = currentMillis;
      String sensorStatus = dhtModule.getSensorsStatusJSON();
      network.sendHealthCheck(sensorStatus);
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS); // Istirahatkan core agar tidak overheat
  }
}

void TaskSensor(void *pvParameters) {
  esp_task_wdt_add(NULL);

  for(;;) {
    esp_task_wdt_reset(); // Feed WDT di awal loop

    // Baca sensor: esp_task_wdt_reset() dipanggil di dalam DHTService::handle() per-sensor
    dhtModule.handle();
    
    esp_task_wdt_reset(); // Feed WDT lagi setelah selesai semua pembacaan

    // Logika Indikator LED Dinamis (Sudah Terpisah)
    ledIndicator.handle(WiFi.status() == WL_CONNECTED, network.getQueueCountFast());

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
