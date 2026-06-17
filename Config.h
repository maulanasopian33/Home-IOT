#pragma once

// ==========================================
// PENGATURAN KREDENSIAL & JARINGAN
// ==========================================
const char* OTA_HOSTNAME = "esp32-sensor-node";
const char* OTA_PASSWORD = "admin";
const char* AP_NAME      = "ESP32-Config-AP";

// ==========================================
// PENGATURAN API (Sekarang diurus oleh AppConfig via WiFiManager)
// ==========================================

// ==========================================
// PENGATURAN WAKTU (NTP)
// ==========================================
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 25200;  // WIB (UTC+7) -> 7 * 3600
const int   DAYLIGHT_OFFSET_SEC = 0; // Tidak ada DST di Indonesia

// ==========================================
// PENGATURAN HEALTH CHECK & HEARTBEAT
// ==========================================
const long HEARTBEAT_INTERVAL = 900000; // Kirim status setiap 15 menit (900.000 ms)

// ==========================================
// PENGATURAN HARDWARE (PIN & PARAMETER)
// ==========================================
const int LED_PIN = 2;
const long BLINK_INTERVAL = 500;

// --- TAMBAHAN UNTUK DHT ---
const long DHT_INTERVAL = 5000; // Baca sensor setiap 5 detik (mengurangi gagal baca dan self-heating)

// Tentukan 3 pin GPIO bebas untuk ESP32 (Contoh: GPIO 4, 16, dan 17)
const int DHT_PINS[3] = {4, 16, 17};
