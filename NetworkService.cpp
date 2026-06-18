#include "NetworkService.h"

// Callback global untuk memberitahu jika config dari WiFiManager harus di-save
bool shouldSaveConfig = false;
void saveConfigCallback() {
  Serial.println("[Config] Menyimpan konfigurasi baru dari WiFiManager.");
  shouldSaveConfig = true;
}

NetworkService::NetworkService() : server(8080), 
  custom_api_url("api_url", "API URL (Sensor)", appConfig.apiUrl, 128),
  custom_api_health_url("api_health", "API Health URL", appConfig.apiHealthUrl, 128),
  custom_temp_offset("temp_offset", "Suhu Offset (Cth: -5.0)", appConfig.tempOffset, 10)
{
  fsMutex = xSemaphoreCreateMutex();
}

void NetworkService::countOfflineQueueInitially() {
  if (!LittleFS.exists(offlineQueueFile)) {
    offlineQueueCount = 0;
    return;
  }
  File file = LittleFS.open(offlineQueueFile, FILE_READ);
  if (!file) {
    offlineQueueCount = 0;
    return;
  }
  offlineQueueCount = 0;
  while (file.available()) {
    if (file.read() == '\n') offlineQueueCount++;
  }
  file.close();
  Serial.printf("[Storage] Antrean dihitung: %d baris\n", offlineQueueCount);
}

int NetworkService::getOfflineQueueCount() {
  if (offlineQueueCount == -1) countOfflineQueueInitially();
  return offlineQueueCount;
}

void NetworkService::saveOfflineData(String payload) {
  if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
    File file = LittleFS.open(offlineQueueFile, FILE_APPEND);
    if (!file) {
      Serial.println("[Storage] Gagal menulis ke antrean offline!");
      xSemaphoreGive(fsMutex);
      return;
    }

    if (file.size() > 50000) {
      file.close();
      LittleFS.remove(offlineQueueFile);
      file = LittleFS.open(offlineQueueFile, FILE_WRITE);
      offlineQueueCount = 0;
    }
    
    file.println(payload);
    file.close();
    
    if (offlineQueueCount != -1) offlineQueueCount++;
    Serial.println("[Offline] Data masuk antrean lokal.");
    xSemaphoreGive(fsMutex);
  }
}

String NetworkService::getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "";
  }
  char timeStr[25];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStr);
}

void NetworkService::sendHealthCheck(String sensorStatusJSON) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // Bypass validasi CA Root agar fleksibel
    HTTPClient http;
    http.begin(client, appConfig.apiHealthUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(3000); // Mencegah blok CPU > 3 detik (WDT default 5 detik)

    #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
    #else
    DynamicJsonDocument doc(1024);
    #endif

    doc["ip"] = WiFi.localIP().toString();
    doc["hostname"] = String(OTA_HOSTNAME);
    doc["status"] = "HEALTH_CHECK";

    String waktu = getTimeString();
    if (waktu != "") doc["tanggal"] = waktu;

    doc["uptime_sec"] = millis() / 1000;
    doc["free_heap_kb"] = ESP.getFreeHeap() / 1024;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["pending_queue_count"] = getOfflineQueueCount();
    
    // Parsing String JSON sensor ke dalam Array
    #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument sensorDoc;
    #else
    DynamicJsonDocument sensorDoc(512);
    #endif
    deserializeJson(sensorDoc, sensorStatusJSON);
    doc["sensors"] = sensorDoc.as<JsonArray>();

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.printf("[Health] Sukses heartbeat. Kode: %d\n", httpCode);
    } else {
      Serial.printf("[Health] Gagal heartbeat. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void NetworkService::sendDataToAPI(const char* statusAplikasi, String idSensor, float suhu, float hum) {
  #if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
  #else
  DynamicJsonDocument doc(512);
  #endif

  doc["ip"] = WiFi.localIP().toString();
  doc["hostname"] = String(OTA_HOSTNAME);
  doc["status"] = String(statusAplikasi);

  String waktu = getTimeString();
  if (waktu != "") doc["tanggal"] = waktu;

  if (idSensor != "" && suhu != -999.0 && hum != -999.0) {
    doc["idsensor"] = idSensor;
    
    // Serialized value untuk merender format float dengan presisi satu angka di belakang koma (contoh 24.5)
    #if ARDUINOJSON_VERSION_MAJOR >= 7
    doc["suhu"] = serialized(String(suhu, 1));
    doc["kelembapan"] = serialized(String(hum, 1));
    #else
    doc["suhu"] = serialized(String(suhu, 1));
    doc["kelembapan"] = serialized(String(hum, 1));
    #endif
  }

  String payload;
  serializeJson(doc, payload);

  // PENTING: Untuk optimasi Dual-Core, kita TIDAK mengirim API secara sinkron di fungsi ini.
  // Kita paksa masuk ke LittleFS (Queue). Nanti TaskNetwork di Core 0 yang akan menembakkannya (syncOfflineData).
  // Hal ini memastikan Sensor tidak pernah terblokir oleh lambatnya jaringan WiFi.
  saveOfflineData(payload);
}

void NetworkService::syncOfflineData() {
  if (getOfflineQueueCount() <= 0) return;

  if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
    File inFile = LittleFS.open(offlineQueueFile, FILE_READ);
    if (!inFile || inFile.size() == 0) {
      if (inFile) inFile.close();
      LittleFS.remove(offlineQueueFile);
      offlineQueueCount = 0;
      xSemaphoreGive(fsMutex);
      return;
    }

    String firstLine = inFile.readStringUntil('\n');
    firstLine.trim();
    inFile.close(); // <-- PENTING: Tutup file sebelum melepas mutex

    if (firstLine.length() == 0) {
      xSemaphoreGive(fsMutex);
      return;
    }
    xSemaphoreGive(fsMutex); // Lepas mutex selama HTTP POST berjalan agar tidak memblokir Core 1
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, appConfig.apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(3000); // Mencegah blok CPU > 3 detik (WDT default 5 detik)
    
    int httpCode = http.POST(firstLine);
    bool success = (httpCode > 0);
    http.end();

    if (success) {
      Serial.printf("[Sync] 1 data berhasil disinkronkan ke server. Kode: %d\n", httpCode);
      
      // Ambil kembali mutex untuk modifikasi file queue
      if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
        File inFile2 = LittleFS.open(offlineQueueFile, FILE_READ);
        File outFile = LittleFS.open("/temp.jsonl", FILE_WRITE);
        if (inFile2 && outFile) {
          inFile2.readStringUntil('\n'); // Skip baris pertama (yang barusan terkirim)
          while (inFile2.available()) {
            String line = inFile2.readStringUntil('\n');
            line.trim();
            if(line.length() > 0) outFile.println(line);
          }
          outFile.close();
        }
        if (inFile2) inFile2.close();

        LittleFS.remove(offlineQueueFile);
        LittleFS.rename("/temp.jsonl", offlineQueueFile);
        if (offlineQueueCount > 0) offlineQueueCount--;
        xSemaphoreGive(fsMutex);
      }
    }
  }
}

void NetworkService::initOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD); // PENTING: password harus cocok dengan --auth= di espota.exe
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.printf("[OTA] Memulai update %s...\n", type.c_str());
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update Selesai! Mereboot...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Auth gagal! Cek password OTA.");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin gagal.");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect gagal.");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive gagal.");
    else if (error == OTA_END_ERROR)     Serial.println("End gagal.");
  });
  ArduinoOTA.begin();
  Serial.printf("[OTA] Siap. Hostname: %s | Port: 3232\n", OTA_HOSTNAME);
  otaReady = true;
}


static bool webServerStarted = false;
void NetworkService::initWebServer() {
  if (webServerStarted) return; // Mencegah server.begin() dipanggil berkali-kali saat reconnect
  
  server.on("/", [this]() { handleRoot(); });
  server.on("/sync", [this]() { handleSync(); });
  server.begin(8080); // Gunakan port 8080 agar tidak bentrok dengan WiFiManager (Port 80)
  Serial.println("[Web] Dashboard lokal berjalan di port 8080");
  webServerStarted = true;
}

void NetworkService::handleRoot() {
  String html = "<html><head><title>ESP32 Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family: Arial, sans-serif; background: #121212; color: #fff; padding: 20px;} table{border-collapse: collapse; width: 100%; max-width: 400px; background: #1e1e1e;} td, th{border: 1px solid #333; padding: 10px;} th{background-color: #333;} button{background: #007bff; color: white; border: none; padding: 10px 20px; font-size: 16px; border-radius: 5px; cursor: pointer;} button:hover{background: #0056b3;}</style>";
  html += "</head><body><h2>ESP32 Dashboard</h2>";
  html += "<table><tr><th>Parameter</th><th>Nilai</th></tr>";
  html += "<tr><td>Uptime</td><td>" + String(millis() / 1000) + " s</td></tr>";
  html += "<tr><td>Free RAM</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  html += "<tr><td>WiFi RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><td>Pending Queue</td><td>" + String(getOfflineQueueCount()) + " data</td></tr>";
  html += "</table><br>";
  html += "<a href='/sync'><button>Paksa Sync Sekarang</button></a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void NetworkService::handleSync() {
  syncOfflineData();
  server.sendHeader("Location", "/");
  server.send(303);
}

void NetworkService::begin() {
  // Update parameter dengan nilai terbaru dari AppConfig (yang baru saja di-load)
  custom_api_url.setValue(appConfig.apiUrl, 128);
  custom_api_health_url.setValue(appConfig.apiHealthUrl, 128);
  custom_temp_offset.setValue(appConfig.tempOffset, 10);

  // Styling Modern (Dark Mode) untuk Config Portal
  const char* custom_css = "<style>"
    "body { background-color: #121212; color: #ffffff; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }"
    ".wrap { max-width: 400px; margin: 40px auto; padding: 20px; background: #1e1e1e; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.5); text-align: center; }"
    "h1, h2, h3 { color: #007bff; font-weight: bold; margin-bottom: 15px; }"
    "button { background-color: #007bff; border: none; border-radius: 8px; padding: 12px 24px; color: white; font-size: 16px; font-weight: bold; cursor: pointer; transition: all 0.3s ease; width: 100%; margin-bottom: 10px; }"
    "button:hover { background-color: #0056b3; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(0,123,255,0.4); }"
    "input[type='text'], input[type='password'] { width: 100%; border-radius: 8px; border: 1px solid #333; padding: 12px; background-color: #2d2d2d; color: #fff; margin-bottom: 15px; font-size: 14px; outline: none; transition: 0.3s; box-sizing: border-box; }"
    "input[type='text']:focus, input[type='password']:focus { border-color: #007bff; box-shadow: 0 0 5px rgba(0,123,255,0.5); }"
    "label { display: block; text-align: left; margin-bottom: 5px; font-size: 13px; color: #bbb; }"
    "a { color: #007bff; text-decoration: none; }"
    "a:hover { text-decoration: underline; }"
    "div.msg { padding: 10px; border-radius: 8px; background: rgba(0, 123, 255, 0.1); border: 1px solid #007bff; margin-bottom: 15px; }"
    "</style>";
  wm.setCustomHeadElement(custom_css);

  // Tambahkan parameter kustom untuk dikonfigurasi via WiFiManager portal
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_api_url);
  wm.addParameter(&custom_api_health_url);
  wm.addParameter(&custom_temp_offset);
  
  // KUNCI NON-BLOCKING:
  // 1. setConnectTimeout(10): Hanya coba konek ke AP tersimpan selama 10 detik.
  //    Jika gagal, langsung lanjut ke config portal (tidak hang 60 detik).
  // 2. setConfigPortalBlocking(false): Portal AP berjalan di background.
  //    Setelah autoConnect() return, wm.process() di handle() yang menjaga portal tetap hidup.
  wm.setConnectTimeout(10);
  wm.setConfigPortalBlocking(false);
  
  if(wm.autoConnect(AP_NAME)) {
    Serial.println("[Network] Koneksi WiFi tersimpan berhasil.");
  } else {
    Serial.println("[Network] WiFi tidak ditemukan. Config portal aktif di: 192.168.4.1");
    Serial.println("[Network] Sistem sensor tetap berjalan dalam mode offline.");
  }
}


void NetworkService::handle() {
  wm.process(); 

  if (shouldSaveConfig) {
    strncpy(appConfig.apiUrl, custom_api_url.getValue(), sizeof(appConfig.apiUrl));
    strncpy(appConfig.apiHealthUrl, custom_api_health_url.getValue(), sizeof(appConfig.apiHealthUrl));
    strncpy(appConfig.tempOffset, custom_temp_offset.getValue(), sizeof(appConfig.tempOffset));
    appConfig.save();
    shouldSaveConfig = false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!isConnected) {
      isConnected = true;
      Serial.println("\n[Network] WiFi Terhubung!");
      
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
      initOTA();
      initWebServer();
      sendDataToAPI("ONLINE_STARTUP");
    }

    if (otaReady) ArduinoOTA.handle();
    server.handleClient();

    unsigned long currentMillis = millis();
    if (currentMillis - lastSyncMillis >= SYNC_INTERVAL) {
      lastSyncMillis = currentMillis;
      syncOfflineData();
    }
  } else {
    if (isConnected) {
      isConnected = false;
      Serial.println("\n[Network] WiFi Terputus! Beralih ke mode offline.");
    }

    // Auto-reconnect manual tanpa memblokir sistem (Non-blocking)
    // Mencoba menyambung kembali setiap 30 detik jika ada kredensial yang tersimpan
    static unsigned long lastReconnectAttempt = 0;
    unsigned long currentMillis = millis(); // Pastikan currentMillis selalu ter-update
    
    if (currentMillis - lastReconnectAttempt >= 30000) {
      lastReconnectAttempt = currentMillis;
      // wm.getWiFiSSID() membaca konfigurasi terakhir dari WiFiManager / NVS
      String savedSSID = wm.getWiFiSSID();
      if (savedSSID.length() > 0) {
        Serial.println("[Network] Mencoba auto-reconnect ke AP tersimpan: " + savedSSID + "...");
        WiFi.begin(savedSSID.c_str(), wm.getWiFiPass().c_str()); // Memicu driver WiFi untuk mencoba konek
      }
    }
  }
}
