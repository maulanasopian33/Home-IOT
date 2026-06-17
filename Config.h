#pragma once

// ==========================================
// PENGATURAN KREDENSIAL & JARINGAN
// ==========================================
const char* OTA_HOSTNAME = "esp32-mending-coding";
const char* OTA_PASSWORD = "admin";
const char* AP_NAME      = "ESP32-Config-AP";

// ==========================================
// PENGATURAN API
// ==========================================
// Ganti dengan URL server API Anda
const char* API_URL = "https://maulanasopian.my.id/api.php"; 

// ==========================================
// PENGATURAN WAKTU (NTP)
// ==========================================
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 25200;  // WIB (UTC+7) -> 7 * 3600
const int   DAYLIGHT_OFFSET_SEC = 0; // Tidak ada DST di Indonesia

// ==========================================
// PENGATURAN HARDWARE (PIN & PARAMETER)
// ==========================================
const int LED_PIN = 2;
const long BLINK_INTERVAL = 500;

// --- TAMBAHAN UNTUK DHT ---
const long DHT_INTERVAL = 5000; // Baca sensor setiap 5 detik (mengurangi gagal baca dan self-heating)

// Nilai offset kalibrasi suhu (contoh: kurangi 5 derajat)
const float TEMP_OFFSET = -5.0;

// Tentukan 3 pin GPIO bebas untuk ESP32 (Contoh: GPIO 4, 16, dan 17)
const int DHT_PINS[3] = {4, 16, 17};
