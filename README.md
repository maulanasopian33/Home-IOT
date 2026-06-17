# 🌐 ESP32 Home IoT - Enterprise Grade Multi-Sensor

![Version](https://img.shields.io/badge/version-2.1-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![Framework](https://img.shields.io/badge/framework-Arduino-orange.svg)
![Architecture](https://img.shields.io/badge/architecture-FreeRTOS-purple.svg)

Firmware IoT tingkat lanjut berbasis ESP32 untuk memonitor multi-sensor suhu (DHT11) dengan presisi ekstrim. Proyek ini telah direkayasa ulang (*refactored*) dari kode konvensional menjadi arsitektur **Dual-Core FreeRTOS**, dirancang untuk beroperasi secara mandiri bertahun-tahun tanpa *crash* berkat perlindungan **Hardware Watchdog** dan **Offline Data Queue (LittleFS)**.

---

## ✨ Fitur Unggulan

- **🚀 Dual-Core Asynchronous (FreeRTOS)**
  Beban kerja dipisah secara ekstrem:
  - **Core 1** didedikasikan 100% untuk pembacaan sensor berpresisi tinggi (dengan filter *Moving Average*).
  - **Core 0** menangani lalulintas jaringan yang berat (HTTP POST, WiFi, WebServer).
- **💾 Anti-Data Loss (LittleFS Offline Queue)**
  Jika WiFi rumah terputus atau server tujuan mati, data tidak akan terbuang! Data akan dikunci ke dalam antrean *file system* ESP32 yang dilindungi *Mutex* dan akan ditembakkan satu per satu saat internet kembali pulih.
- **🛡️ Auto-Recovery (Hardware Watchdog Timer)**
  ESP32 dilengkapi WDT 15 detik. Apabila sistem mengalami *Kernel Panic* atau koneksi jaringan memblokir perangkat, listrik akan diputus dan sistem akan di-*reboot* otomatis.
- **⚙️ Dynamic Captive Portal (WiFiManager)**
  Tidak ada *Hardcoding*! URL API, API Health, dan Suhu Offset Kalibrasi dikonfigurasi melalui Web Interface saat alat pertama kali menyala (Mode AP `ESP32-Config-AP`).
- **💡 Smart LED Indicator**
  Status sistem dapat dianalisis tanpa *Serial Monitor* hanya dengan melihat kedipan LED biru bawaan:
  - 🔴 **Fast Blink (200ms)**: Tidak ada internet / Menunggu konfigurasi WiFi.
  - 🟡 **Sync Blink (500ms)**: Terkoneksi, namun sedang bekerja keras memompa antrean data *offline* ke server.
  - 🟢 **Heartbeat (Nyala 100ms, Mati 2s)**: Kondisi Normal & Sempurna (Alat sedang *idle* & hemat daya).
- **📊 Local Web Dashboard**
  Akses IP ESP32 lewat peramban web lokal untuk melihat langsung status *Free RAM*, Sinyal (RSSI), *Uptime*, dan beban antrean.

---

## 🛠️ Persiapan & Cara Instalasi

### 1. Kebutuhan Library Arduino IDE
Pastikan Anda menginstal *library* berikut lewat `Library Manager`:
1. `DHT sensor library` by Adafruit
2. `WiFiManager` by tzapu
3. `ArduinoJson` by Benoit Blanchon (Wajib untuk Anti-Memory Leak)

### 2. Pengaturan Board & Partisi (Sangat Penting ⚠️)
Karena sistem Queue dan penyimpan URL menggunakan memori fisik, Anda **wajib** menyisakan ruang memori untuk *File System*.
Pilih skema partisi di Arduino IDE: `Tools` ➔ `Partition Scheme` ➔ **`Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)`**.

### 3. Kompilasi & Konfigurasi
1. Tekan tombol `Upload` ke ESP32 Anda.
2. Cabut kabel USB dan hubungkan ke Power Bank / Adaptor.
3. Cari WiFi bernama **`ESP32-Config-AP`** dari HP Anda.
4. Jendela akan muncul, masukkan *SSID*, *Password*, *URL API*, dan *Offset Kalibrasi*.
5. Simpan dan biarkan perangkat mandiri terkoneksi ke Server Anda!

---

## 📦 Dokumentasi JSON Payload API

ESP32 mengirim data menggunakan antarmuka **HTTP POST (application/json)** ke URL Backend yang telah Anda tentukan.

### A. Endpoint Utama (Data Sensor - Dikirim jika ada perubahan cuaca)
```json
{
  "ip": "192.168.1.50",
  "hostname": "esp32-sensor-node",
  "status": "DHT_UPDATE",
  "tanggal": "2026-06-17 10:15:20",
  "idsensor": "DHT11_PIN_4",
  "suhu": "24.5",
  "kelembapan": "73.0"
}
```

### B. Endpoint Health Check (Heartbeat - Rutin per 15 Menit)
```json
{
  "ip": "192.168.1.50",
  "hostname": "esp32-sensor-node",
  "status": "HEALTH_CHECK",
  "tanggal": "2026-06-17 10:30:00",
  "uptime_sec": 900,
  "free_heap_kb": 124,
  "wifi_rssi": -62,
  "pending_queue_count": 0,
  "sensors": [
    {"id": "DHT11_PIN_4", "is_available": true, "failed_reads": 0},
    {"id": "DHT11_PIN_16", "is_available": true, "failed_reads": 0},
    {"id": "DHT11_PIN_17", "is_available": false, "failed_reads": 3}
  ]
}
```

---

## 🏛️ Struktur Direktori Moduler
Proyek ini mengadopsi standar kode bersih (C++) yang memisahkan logika dari fungsi utama `loop()`.
- `AppConfig` : Menyimpan pengaturan file JSON dinamis.
- `DHTService`: Berisi filter data (anti glitch) dan inisialisasi pin sensor.
- `NetworkService`: Berisi sistem antrean *LittleFS*, pengirim *HTTP Payload*, dan *Local Web Dashboard*.
- `LEDService` : Mengatur logika indikator pola kedipan perangkat.
- `Sensor_Suhu.ino`: Berfungsi murni sebagai pendefinisi utas (*Task*) untuk *FreeRTOS*.

---
Diciptakan untuk stabilitas tanpa kompromi. ☕ Happy Coding!
