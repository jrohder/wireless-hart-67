#include "WiFiDashboard.h"

WiFiDashboard::WiFiDashboard()
    : clientConnected(false),
      server(WEB_SERVER_PORT),
      lastClientCheckTime(0),
      bootTime(0),
      lastTrendSampleTime(0),
      trendIndex(0) {
  memset(trendBuffer, 0, sizeof(trendBuffer));
}

void WiFiDashboard::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, false, WIFI_AP_MAX_CONNECTIONS);

  Serial.printf("[WiFi] AP started: %s\n", WIFI_AP_SSID);
  Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());

  setupRoutes();
  server.begin();
  bootTime = millis();
  Serial.println("[WiFi] Web server started");
}

void WiFiDashboard::end() {
  server.end();
  WiFi.softAPdisconnect(true);
  clientConnected = false;
  Serial.println("[WiFi] Disabled");
}

void WiFiDashboard::update(const DashboardSnapshot &snap) {
  snapshot = snap;
  checkClientConnection();
  sampleTrend(snap);
}

bool WiFiDashboard::hasClient() const { return clientConnected; }

void WiFiDashboard::sampleTrend(const DashboardSnapshot &snap) {
  unsigned long now = millis();
  if (now - lastTrendSampleTime < TREND_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastTrendSampleTime = now;

  uint16_t sample = snap.hartCarrier ? 1000 : 0;
  if (snap.hartLastActivitySecAgo < 2) {
    sample = 1000;
  }

  trendBuffer[trendIndex] = sample;
  trendIndex = (trendIndex + 1) % TREND_BUFFER_SIZE;
}

void WiFiDashboard::setupRoutes() {
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "text/html; charset=utf-8", generateDashboardHtml());
  });

  server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "application/json", generateStatusJson());
  });

  server.on("/api/trend", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "application/json", generateTrendJson());
  });

  server.onNotFound([this](AsyncWebServerRequest *request) {
    handleNotFound(request);
  });
}

String WiFiDashboard::generateStatusJson() const {
  const char *modeStr =
      snapshot.mode == MODE_BLUETOOTH ? "bluetooth" : "wifi";

  String json = "{";
  json += "\"mode\":\"" + String(modeStr) + "\",";
  json += "\"fw_version\":\"" + String(FW_VERSION) + "\",";
  json += "\"bt_connected\":" + String(snapshot.btConnected ? "true" : "false") + ",";
  json += "\"wifi_connected\":" +
          String(snapshot.wifiClientConnected ? "true" : "false") + ",";
  json += "\"battery_voltage\":" + String(snapshot.batteryVoltage, 2) + ",";
  json += "\"battery\":" + String(snapshot.batteryPercent) + ",";
  json += "\"uptime\":" + String(snapshot.uptimeSec) + ",";
  json += "\"hart_carrier\":" + String(snapshot.hartCarrier ? "true" : "false") + ",";
  json += "\"hart_tx\":" + String(snapshot.hartTxBytes) + ",";
  json += "\"hart_rx\":" + String(snapshot.hartRxBytes) + ",";
  json += "\"hart_last_activity\":" + String(snapshot.hartLastActivitySecAgo) + ",";
  json += "\"heap\":" + String(snapshot.freeHeap) + ",";
  json += "\"wifi_rssi\":" + String(snapshot.wifiRssi) + ",";
  json += "\"cpu_freq\":" + String(snapshot.cpuFreqMhz) + ",";
  json += "\"mac\":\"" + snapshot.macAddress + "\",";
  json += "\"chip_revision\":" + String(snapshot.chipRevision) + ",";
  if (isnan(snapshot.chipTempC)) {
    json += "\"chip_temp\":null";
  } else {
    json += "\"chip_temp\":" + String(snapshot.chipTempC, 1);
  }
  json += "}";
  return json;
}

String WiFiDashboard::generateTrendJson() const {
  String json = "[";
  for (int i = 0; i < TREND_BUFFER_SIZE; i++) {
    int idx = (trendIndex + i) % TREND_BUFFER_SIZE;
    json += String(trendBuffer[idx]);
    if (i < TREND_BUFFER_SIZE - 1) {
      json += ",";
    }
  }
  json += "]";
  return json;
}

void WiFiDashboard::checkClientConnection() {
  unsigned long now = millis();
  if (now - lastClientCheckTime < CLIENT_CHECK_INTERVAL_MS) {
    return;
  }
  lastClientCheckTime = now;

  bool hasClientNow = WiFi.softAPgetStationNum() > 0;
  if (hasClientNow != clientConnected) {
    clientConnected = hasClientNow;
    Serial.println(clientConnected ? "[WiFi] Client connected"
                                 : "[WiFi] Client disconnected");
  }
}

int WiFiDashboard::available() { return 0; }
uint8_t WiFiDashboard::read() { return 0; }
void WiFiDashboard::write(uint8_t) {}
void WiFiDashboard::flush() {}

String WiFiDashboard::generateDashboardHtml() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Wireless HART 67 Dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      background: #121212;
      color: #e0e0e0;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      padding: 16px;
    }
    .container { max-width: 640px; margin: 0 auto; }
    header {
      text-align: center;
      margin-bottom: 20px;
      padding-bottom: 12px;
      border-bottom: 2px solid #333;
    }
    h1 { font-size: 24px; color: #4fc3f7; margin-bottom: 4px; }
    .subtitle { font-size: 12px; color: #888; }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-bottom: 16px;
    }
    .card {
      background: #1e1e1e;
      border-radius: 8px;
      padding: 14px;
      border-left: 4px solid #4fc3f7;
    }
    .card-label {
      font-size: 11px;
      color: #888;
      text-transform: uppercase;
      margin-bottom: 4px;
    }
    .card-value { font-size: 18px; font-weight: 600; }
    .card-unit { font-size: 11px; color: #666; margin-left: 4px; }
    .section {
      background: #1e1e1e;
      border-radius: 8px;
      padding: 14px;
      margin-bottom: 16px;
    }
    .status-indicator {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      margin-right: 6px;
      background: #666;
      vertical-align: middle;
    }
    .status-indicator.on {
      background: #4caf50;
      box-shadow: 0 0 8px rgba(76, 175, 80, 0.5);
    }
    canvas { width: 100%; height: auto; display: block; margin-top: 8px; }
    .row { margin-bottom: 6px; font-size: 14px; }
    .refresh-time { text-align: center; font-size: 11px; color: #666; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>Wireless HART 67</h1>
      <p class="subtitle">Portable Field Communicator</p>
    </header>

    <div class="grid">
      <div class="card"><div class="card-label">Mode</div><div class="card-value" id="mode">--</div></div>
      <div class="card"><div class="card-label">Uptime</div><div class="card-value" id="uptime">0:00:00</div></div>
      <div class="card"><div class="card-label">Battery</div><div class="card-value"><span id="battery">--</span><span class="card-unit">%</span></div></div>
      <div class="card"><div class="card-label">Voltage</div><div class="card-value"><span id="voltage">--</span><span class="card-unit">V</span></div></div>
      <div class="card"><div class="card-label">Firmware</div><div class="card-value" id="fw-version">--</div></div>
      <div class="card"><div class="card-label">Free Heap</div><div class="card-value"><span id="heap">--</span><span class="card-unit">KB</span></div></div>
      <div class="card"><div class="card-label">Bluetooth</div><div class="card-value"><span class="status-indicator" id="bt-status"></span><span id="bt-text">--</span></div></div>
      <div class="card"><div class="card-label">WiFi Client</div><div class="card-value"><span class="status-indicator" id="wifi-status"></span><span id="wifi-text">--</span></div></div>
      <div class="card"><div class="card-label">WiFi RSSI</div><div class="card-value"><span id="rssi">--</span><span class="card-unit">dBm</span></div></div>
      <div class="card"><div class="card-label">CPU Freq</div><div class="card-value"><span id="cpu-freq">--</span><span class="card-unit">MHz</span></div></div>
    </div>

    <div class="section">
      <div class="card-label">HART Modem</div>
      <div class="row">Carrier: <span class="status-indicator" id="carrier-status"></span><span id="carrier-text">--</span></div>
      <div class="row">TX Bytes: <span id="hart-tx">0</span></div>
      <div class="row">RX Bytes: <span id="hart-rx">0</span></div>
      <div class="row">Last Activity: <span id="hart-last">--</span></div>
    </div>

    <div class="section">
      <div class="card-label">System</div>
      <div class="row">MAC: <span id="mac">--</span></div>
      <div class="row">Chip Rev: <span id="chip-rev">--</span></div>
      <div class="row">Chip Temp: <span id="chip-temp">--</span></div>
    </div>

    <div class="section">
      <div class="card-label">HART Activity (5 min)</div>
      <canvas id="trend-chart" width="320" height="140"></canvas>
    </div>

    <div class="refresh-time">Last updated: <span id="last-update">now</span></div>
  </div>

  <script>
    const API_INTERVAL = 1000;
    let lastUpdate = Date.now();
    let trendData = [];

    function formatUptime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
    }

    function setIndicator(el, on) {
      el.className = on ? 'status-indicator on' : 'status-indicator';
    }

    async function updateTrend() {
      try {
        const response = await fetch('/api/trend');
        trendData = await response.json();
        drawChart();
      } catch (e) {
        console.error('Trend update failed:', e);
      }
    }

    async function updateStatus() {
      try {
        const response = await fetch('/api/status');
        const data = await response.json();

        document.getElementById('mode').textContent = data.mode.toUpperCase();
        document.getElementById('uptime').textContent = formatUptime(data.uptime);
        document.getElementById('battery').textContent = data.battery;
        document.getElementById('voltage').textContent = data.battery_voltage.toFixed(2);
        document.getElementById('fw-version').textContent = data.fw_version;
        document.getElementById('heap').textContent = (data.heap / 1024).toFixed(1);
        document.getElementById('hart-tx').textContent = data.hart_tx;
        document.getElementById('hart-rx').textContent = data.hart_rx;
        document.getElementById('hart-last').textContent = data.hart_last_activity + 's ago';
        document.getElementById('rssi').textContent = data.wifi_rssi;
        document.getElementById('cpu-freq').textContent = data.cpu_freq;
        document.getElementById('mac').textContent = data.mac;
        document.getElementById('chip-rev').textContent = data.chip_revision;
        document.getElementById('chip-temp').textContent =
          data.chip_temp === null ? 'N/A' : data.chip_temp.toFixed(1) + ' °C';

        setIndicator(document.getElementById('bt-status'), data.bt_connected);
        document.getElementById('bt-text').textContent = data.bt_connected ? 'Connected' : 'Off';

        setIndicator(document.getElementById('wifi-status'), data.wifi_connected);
        document.getElementById('wifi-text').textContent = data.wifi_connected ? 'Connected' : 'Waiting';

        setIndicator(document.getElementById('carrier-status'), data.hart_carrier);
        document.getElementById('carrier-text').textContent = data.hart_carrier ? 'Detected' : 'None';

        lastUpdate = Date.now();
        document.getElementById('last-update').textContent = 'now';
      } catch (e) {
        console.error('Status update failed:', e);
      }
    }

    function drawChart() {
      const canvas = document.getElementById('trend-chart');
      const ctx = canvas.getContext('2d');
      const width = canvas.width;
      const height = canvas.height;

      ctx.fillStyle = '#1e1e1e';
      ctx.fillRect(0, 0, width, height);

      ctx.strokeStyle = '#333';
      for (let i = 0; i <= 4; i++) {
        const y = (height / 4) * i;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(width, y);
        ctx.stroke();
      }

      if (trendData.length < 2) return;

      ctx.strokeStyle = '#4fc3f7';
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let i = 0; i < trendData.length; i++) {
        const x = (width / (trendData.length - 1)) * i;
        const y = height - (trendData[i] / 1000) * height;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    updateStatus();
    updateTrend();
    setInterval(updateStatus, API_INTERVAL);
    setInterval(updateTrend, API_INTERVAL);
    setInterval(() => {
      const diff = Math.floor((Date.now() - lastUpdate) / 1000);
      document.getElementById('last-update').textContent = diff < 2 ? 'now' : diff + 's ago';
    }, 1000);
  </script>
</body>
</html>
)rawliteral";
}

void WiFiDashboard::handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not Found");
}
