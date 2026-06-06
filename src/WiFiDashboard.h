#ifndef WIFI_DASHBOARD_H
#define WIFI_DASHBOARD_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"

struct DashboardSnapshot {
  OperatingMode mode = MODE_WIFI;
  bool btConnected = false;
  bool wifiClientConnected = false;
  float batteryVoltage = 0.0f;
  uint8_t batteryPercent = 0;
  unsigned long uptimeSec = 0;
  bool hartCarrier = false;
  uint32_t hartTxBytes = 0;
  uint32_t hartRxBytes = 0;
  unsigned long hartLastActivitySecAgo = 0;
  uint32_t freeHeap = 0;
  int wifiRssi = 0;
  uint32_t cpuFreqMhz = 0;
  String macAddress;
  uint8_t chipRevision = 0;
  float chipTempC = NAN;
};

class WiFiDashboard {
public:
  WiFiDashboard();
  void begin();
  void end();
  void update(const DashboardSnapshot &snapshot);

  int available();
  uint8_t read();
  void write(uint8_t byte);
  void flush();
  bool hasClient() const;

private:
  void setupRoutes();
  String generateDashboardHtml();
  String generateStatusJson() const;
  String generateTrendJson() const;
  void handleNotFound(AsyncWebServerRequest *request);
  void checkClientConnection();
  void sampleTrend(const DashboardSnapshot &snapshot);

  bool clientConnected;
  AsyncWebServer server;
  unsigned long lastClientCheckTime;
  unsigned long bootTime;
  unsigned long lastTrendSampleTime;
  int trendIndex;
  uint16_t trendBuffer[TREND_BUFFER_SIZE];
  DashboardSnapshot snapshot;
};

#endif  // WIFI_DASHBOARD_H
