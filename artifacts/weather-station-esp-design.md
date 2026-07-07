# ESP8266 自包含天气站 — 设计文档

> **版本**: v1.0
> **日期**: 2026-07-07
> **状态**: 已批准

## 概述

方案 C：ESP8266 自包含版本。一个固件，同时做天气数据采集和 Web Dashboard 服务，无需任何外部依赖。

## 系统架构

```
ESP8266 (NodeMCU)
┌───────────────────────────────────────────────┐
│  WiFiManager                                  │
│    └── 首次启动→AP配网，后续自动连接            │
│                                                │
│  NTP Client (configTime, 每小时同步)            │
│                                                │
│  WeatherTask (每10分钟)                         │
│    ├── BearSSL::WiFiClientSecure               │
│    │   └── HTTPS GET Open-Meteo                │
│    ├── ArduinoJson 解析                        │
│    │   └── temperature_2m, humidity, wind...   │
│    └── 存入环形缓冲区 (24条)                    │
│                                                │
│  RingBuffer (内存环形缓冲区)                    │
│    └── 24条记录 × ~64B ≈ 1.5KB RAM            │
│                                                │
│  ESPAsyncWebServer (端口80)                    │
│    ├── GET / → index.html (PROGMEM)           │
│    └── GET /api/weather → JSON                │
│                                                │
│  Watchdog (硬件WDT)                            │
└───────────────────────────────────────────────┘
       │
       │ 局域网 (WiFi)
       ▼
┌───────────────────────┐
│  浏览器 Dashboard      │
│  ├── 实时数据卡片       │
│  │   ├── 🌡️ 温度      │
│  │   ├── 💧 湿度      │
│  │   ├── 🌬️ 风速/风向 │
│  │   └── ☁️ 天气描述   │
│  ├── Chart.js 温度曲线  │
│  │   └── 最近24条记录   │
│  ├── 系统状态 (WiFi/运行时间) │
│  └── 30秒自动刷新      │
└───────────────────────┘
```

## 组件设计

### 1. WiFiManager 配网

- 库: `tzapu/WiFiManager` (PlatformIO lib)
- 首次启动: 创建 AP `ESP-Weather-Station` → 浏览器配网
- 后续: 自动连接已保存网络
- 失败: 3次重试后重新进入配网模式

### 2. Open-Meteo 数据获取

**API**: `https://api.open-meteo.com/v1/forecast`

```
GET /v1/forecast?latitude=39.91&longitude=116.41
  &current=temperature_2m,relative_humidity_2m,
          apparent_temperature,weather_code,
          wind_speed_10m,wind_direction_10m
  &timezone=auto
```

**响应** (~400B):
```json
{
  "current": {
    "time": "2026-07-07T10:00",
    "temperature_2m": 28.5,
    "relative_humidity_2m": 65,
    "apparent_temperature": 30.2,
    "weather_code": 0,
    "wind_speed_10m": 12.3,
    "wind_direction_10m": 90
  }
}
```

**WMO Weather Codes** (核心映射):
| Code | 天气 | 图标 |
|------|------|------|
| 0 | 晴天 | ☀️ |
| 1-3 | 多云 | ⛅ |
| 45-48 | 雾 | 🌫️ |
| 51-57 | 毛毛雨 | 🌦️ |
| 61-67 | 雨 | 🌧️ |
| 71-77 | 雪 | ❄️ |
| 80-82 | 阵雨 | 🌧️ |
| 95-99 | 雷暴 | ⛈️ |

### 3. 环形缓冲区 (RingBuffer)

```cpp
struct WeatherRecord {
    time_t timestamp;     // 4 bytes
    float temp;           // 4 bytes
    float feels_like;     // 4 bytes
    uint8_t humidity;     // 1 byte
    float wind_speed;     // 4 bytes
    uint16_t wind_dir;    // 2 bytes
    int8_t weather_code;  // 1 byte
};  // ≈ 24 bytes + padding ≈ 32 bytes

#define MAX_RECORDS 24
WeatherRecord buffer[MAX_RECORDS];
uint8_t head = 0;
uint8_t count = 0;
```

### 4. AsyncWebServer

- 库: `me-no-dev/ESPAsyncWebServer` + `me-no-dev/AsyncTCP`
- **`GET /`**: 返回 HTML 页面（PROGMEM 字符串，~8KB）
- **`GET /api/weather`**: 返回 JSON
  ```json
  {
    "current": { "temp": 28.5, "humidity": 65, ... },
    "history": [
      { "time": "2026-07-07T09:00", "temp": 27.2, "humidity": 68 },
      ...
    ],
    "system": { "uptime": 3600, "wifi_rssi": -57, "free_heap": 28000 }
  }
  ```
- **`GET /reset`**: 重置 WiFi 配置（重新配网）

### 5. Dashboard HTML

内嵌在固件中的单页 HTML（PROGMEM）:
- Chart.js: 通过 CDN 加载
- CSS: 内联样式，移动端自适应
- JS: `fetch('/api/weather')` 每 30 秒轮询
- 显示: 数据卡片 + Chart.js 折线图

### 6. 定时调度 (非阻塞)

```
10:00:00  ── WeatherTask (HTTPS GET) ── 成功 → 存入 RingBuffer
10:00:03  ── 完成，进入睡眠状态
10:00:30  ── 浏览器 fetch /api/weather → 返回最新数据
10:10:00  ── WeatherTask 再次执行
...
```

使用 `millis()` 非阻塞调度，不阻塞 Web 服务。

## 内存预算

| 组件 | RAM 占用 | 说明 |
|------|---------|------|
| WiFi 栈 + TCP/IP | ~15KB | 系统开销 |
| BearSSL (异步时) | ~8KB | 比同步模式省 ~15KB |
| AsyncWebServer | ~4KB | 异步，无阻塞 |
| RingBuffer | ~1.5KB | 24条记录 |
| ArduinoJson | ~1KB | 临时解析 |
| 其他 + 栈 | ~10KB | 变量、库开销 |
| **总计** | **~40KB** | 可用 ~50KB，余量 ~10KB |

> **注意**: 使用 `ESPAsyncWebServer` 而非标准 `ESP8266WebServer`，因为异步 Web 服务器不阻塞主循环，且内存占用更低。但需要同时使用异步 WiFiClient 来获取 HTTPS 数据（不能用同步的 `WiFiClientSecure` + `HTTPClient`），这需要调整为 `AsyncHTTPRequest` 或改用 `WiFiClientSecure` 的同步模式但用短连接方式。

> **修正**: 实际上经评估，更稳妥的方式是使用 **ESPAsyncWebServer 跑 Web 服务** + **同步 HTTPS 短连接获取数据**，两者在主循环中协调，因为 ESP8266 的异步 HTTP 客户端生态不如同步成熟。同步 HTTPS 连接完成后立即释放，不长期占用。

## 代码结构

```
src/
├── main.cpp              # 入口，setup/loop，调度器
├── config.h              # WiFiManager 配置，API参数
├── WeatherFetcher.h/.cpp # Open-Meteo HTTPS 获取 + JSON 解析
├── RingBuffer.h/.cpp     # 环形缓冲区
├── WebServer.h/.cpp      # AsyncWebServer 路由 + API
├── DashboardHtml.h       # HTML 页面 (PROGMEM 字符串)
└── WeatherCodes.h        # WMO 天气代码 → 文字描述映射
```

## 错误处理策略

| 场景 | 处理方式 |
|------|---------|
| HTTPS 请求失败 | 等待 30 秒后重试，最多 3 次 |
| JSON 解析失败 | 丢弃本次数据，记录错误日志 |
| WiFi 断连 | 自动重连，重连后立即恢复 |
| 所有重试都失败 | 跳过本轮，等待下一个 10 分钟周期 |
| Web 服务器异常 | 不阻塞，继续提供服务 |

## 设计决策记录

| 决策 | 选择 | 理由 |
|------|------|------|
| Web Server | ESPAsyncWebServer | 非阻塞，低内存，适合并发请求 |
| HTTPS 方式 | 同步 WiFiClientSecure + HTTPClient | AsyncHTTP 在 ESP8266 上生态不成熟 |
| HTML 存放 | PROGMEM 字符串 | 不占 RAM，加载速度快 |
| 图表库 | Chart.js (CDN) | 轻量，不占 ESP8266 资源 |
| 数据刷新 | 前端 30s 轮询 | 实现简单，对 WeatherTask 无影响 |
| 位置配置 | 编译时设定 (上海) | 演示项目固定即可 |

## 后续扩展 (不做)

- 和风天气备用数据源 → 留到以后
- 方案 A (Node.js + DB + Vue) → 切 `feature/weather-station-web` 分支实现
- 远程访问 → 需公网 IP/内网穿透
