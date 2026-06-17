#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Config.h"
#include "AppConfig.h"

class NetworkService {
private:
  WiFiManager wm;
  WebServer server;
  bool isConnected = false;
  bool otaReady = false;
  SemaphoreHandle_t fsMutex;

  const char* offlineQueueFile = "/offline.jsonl";
  unsigned long lastSyncMillis = 0;
  const long SYNC_INTERVAL = 2000;
  int offlineQueueCount = -1; // -1 means uninitialized

  // Custom parameters for WiFiManager
  WiFiManagerParameter custom_api_url;
  WiFiManagerParameter custom_api_health_url;
  WiFiManagerParameter custom_temp_offset;

  void initOTA();
  void initWebServer();
  void handleRoot();
  void handleSync();

  String getTimeString();
  void countOfflineQueueInitially();
  void saveOfflineData(String payload);
  void syncOfflineData();

public:
  NetworkService();
  void begin();
  void handle();
  int getOfflineQueueCount();
  int getQueueCountFast() const { return (offlineQueueCount == -1) ? 0 : offlineQueueCount; }
  void sendHealthCheck(String sensorStatusJSON);
  void sendDataToAPI(const char* statusAplikasi, String idSensor = "", float suhu = -999.0, float hum = -999.0);
};
