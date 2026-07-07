# ESP8266 自包含天气站 — 实施计划

> **For agentic workers:** Use subagent-driven-development or executing-plans to implement task-by-task.
>
> **目标:** 在 ESP8266（NodeMCU）上实现一个自包含天气站 — 通过 WiFiManager 配网 → HTTPS 获取 Open-Meteo 天气数据 → 内存缓存 → 内嵌 Web Dashboard（Chart.js）
>
> **架构:** 单芯片方案。ESPAsyncWebServer 提供 HTTP 服务，主循环中定时执行同步 HTTPS 获取天气数据，存入环形缓冲区，Dashboard 通过 REST API 读取数据。
>
> **技术栈:** C++ (Arduino框架), PlatformIO, ESPAsyncWebServer, WiFiManager, ArduinoJson, BearSSL, Chart.js

## 全局约束

- ESP8266 可用堆 ~50KB，SSL 连接需 ~8-15KB
- 代码必须编译通过无 warning
- 所有字符串常量使用 PROGMEM 存于 Flash
- 所有定时任务使用 `millis()` 非阻塞模式
- 无动态内存分配（除 ArduinoJson 解析临时使用）
- Open-Meteo API: `https://api.open-meteo.com/v1/forecast`
- 默认坐标: 上海 (31.23, 121.47)

---
### 文件结构

```
src/
├── main.cpp              入口：setup/loop/调度器
├── config.h              编译时常量：API URL、坐标、引脚
├── WeatherCodes.h        WMO天气码 → 文字描述 PROGMEM 表
├── WeatherFetcher.cpp/h  HTTPS获取 + JSON解析
├── RingBuffer.cpp/h      环形缓冲区（24条记录）
└── WebServer.cpp/h       AsyncWebServer + DashboardHtml 内嵌页面
```
---

## 任务分解

### Task 1: 项目配置和基础文件

**文件:**
- 修改: `platformio.ini`
- 创建: `src/config.h`
- 创建: `src/WeatherCodes.h`

- [ ] **Step 1: 更新 platformio.ini** — 添加库依赖

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
upload_speed = 921600
build_flags =
    -DWEATHER_LOCATION_LAT=31.23
    -DWEATHER_LOCATION_LON=121.47
    -DWEATHER_LOCATION_NAME=Shanghai
    -DTZ_OFFSET=28800

lib_deps =
    tzapu/WiFiManager @ ^2.0.18
    me-no-dev/ESPAsyncWebServer @ ^3.7.1
    me-no-dev/ESPAsyncTCP @ ^1.2.2
    bblanchon/ArduinoJson @ ^6.21.5
```

- [ ] **Step 2: 创建 config.h**

```cpp
#pragma once
#include <cstdint>

// ---- WiFi ----
#define WIFI_AP_NAME        "ESP-Weather-Station"
#define WIFI_AP_PASSWORD    "weather123"

// ---- Open-Meteo API ----
#define API_HOST            "api.open-meteo.com"
#define API_PORT            443
#define API_URL             "/v1/forecast?latitude=" \
                            STRINGIFY(WEATHER_LOCATION_LAT) \
                            "&longitude=" \
                            STRINGIFY(WEATHER_LOCATION_LON) \
                            "&current=temperature_2m," \
                            "relative_humidity_2m," \
                            "apparent_temperature," \
                            "weather_code,wind_speed_10m," \
                            "wind_direction_10m&timezone=auto"

// ---- Timing ----
#define WEATHER_INTERVAL_MS (10 * 60 * 1000UL)  // 10分钟
#define NTP_INTERVAL_MS     (60 * 60 * 1000UL)  // 1小时
#define RETRY_DELAY_MS      (30 * 1000UL)       // 失败重试延迟

// ---- Ring Buffer ----
#define MAX_RECORDS 24

// ---- Data Structures ----
struct WeatherRecord {
    time_t timestamp;
    float temp;            // °C
    float feels_like;      // °C
    uint8_t humidity;      // %
    float wind_speed;      // km/h
    uint16_t wind_dir;     // 度
    int8_t weather_code;   // WMO code
};

#define STRINGIFY(x) #x
```

- [ ] **Step 3: 创建 WeatherCodes.h**

```cpp
#pragma once
#include <Arduino.h>
#include <pgmspace.h>

struct WeatherCodeEntry {
    int8_t code;
    const char* label;
};

// WMO Weather interpretation codes (WW)
// https://open-meteo.com/en/docs#weathervariables
static const WeatherCodeEntry WEATHER_CODES[] PROGMEM = {
    {0,   "☀️ 晴"},
    {1,   "🌤️ 大部晴朗"},
    {2,   "⛅ 多云"},
    {3,   "☁️ 阴"},
    {45,  "🌫️ 雾"},
    {48,  "🌫️ 雾凇"},
    {51,  "🌦️ 小毛毛雨"},
    {53,  "🌦️ 毛毛雨"},
    {55,  "🌧️ 大毛毛雨"},
    {56,  "🌦️ 冻毛毛雨"},
    {57,  "🌧️ 冻大毛毛雨"},
    {61,  "🌦️ 小雨"},
    {63,  "🌧️ 中雨"},
    {65,  "🌧️ 大雨"},
    {66,  "🌧️ 冻雨"},
    {67,  "🌧️ 冻大雨"},
    {71,  "❄️ 小雪"},
    {73,  "❄️ 中雪"},
    {75,  "❄️ 大雪"},
    {77,  "❄️ 雪粒"},
    {80,  "🌦️ 小阵雨"},
    {81,  "🌧️ 中阵雨"},
    {82,  "🌧️ 大阵雨"},
    {85,  "❄️ 小阵雪"},
    {86,  "❄️ 大阵雪"},
    {95,  "⛈️ 雷暴"},
    {96,  "⛈️ 雷暴+小冰雹"},
    {99,  "⛈️ 雷暴+大冰雹"},
};

static constexpr int WEATHER_CODE_COUNT = sizeof(WEATHER_CODES) / sizeof(WEATHER_CODES[0]);

inline const char* getWeatherLabel(int8_t code) {
    for (int i = 0; i < WEATHER_CODE_COUNT; i++) {
        WeatherCodeEntry entry;
        memcpy_P(&entry, &WEATHER_CODES[i], sizeof(WeatherCodeEntry));
        if (entry.code == code) return entry.label;
    }
    return "❓ 未知";
}

inline const char* getWindDirLabel(uint16_t deg) {
    if (deg < 22)  return "北";
    if (deg < 67)  return "东北";
    if (deg < 112) return "东";
    if (deg < 157) return "东南";
    if (deg < 202) return "南";
    if (deg < 247) return "西南";
    if (deg < 292) return "西";
    if (deg < 337) return "西北";
    return "北";
}
```

- [ ] **Step 4: 编译验证**

```bash
pio run
```
预期：编译成功，无报错。

- [ ] **Step 5: 提交**

```bash
git add platformio.ini src/config.h src/WeatherCodes.h
git commit -m "feat(config): add platform configuration and weather code tables"
```

---

### Task 2: RingBuffer — 环形缓冲区

**文件:**
- 创建: `src/RingBuffer.h`
- 创建: `src/RingBuffer.cpp`

**接口:**
- 消费: `config.h` 中 `WeatherRecord` 结构体
- 生产: `ring_init()`, `ring_push(rec) → bool`, `ring_latest(out) → bool`, `ring_get_all(out, max) → int`, `ring_count() → int`

- [ ] **Step 1: 创建 RingBuffer.h**

```cpp
#pragma once
#include "config.h"

void    ring_init();
bool    ring_push(const WeatherRecord& rec);
bool    ring_latest(WeatherRecord* out);
int     ring_get_all(WeatherRecord* out, int max);
int     ring_count();
```

- [ ] **Step 2: 创建 RingBuffer.cpp**

```cpp
#include "RingBuffer.h"
#include <string.h>

static WeatherRecord buffer[MAX_RECORDS];
static uint8_t head = 0;
static uint8_t count = 0;

void ring_init() {
    head = 0;
    count = 0;
    memset(buffer, 0, sizeof(buffer));
}

bool ring_push(const WeatherRecord& rec) {
    buffer[head] = rec;
    head = (head + 1) % MAX_RECORDS;
    if (count < MAX_RECORDS) count++;
    return true;
}

bool ring_latest(WeatherRecord* out) {
    if (count == 0) return false;
    uint8_t idx = (head == 0) ? MAX_RECORDS - 1 : head - 1;
    *out = buffer[idx];
    return true;
}

int ring_get_all(WeatherRecord* out, int max) {
    int n = (count < max) ? count : max;
    uint8_t start = (count < MAX_RECORDS) ? 0 : head;
    for (int i = 0; i < n; i++) {
        out[i] = buffer[(start + i) % MAX_RECORDS];
    }
    return n;
}

int ring_count() {
    return count;
}
```

- [ ] **Step 3: 编译验证**

```bash
pio run
```
预期：编译成功。

- [ ] **Step 4: 提交**

```bash
git add src/RingBuffer.h src/RingBuffer.cpp
git commit -m "feat: add RingBuffer for weather data history"
```

---

### Task 3: WeatherFetcher — HTTPS 天气数据获取

**文件:**
- 创建: `src/WeatherFetcher.h`
- 创建: `src/WeatherFetcher.cpp`

**接口:**
- 消费: `config.h`, `RingBuffer.h`
- 生产: `weather_init()`, `weather_fetch(rec*) → bool`

- [ ] **Step 1: 创建 WeatherFetcher.h**

```cpp
#pragma once
#include "config.h"

void    weather_init();
bool    weather_fetch(WeatherRecord* out);
```

- [ ] **Step 2: 创建 WeatherFetcher.cpp**

关键要点：
- 使用 `BearSSL::WiFiClientSecure`（全局复用）
- `setInsecure()` — 演示项目跳过证书验证
- `setBufferSizes(1024, 512)` — MFLN 优化节省内存
- 使用 `HTTPClient` 同步短连接
- ArduinoJson 过滤解析，只提取需要的字段
- 从 `responseStream()` 直接解析，避免 String 中转

```cpp
#include "WeatherFetcher.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <BearSSLHelpers.h>
#include <ArduinoJson.h>

static BearSSL::WiFiClientSecure client;

void weather_init() {
    client.setInsecure();
    client.setBufferSizes(1024, 512);  // MFLN: 接收缓冲从16KB→512B
}

bool weather_fetch(WeatherRecord* out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);

    bool ok = false;
    http.begin(client, String("https://") + API_HOST + API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        // 使用过滤，只解析需要的字段 —— 大幅减少内存占用
        StaticJsonDocument<96> filter;
        JsonObject filter_current = filter["current"].to<JsonObject>();
        filter_current["temperature_2m"] = true;
        filter_current["relative_humidity_2m"] = true;
        filter_current["apparent_temperature"] = true;
        filter_current["weather_code"] = true;
        filter_current["wind_speed_10m"] = true;
        filter_current["wind_direction_10m"] = true;

        // 使用 Stream 直接解析，避免 String 中转
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, http.getStream(),
            DeserializationOption::Filter(filter));

        if (!err) {
            JsonObject current = doc["current"];
            out->temp       = current["temperature_2m"];
            out->feels_like = current["apparent_temperature"];
            out->humidity   = current["relative_humidity_2m"];
            out->wind_speed = current["wind_speed_10m"];
            out->wind_dir   = current["wind_direction_10m"];
            out->weather_code = current["weather_code"];
            out->timestamp  = time(nullptr);
            ok = true;
        }
    }

    http.end();
    return ok;
}
```

- [ ] **Step 3: 编译验证**

```bash
pio run
```
预期：编译成功。

- [ ] **Step 4: 提交**

```bash
git add src/WeatherFetcher.h src/WeatherFetcher.cpp
git commit -m "feat: add WeatherFetcher for Open-Meteo HTTPS data"
```

---

### Task 4: WebServer — HTTP 服务 + REST API

**文件:**
- 创建: `src/WebServer.h`
- 创建: `src/WebServer.cpp`
- 创建: `src/DashboardHtml.h`（内嵌 HTML PROGMEM）

**接口:**
- 消费: `RingBuffer.h`, `WeatherCodes.h`
- 生产: `webserver_init()`

- [ ] **Step 1: 创建 WebServer.h**

```cpp
#pragma once

void webserver_init();
```

- [ ] **Step 2: 创建 WebServer.cpp**

路由表：
| 路径 | 方法 | 说明 |
|------|------|------|
| `/` | GET | 返回 Dashboard HTML 页面 |
| `/api/weather` | GET | 返回最新+历史 JSON 数据 |
| `/api/reset` | GET | 重置 WiFi 配置（重新配网）|

```cpp
#include "WebServer.h"
#include "DashboardHtml.h"
#include "RingBuffer.h"
#include "WeatherCodes.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);

void webserver_init() {
    // 主页: Dashboard HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp = request->beginResponse_P(
            200, "text/html", DASHBOARD_HTML, DASHBOARD_HTML_SIZE);
        request->send(resp);
    });

    // API: 天气数据 JSON
    server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest* request) {
        // 分配约 3KB 的动态 JSON（堆临时使用，请求完成后释放）
        const size_t capacity = JSON_OBJECT_SIZE(3)
            + JSON_OBJECT_SIZE(7)
            + JSON_ARRAY_SIZE(MAX_RECORDS)
            + MAX_RECORDS * JSON_OBJECT_SIZE(4)
            + 400;
        DynamicJsonDocument doc(capacity);

        JsonObject root = doc.to<JsonObject>();

        // 当前数据
        WeatherRecord latest;
        bool hasData = ring_latest(&latest);
        if (hasData) {
            JsonObject current = root["current"].to<JsonObject>();
            current["temp"]        = latest.temp;
            current["feels_like"]  = latest.feels_like;
            current["humidity"]    = latest.humidity;
            current["wind_speed"]  = latest.wind_speed;
            current["wind_dir"]    = latest.wind_dir;
            current["wind_label"]  = getWindDirLabel(latest.wind_dir);
            current["weather_code"] = latest.weather_code;
            current["weather_text"] = getWeatherLabel(latest.weather_code);
            current["timestamp"]   = (long)latest.timestamp;
        }

        // 历史数据
        JsonArray hist = root["history"].to<JsonArray>();
        WeatherRecord records[MAX_RECORDS];
        int n = ring_get_all(records, MAX_RECORDS);
        for (int i = 0; i < n; i++) {
            JsonObject item = hist.createNestedObject();
            item["time"]    = (long)records[i].timestamp;
            item["temp"]    = records[i].temp;
            item["humidity"] = records[i].humidity;
        }

        // 系统状态
        JsonObject sys = root["system"].to<JsonObject>();
        sys["uptime"]    = (long)(millis() / 1000);
        sys["free_heap"] = ESP.getFreeHeap();
        sys["rssi"]      = WiFi.RSSI();
        sys["records"]   = ring_count();
        sys["location"]  = WEATHER_LOCATION_NAME;

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // 重置配网
    server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "OK - resetting WiFi config...");
        delay(100);
        WiFi.disconnect(true);
        ESP.restart();
    });

    // 默认 404
    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });

    server.begin();
}
```

- [ ] **Step 3: 创建 DashboardHtml.h**

内嵌 HTML 页面（使用 PROGMEM 存储），包含：
- 响应式 CSS（移动端+桌面）
- Chart.js CDN 加载
- 实时数据卡片区
- 温度曲线图
- 系统状态区
- 30 秒自动轮询

```cpp
#pragma once
#include <pgmspace.h>

// HTML Dashboard - 使用 PROGMEM 存于 Flash，不占 RAM
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP8266 天气站</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.7/dist/chart.umd.min.js"></script>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #f0f4f8; color: #1a202c; padding: 16px; min-height: 100vh;
  }
  .container { max-width: 800px; margin: 0 auto; }
  h1 {
    font-size: 1.3rem; text-align: center; padding: 12px 0;
    color: #2b6cb0; display: flex; align-items: center; justify-content: center;
    gap: 8px;
  }
  .update-time { text-align: center; color: #718096; font-size: 0.8rem; margin-bottom: 16px; }
  .cards {
    display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 12px; margin-bottom: 16px;
  }
  .card {
    background: white; border-radius: 12px; padding: 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1); text-align: center;
  }
  .card .label { font-size: 0.8rem; color: #718096; margin-bottom: 4px; }
  .card .value {
    font-size: 1.8rem; font-weight: 700; color: #2d3748;
  }
  .card .unit { font-size: 0.9rem; color: #a0aec0; margin-left: 2px; }
  .card .sub { font-size: 0.75rem; color: #718096; margin-top: 4px; }
  .temp-color { color: #e53e3e; }
  .humidity-color { color: #3182ce; }
  .wind-color { color: #38a169; }
  .chart-container {
    background: white; border-radius: 12px; padding: 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 16px;
  }
  .status-bar {
    background: white; border-radius: 12px; padding: 12px 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    font-size: 0.8rem; color: #718096;
    display: flex; flex-wrap: wrap; justify-content: space-between; gap: 8px;
  }
  .loading { text-align: center; color: #a0aec0; padding: 40px; }
  .error { text-align: center; color: #e53e3e; padding: 20px; display: none; }
  @media (max-width: 480px) {
    .cards { grid-template-columns: repeat(2, 1fr); }
    .card .value { font-size: 1.4rem; }
  }
</style>
</head>
<body>
<div class="container">
  <h1>☀️ <span id="location">天气站</span></h1>
  <div class="update-time">⏱️ 更新于: <span id="updateTime">--</span></div>

  <div class="cards">
    <div class="card">
      <div class="label">🌡️ 温度</div>
      <div class="value temp-color"><span id="temp">--</span><span class="unit">°C</span></div>
      <div class="sub">体感 <span id="feels">--</span>°C</div>
    </div>
    <div class="card">
      <div class="label">💧 湿度</div>
      <div class="value humidity-color"><span id="humidity">--</span><span class="unit">%</span></div>
    </div>
    <div class="card">
      <div class="label">🌬️ 风速</div>
      <div class="value wind-color"><span id="wind">--</span><span class="unit">km/h</span></div>
      <div class="sub">风向 <span id="windDir">--</span></div>
    </div>
    <div class="card">
      <div class="label">☁️ 天气</div>
      <div class="value" style="font-size:1.3rem"><span id="weatherText">--</span></div>
    </div>
  </div>

  <div class="chart-container">
    <canvas id="tempChart" height="200"></canvas>
  </div>

  <div class="status-bar">
    <span>📶 WiFi: <span id="rssi">--</span> dBm</span>
    <span>🧠 内存: <span id="heap">--</span> KB</span>
    <span>⏱️ 运行: <span id="uptime">--</span></span>
    <span>📊 记录: <span id="records">0</span> 条</span>
  </div>

  <div class="error" id="errorMsg">⚠️ 无法获取数据，正在重试...</div>
</div>

<script>
let tempChart = null;
const COLORS = { temp: '#e53e3e', fill: '#fed7d7', humidity: '#3182ce' };

function formatTime(ts) {
  const d = new Date(ts * 1000);
  return d.toLocaleString('zh-CN', { hour: '2-digit', minute: '2-digit' });
}

function formatUptime(sec) {
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (d > 0) return d + '天' + h + '时';
  if (h > 0) return h + '时' + m + '分';
  return m + '分';
}

function updateDashboard(data) {
  document.getElementById('errorMsg').style.display = 'none';
  document.getElementById('location').textContent = data.system.location || '天气站';

  const c = data.current;
  if (c) {
    document.getElementById('temp').textContent = c.temp != null ? c.temp.toFixed(1) : '--';
    document.getElementById('feels').textContent = c.feels_like != null ? c.feels_like.toFixed(1) : '--';
    document.getElementById('humidity').textContent = c.humidity != null ? c.humidity : '--';
    document.getElementById('wind').textContent = c.wind_speed != null ? c.wind_speed.toFixed(1) : '--';
    document.getElementById('windDir').textContent = c.wind_label || '--';
    document.getElementById('weatherText').textContent = c.weather_text || '--';
    document.getElementById('updateTime').textContent = c.timestamp ? formatTime(c.timestamp) : '--';
  }

  const sys = data.system;
  if (sys) {
    document.getElementById('rssi').textContent = sys.rssi || '--';
    document.getElementById('heap').textContent = sys.free_heap ? (sys.free_heap / 1024).toFixed(0) : '--';
    document.getElementById('uptime').textContent = sys.uptime ? formatUptime(sys.uptime) : '--';
    document.getElementById('records').textContent = sys.records || 0;
  }

  // Chart.js 温度曲线
  const hist = data.history || [];
  const labels = hist.map(r => r.time ? formatTime(r.time) : '');
  const temps = hist.map(r => r.temp != null ? r.temp : null);

  if (tempChart) {
    tempChart.data.labels = labels;
    tempChart.data.datasets[0].data = temps;
    tempChart.update('none');
  } else if (labels.length > 0) {
    const ctx = document.getElementById('tempChart').getContext('2d');
    tempChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: labels,
        datasets: [{
          label: '温度 (°C)',
          data: temps,
          borderColor: COLORS.temp,
          backgroundColor: COLORS.fill,
          fill: true,
          tension: 0.3,
          pointRadius: 3,
          pointHoverRadius: 6,
          spanGaps: true
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: true,
        plugins: { legend: { display: false } },
        scales: {
          x: {
            ticks: { maxTicksLimit: 8, font: { size: 10 } },
            grid: { display: false }
          },
          y: {
            ticks: { font: { size: 10 } },
            grid: { color: '#e2e8f0' }
          }
        }
      }
    });
  }
}

function fetchWeather() {
  fetch('/api/weather')
    .then(r => r.json())
    .then(data => updateDashboard(data))
    .catch(err => {
      document.getElementById('errorMsg').style.display = 'block';
    });
}

// 首次加载
fetchWeather();
// 30秒轮询
setInterval(fetchWeather, 30000);
</script>
</body>
</html>
)rawliteral";

static const size_t DASHBOARD_HTML_SIZE = sizeof(DASHBOARD_HTML);
```

- [ ] **Step 4: 编译验证**

```bash
pio run
```
预期：编译成功。

- [ ] **Step 5: 提交**

```bash
git add src/WebServer.h src/WebServer.cpp src/DashboardHtml.h
git commit -m "feat: add async web server with dashboard HTML"
```

---

### Task 5: main.cpp — 整合所有组件

**文件:**
- 修改: `src/main.cpp`

- [ ] **Step 1: 编写 main.cpp**

关键逻辑：
- `setup()`: Serial → WiFiManager 配网 → NTP → weather_init → webserver_init → 首次数据获取
- `loop()`: 10 分钟定时获取天气 → 存入 RingBuffer → 喂狗

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include "config.h"
#include "RingBuffer.h"
#include "WeatherFetcher.h"
#include "WebServer.h"
#include "WeatherCodes.h"

// ---- 调度状态 ----
static unsigned long lastWeatherFetch = 0;
static unsigned long lastNtpSync = 0;
static bool ntpSynced = false;

// ---- 函数声明 ----
void syncNtp();
void doWeatherFetch();

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("===================================");
    Serial.println("  ESP8266 Weather Station v1.0");
    Serial.println("===================================");

    // 1. WiFiManager 配网
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setConnectTimeout(15);

    bool connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
    if (!connected) {
        Serial.println("[ERR] WiFi 配网失败，重启...");
        ESP.restart();
    }
    Serial.print("[OK] WiFi 已连接: ");
    Serial.println(WiFi.SSID());
    Serial.print("    IP 地址: ");
    Serial.println(WiFi.localIP());

    // 2. 初始化学组件
    ring_init();
    weather_init();
    webserver_init();

    // 3. NTP 时间同步
    syncNtp();

    // 4. 首次数据获取
    doWeatherFetch();

    Serial.println("\n[READY] 打开浏览器访问 http://" + WiFi.localIP().toString());
}

void loop() {
    unsigned long now = millis();

    // 每60分钟同步NTP
    if (now - lastNtpSync >= NTP_INTERVAL_MS) {
        syncNtp();
        lastNtpSync = now;
    }

    // 每10分钟获取天气数据
    if (now - lastWeatherFetch >= WEATHER_INTERVAL_MS) {
        doWeatherFetch();
        lastWeatherFetch = now;
    }

    // ESP8266 喂狗（硬件WDT自动运行，但保持loop不太忙）
    yield();
    delay(10);
}

void syncNtp() {
    if (WiFi.status() != WL_CONNECTED) return;

    configTime(TZ_OFFSET, 0, "ntp.aliyun.com", "ntp.ntsc.ac.cn", "time.nist.gov");
    Serial.print("[NTP] 同步时间...");

    time_t now = time(nullptr);
    int retries = 20;
    while (now < 100000 && retries-- > 0) {
        delay(500);
        now = time(nullptr);
    }

    if (now > 100000) {
        ntpSynced = true;
        Serial.print(" OK: ");
        Serial.println(ctime(&now));
    } else {
        Serial.println(" 失败 (使用 unix epoch)");
    }
}

void doWeatherFetch() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WARN] WiFi 未连接，跳过获取");
        return;
    }

    Serial.print("[WEATHER] 获取数据... ");
    WeatherRecord rec;
    bool ok = weather_fetch(&rec);

    if (ok) {
        ring_push(rec);
        Serial.print("OK: ");
        Serial.print(rec.temp);
        Serial.print("°C, ");
        Serial.print(rec.humidity);
        Serial.print("%, ");
        Serial.println(getWeatherLabel(rec.weather_code));
    } else {
        Serial.println("失败 (将在 30s 后重试)");
        // 设置提前重试
        lastWeatherFetch = millis() - (WEATHER_INTERVAL_MS - RETRY_DELAY_MS);
    }
}
```

- [ ] **Step 2: 编译验证**

```bash
pio run
```
预期：编译成功，无警告。

- [ ] **Step 3: 提交**

```bash
git add src/main.cpp
git commit -m "feat: integrate all components in main.cpp"
```

---

### Task 6: 构建 & 上传验证

**文件:** 无代码变更

- [ ] **Step 1: 最终构建**

```bash
pio run
```
预期：编译成功，无警告。
实际大小应 < 固件空间（当前 ~280KB / 4MB，约 7%，绰绰有余）。

- [ ] **Step 2: 上传固件**

```bash
pio run --target upload
```

- [ ] **Step 3: 串口监视验证**

```bash
pio device monitor
```

预期输出类似：
```
===================================
  ESP8266 Weather Station v1.0
===================================
[OK] WiFi 已连接: BarryBlue
    IP 地址: 192.168.1.100
[NTP] 同步时间... OK: Tue Jul  7 10:00:00 2026
[WEATHER] 获取数据... OK: 28.5°C, 65%, ☀️ 晴
[READY] 打开浏览器访问 http://192.168.1.100
[WEATHER] 获取数据... OK: 28.7°C, 63%, ⛅ 多云
```

- [ ] **Step 4: 浏览器验证**

在浏览器中访问 ESP8266 的 IP 地址，验证：
- [ ] Dashboard 页面正常加载
- [ ] 数据卡片显示正确的温度/湿度/风速
- [ ] 温度曲线图显示历史数据
- [ ] 30秒后自动更新
- [ ] 手机端显示正常（响应式）

- [ ] **Step 5: 提交最终版本**

```bash
git push -u origin feature/weather-station-esp
```

---

## 自检清单

**1. Spec 覆盖度:**
- [x] WiFiManager 配网 → Task 5 (main.cpp)
- [x] Open-Meteo HTTPS 获取 → Task 3 (WeatherFetcher)
- [x] 环形缓冲区 → Task 2 (RingBuffer)
- [x] AsyncWebServer REST API → Task 4 (WebServer)
- [x] Dashboard HTML + Chart.js → Task 4 (DashboardHtml.h)
- [x] 30 秒前端轮询 → DashboardHtml.js 中的 `setInterval(fetchWeather, 30000)`
- [x] 10 分钟数据获取 → Task 5 (main.cpp loop)
- [x] NTP 时间同步 → Task 5 (main.cpp syncNtp)
- [x] WMO 天气码 → Task 1 (WeatherCodes.h)
- [x] MFLN 内存优化 → Task 3 (WeatherFetcher.cpp, setBufferSizes)
- [x] PROGMEM 存储 HTML → Task 4 (DashboardHtml.h)

**2. 占位符检查:** 无 TBD/TODO/fixme 占位符 ✅

**3. 类型一致性:**
- `WeatherFetcher.cpp` → `weather_fetch(WeatherRecord*)` 与 Task 2 `ring_push(WeatherRecord)` 类型一致 ✅
- `WebServer.cpp` 中 `ring_latest(&latest)` 使用 `WeatherRecord*` 类型 ✅
- `WebServer.cpp` 中 `ring_get_all(records, MAX_RECORDS)` 使用 `WeatherRecord*` 类型 ✅

**4. 范围检查:** 专注于方案 C，6 个文件，6 个任务，范围适当 ✅
