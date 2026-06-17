#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

#include "Config.h"
#include "AppConfig.h"
#include "NetworkService.h"
#include "DHTService.h"
#include "LEDService.h"

#define WDT_TIMEOUT 15 // 15 Detik toleransi sebelum ESP auto-reboot

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
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // Memantau semua core
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
#else
  esp_task_wdt_init(WDT_TIMEOUT, true); // true = panic (reboot)
#endif

  // 2. Inisialisasi File System & Konfigurasi
  if (!LittleFS.begin(true)) {
    Serial.println("[Storage] Error: Gagal me-mount LittleFS!");
  } else {
    Serial.println("[Storage] LittleFS berhasil di-mount.");
  }
  appConfig.load(); // Load parameter dari LittleFS

  // 3. Daftarkan callback sensor
  dhtModule.onDataChange(onSensorChange);

  // 4. Inisialisasi Service dasar
  network.begin();
  dhtModule.begin();

  // 5. Pembuatan FreeRTOS Tasks (Dual-Core)
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
}

// Loop utama Arduino tidak dipakai agar memori bersih
void loop() {
  vTaskDelete(NULL);
}

// ==========================================
// FREERTOS TASKS
// ==========================================

void TaskNetwork(void *pvParameters) {
  esp_task_wdt_add(NULL); // Daftarkan task ini ke sistem pengawas WDT
  unsigned long previousHeartbeatMillis = 0;

  for(;;) {
    esp_task_wdt_reset(); // Beri makan WDT agar tidak reboot
    
    // Proses semua infrastruktur jaringan (OTA, Sync Queue, Web Server)
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
    esp_task_wdt_reset();
    
    // Baca sensor dengan tingkat presisi tinggi
    dhtModule.handle();

    // Logika Indikator LED Dinamis (Sudah Terpisah)
    ledIndicator.handle(WiFi.status() == WL_CONNECTED, network.getQueueCountFast());

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
