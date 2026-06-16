#include "Config.h"
#include "NetworkService.h"

// Inisialisasi layanan jaringan
NetworkService network;

// Variabel Aplikasi
unsigned long previousMillis = 0;
int ledState = LOW;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Jalankan infrastruktur jaringan di background
  network.begin();
  
  Serial.println("Sistem Aplikasi Utama Dimulai!");
}

void loop() {
  // 1. WAJIB: Biarkan layanan jaringan beroperasi
  network.handle();

  // 2. Logika Aplikasi Anda (Hardware / Sensor)
  jalankanBlink();
}

// --- FUNGSI APLIKASI ---
void jalankanBlink() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= BLINK_INTERVAL) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}
