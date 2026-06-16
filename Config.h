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
const char* API_URL = "http://192.168.1.100/api.php"; 

// ==========================================
// PENGATURAN HARDWARE (PIN & PARAMETER)
// ==========================================
const int LED_PIN = 2;
const long BLINK_INTERVAL = 500;
