# 实时天气数据采集与展示系统 — 技术评审文档

> **版本**: v0.1 (草案)
> **日期**: 2026-07-07
> **作者**: Claude Code
> **状态**: 待评审

---

## 目录

1. [项目概述与目标](#1-项目概述与目标)
2. [可行性评估](#2-可行性评估)
3. [技术选型与对比](#3-技术选型与对比)
4. [系统架构设计](#4-系统架构设计)
5. [关键技术难点与对策](#5-关键技术难点与对策)
6. [类似项目参考](#6-类似项目参考)
7. [实施路线图](#7-实施路线图)
8. [风险评估](#8-风险评估)

---

## 1. 项目概述与目标

### 1.1 项目愿景

构建一个端到端的实时天气数据系统：ESP8266 通过 WiFi 从互联网获取实时天气数据，上传到云端数据库，前端 Web 页面定时刷新展示可视化数据面板。

### 1.2 核心目标

| 目标 | 说明 |
|------|------|
| **数据采集** | ESP8266 定时从天气 API 获取实时天气数据 |
| **数据上传** | ESP8266 通过 HTTP/REST 将数据上传至云端数据库 |
| **数据存储** | 云端数据库存储时间序列天气数据 |
| **数据展示** | Web 前端 Dashboard 定时刷新，展示实时数据与历史趋势 |
| **稳定运行** | 系统 7×24 小时稳定运行，具备断线重连机制 |

### 1.3 非目标（本次范围外）

- 本地传感器采集（DHT11/BMP280 等）— 后续可扩展
- 移动端 App — 优先 Web 端
- 告警/推送通知
- 多设备数据聚合

---

## 2. 可行性评估

### 2.1 硬件可行性 ✅

| 项目 | 评估 | 备注 |
|------|------|------|
| **WiFi 连接** | ✅ 完全可行 | ESP8266 原生 WiFi，已验证可扫描 12 个网络 |
| **HTTPS 请求** | ⚠️ 可行但需优化 | 参考 5.1 节内存管理 |
| **JSON 解析** | ⚠️ 可行但需过滤 | 需使用 ArduinoJson + 过滤功能 |
| **定时运行** | ✅ 完全可行 | 使用 millis() 非阻塞定时 |
| **可用堆内存** | ~50KB | 需精打细算，SSL 占用 ~24KB |

### 2.2 网络可行性 ✅

| 项目 | 评估 |
|------|------|
| **天气 API 在中国可访问** | ✅ 和风天气、Open-Meteo、OpenWeatherMap(cn-) 均可 |
| **云端数据库在中国可访问** | ⚠️ Supabase 可能需要代理，建议自建后端 |
| **前端页面加载** | ✅ Vercel/自建服务器均可 |

### 2.3 软件可行性 ✅

| 项目 | 评估 |
|------|------|
| **ESP8266 SDK 支持** | ✅ Arduino Core for ESP8266 功能完整 |
| **ArduinoJson 库** | ✅ 成熟稳定，支持过滤功能 |
| **HTTPClient** | ✅ 支持 HTTPS |
| **前端框架** | ✅ 技术成熟 |

**总体结论**: ⭐ **完全可行**，但 ESP8266 端需要精细的内存管理。

---

## 3. 技术选型与对比

### 3.1 天气数据源 API 对比

| 特性 | 和风天气 (QWeather) 🥇 | Open-Meteo 🥇 | OpenWeatherMap | WeatherAPI.com |
|------|----------------------|---------------|----------------|----------------|
| **免费额度** | 1000次/天 (免费版) | **无限制，完全免费** | 60次/分钟 (免费版) | 100万次/月 |
| **需要 API Key** | 是 | **否** | 是 | 是 |
| **中国访问** | ✅ 国内服务器，低延迟 | ✅ 可通过代理 | ⚠️ 使用 cn- 端点 | ⚠️ 可能被限 |
| **数据丰富度** | 实时+7天预报+AQI+生活指数 | 实时+16天预报+多种模型 | 实时+分钟级+历史 | 实时+天文+足球 |
| **API 响应大小** | ~800B-1.2KB (当前天气) | ~400B-800B (当前天气) | ~1-2KB | ~1-2KB |
| **注册复杂度** | 需注册（简单） | **无需注册** | 需注册 | 需注册 |
| **认证方式** | API Key → JWT (2027年迁移) | 无 | API Key | API Key |
| **ESP8266 库支持** | [ESP8266_qweather](https://github.com/shufengwu9511/ESP8266_qweather) | 手动 HTTP | 标准 HTTPClient | 标准 HTTPClient |

#### 推荐方案

| 场景 | 推荐 |
|------|------|
| **国内用户 + 中文数据** | **和风天气**（注册简单，国内服务器，ESP8266 有专用库） |
| **全球用户 + 零成本** | **Open-Meteo**（无需注册/API Key，数据量最小适合 ESP8266） |
| **两者兼顾** | 和风天气为主，Open-Meteo 作为备用数据源 |

### 3.2 云端数据库/后端方案对比

| 特性 | Supabase 🥇 | 自建 Node.js + SQLite/Postgres 🥇 | Thinger.io | InfluxDB + Grafana |
|------|------------|----------------------------------|------------|-------------------|
| **免费额度** | 500MB DB + 无限 API | 取决于服务器 | 2 设备 + 10 buckets | 自建免费 |
| **部署复杂度** | 中（配置 Row Level Security） | 高（需运维服务器） | **低**（拖拽仪表盘） | 中 |
| **REST API** | ✅ 原生支持 | ✅ 自建 | ✅ | ✅ (v2 API) |
| **实时推送** | ✅ WebSocket 订阅 | 需自建 (SSE/WS) | ✅ | ⚠️ 需额外配置 |
| **中国访问** | ⚠️ 可能被墙 | ✅ 自建服务器可控 | ⚠️ | ✅ 自建 |
| **数据模型** | 关系型 (PostgreSQL) | 灵活 | IoT 专用 | 时序专用 |
| **前端 Dashboard** | 需额外搭建（推荐 Retool/Appsmith） | 自建 | **内置** | **Grafana 内置** |
| **扩展性** | 强 | 强 | 有限 | 强 |
| **ESP8266 库** | [ESPSupabase](https://github.com/jhagas/ESPSupabase) | 标准 HTTPClient | 官方库支持 | HTTP POST |

#### 推荐方案

**初选方案**: **自建 Node.js/Express + SQLite 后端**（第一阶段 MVP）

| 原因 | 说明 |
|------|------|
| 🌐 中国访问无阻 | 可部署在境内 VPS 或 Railway 上 |
| 🔧 简单可控 | SQLite 无需额外数据库服务 |
| 💰 零成本起步 | Railway 免费额度足够 |
| 🚀 快速原型 | 一天内可搭建完整 CRUD |
| 📈 平滑升级 | 后续可迁移至 PostgreSQL/Supabase |

**第二阶段升级目标**: **Supabase + PostgreSQL**（如需更完善的后端能力）

### 3.3 前端技术选型对比

| 特性 | Vue 3 + ECharts 🥇 | React + Chart.js | 纯 HTML/JS + ECharts |
|------|--------------------|------------------|---------------------|
| **上手难度** | 中 | 中高 | **低** |
| **开发效率** | 高（Vite + 单文件组件） | 中 | 中（无脚手架） |
| **图表能力** | **ECharts 极强**（中国优化） | Chart.js 够用 | ECharts 强 |
| **响应式** | ✅ | ✅ | ❌ 需手写 |
| **实时更新** | ✅ setInterval + fetch | ✅ 同左 | ✅ 同左 |
| **包大小** | ~200KB (gzip ~70KB) | ~150KB (gzip ~50KB) | ~120KB (gzip ~40KB) |
| **中国 CDN** | ✅ 阿里/腾讯 CDN | ✅ | ✅ |

#### 推荐方案

**初选方案**: **Vue 3 + ECharts**（Vite 构建）

| 原因 | 说明 |
|------|------|
| 🏆 ECharts 对中文/中国用户优化最好 | 地图、时间轴、主题支持完善 |
| ⚡ Vite 开发体验极佳 | HMR 秒级更新 |
| 🧩 Vue 3 Composition API 适合数据可视化 | 逻辑复用清晰 |
| 📦 可通过 CDN 引入避免打包过大 | 灵活 |

### 3.4 整体技术栈总结

```
┌─────────────────────────────────────────────────────────┐
│                    推荐技术栈 (MVP)                       │
├──────────────┬──────────────────┬───────────────────────┤
│   层级        │     技术选择      │       备注            │
├──────────────┼──────────────────┼───────────────────────┤
│ 硬件端        │ ESP8266 (NodeMCU)│ 现有硬件, Arduino框架  │
│ 天气数据源    │ 和风天气/Open-Meteo│ 双重数据源备选         │
│ 后端服务      │ Node.js/Express  │ JavaScript全栈统一     │
│ 数据库        │ SQLite (MVP)     │ → PostgreSQL (二期)    │
│ 部署平台      │ Railway/VPS      │ 零成本起步             │
│ 前端框架      │ Vue 3 + ECharts  │ Vite 构建             │
│ 前端部署      │ Vercel/GitHub Pages│ 静态站点, CDN加速      │
│ 通信协议      │ HTTP REST + JSON  │ 简单可靠              │
│ 更新方式      │ 定时轮询 (polling) │ 前端30s, ESP8266 10min │
└──────────────┴──────────────────┴───────────────────────┘
```

---

## 4. 系统架构设计

### 4.1 整体架构图

```
┌─────────────┐     HTTPS      ┌───────────────┐
│             │ ──────────────> │               │
│  ESP8266    │     GET天气数据   │  天气 API      │
│  (数据采集)  │ <────────────── │  (和风/Open-Meteo)│
│             │     JSON响应     │               │
└─────────────┘                 └───────────────┘
       │
       │ HTTP POST (JSON)
       │ {temp, humidity, wind, timestamp}
       ▼
┌─────────────────┐
│                  │
│  Node.js 后端    │──── POST /api/weather ──>  SQLite/PostgreSQL
│  (Express)       │<─── SELECT ─────────────
│                  │
└────────┬────────┘
         │
         │ GET /api/weather (REST JSON)
         ▼
┌─────────────────┐
│                  │
│  Vue 3 + ECharts │──── 30s 轮询 ────> 实时展示
│  Dashboard       │
│                  │
└─────────────────┘
```

### 4.2 数据流时序

```
ESP8266                     Node.js Backend              Frontend
   │                             │                          │
   │  1. WiFi 连接               │                          │
   │─────────────────────────────│                          │
   │                             │                          │
   │  2. GET 天气 API (10min周期) │                          │
   │──── HTTPS ────> 和风天气      │                          │
   │<─── JSON ───────────        │                          │
   │                             │                          │
   │  3. POST /api/weather       │                          │
   │──── HTTP ──────────────────>│                          │
   │   {temp, humidity, wind...} │  4. INSERT INTO weather   │
   │                             │──── SQL ──────> Database  │
   │<─── 201 Created ───────────│                          │
   │                             │                          │
   │                             │  5. GET /api/weather     │
   │                             │<─── fetch (30s轮询) ─────│
   │                             │─── JSON ────────────────>│
   │                             │                          │
   │                             │  6. ECharts 渲染         │
   │                             │    (温度曲线/湿度/风力)    │
```

### 4.3 ESP8266 端架构

```
┌─────────────────────────────────────┐
│        ESP8266 数据采集固件           │
├─────────────────────────────────────┤
│                                     │
│  WiFiManager ──> 自动连接/重连        │
│                                     │
│  WeatherFetcher (10min定时)          │
│  ├── HTTPS GET → 天气 API           │
│  ├── ArduinoJson → 解析             │
│  └── 提取字段: temp/humidity/wind   │
│                                     │
│  DataUploader (解析后立即上传)         │
│  ├── HTTP POST → 后端 /api/weather  │
│  └── 重试机制 (失败后3次重试)          │
│                                     │
│  Watchdog (看门狗)                   │
│  └── ESP.wdtFeed() + 硬件WDT        │
│                                     │
│  NTPClient (时间同步)                │
│  └── 为数据添加时间戳                 │
│                                     │
└─────────────────────────────────────┘
```

### 4.4 数据库 Schema (第一阶段 SQLite)

```sql
-- 天气数据表
CREATE TABLE weather_records (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   TEXT    NOT NULL DEFAULT (datetime('now')),
    temp        REAL,              -- 温度 (°C)
    feels_like  REAL,              -- 体感温度 (°C)
    humidity    INTEGER,           -- 湿度 (%)
    pressure    INTEGER,           -- 气压 (hPa)
    wind_speed  REAL,              -- 风速 (km/h)
    wind_dir    TEXT,              -- 风向
    weather_text TEXT,             -- 天气描述 (晴/多云/雨)
    weather_code INTEGER,          -- 天气代码
    visibility  INTEGER,           -- 能见度 (km)
    cloud       INTEGER,           -- 云量 (%)
    source      TEXT DEFAULT 'qweather'  -- 数据来源
);

-- 索引
CREATE INDEX idx_weather_timestamp ON weather_records(timestamp DESC);
CREATE INDEX idx_weather_date ON weather_records(date(timestamp));

-- 系统状态表
CREATE TABLE system_status (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   TEXT    NOT NULL DEFAULT (datetime('now')),
    esp_chip_id TEXT,
    free_heap   INTEGER,
    uptime      INTEGER,           -- 运行秒数
    wifi_rssi   INTEGER,           -- WiFi信号强度
    last_success INTEGER            -- 上次成功获取时间戳
);
```

### 4.5 REST API 设计

| 方法 | 路径 | 说明 | 频率限制 |
|------|------|------|---------|
| `POST` | `/api/weather` | ESP8266 上传天气数据 | 每设备每10分钟 |
| `GET` | `/api/weather/latest` | 获取最新一条数据 | 30次/分钟 |
| `GET` | `/api/weather/history?hours=24` | 获取历史数据 | 30次/分钟 |
| `GET` | `/api/weather/daily?days=7` | 获取每日汇总 | 30次/分钟 |
| `GET` | `/api/status` | 获取ESP8266系统状态 | 30次/分钟 |
| `POST` | `/api/status` | ESP8266 上传状态 | 每设备每30分钟 |

### 4.6 前端 Dashboard 设计

```
┌──────────────────────────────────────────────┐
│  🌤️ 实时天气监控面板    最后更新: 10:32:45     │
├──────────────────────────────────────────────┤
│ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│ │  🌡️ 温度  │ │  💧 湿度  │ │  🌬️ 风速  │       │
│ │  28.5°C  │ │  65%    │ │  12km/h  │       │
│ │  体感 30°C│ │          │ │  东风3级  │       │
│ └──────────┘ └──────────┘ └──────────┘       │
│ ┌──────────────────────────────────────┐      │
│ │   📈 24小时温度变化曲线 (ECharts)       │      │
│ │   ┌────────────────────────────────┐ │      │
│ │   │  ▁▃▆██▇▆▅▃▂▁▃▅▇██▇▆▅▄▃▂▁     │ │      │
│ │   └────────────────────────────────┘ │      │
│ └──────────────────────────────────────┘      │
│ ┌──────────────────────────────────────┐      │
│ │   📊 7天温度/湿度对比                  │      │
│ └──────────────────────────────────────┘      │
│ ┌──────────┐ ┌──────────────────────┐         │
│ │ 系统状态   │ │ 数据日志表格            │         │
│ │ WiFi: -57 │ │ 时间   温度  湿度  天气  │         │
│ │ 内存: 45KB│ │ 10:30  28.5  65%  晴  │         │
│ │ 运行: 3d  │ │ 10:20  28.3  66%  晴  │         │
│ └──────────┘ └──────────────────────┘         │
├──────────────────────────────────────────────┤
│  数据来源: 和风天气 | ESP8266 最后上报: 10:30   │
└──────────────────────────────────────────────┘
```

---

## 5. 关键技术难点与对策

### 5.1 ESP8266 内存管理（最大挑战）

#### 问题

ESP8266 可用堆内存约 **50KB**，而 SSL/TLS (BearSSL) 连接占用约 **22-28KB**：

| 组件 | 内存占用 |
|------|---------|
| BearSSL 辅助栈 | ~5.6-7KB |
| BearSSL 接收缓冲区 | 16KB (默认) |
| BearSSL 发送缓冲区 | ~512B |
| **SSL 连接总计** | **~22-28KB** |
| HTTP 响应 JSON 解析 | ~1-3KB (需过滤) |
| WiFi 栈开销 | ~5-10KB |
| **合计占用** | **~35-45KB** |

#### 对策

| 策略 | 说明 | 效果 |
|------|------|------|
| **全局 WiFiClientSecure** | 不要每次请求创建，在全局/ setup() 中创建一次 | 避免碎片化 |
| **setInsecure()** | 跳证书验证（天气数据可接受） | 节省 CA 证书 RAM |
| **MFLN (最大分段长度协商)** | 接收缓冲区从 16KB 降至 512B-1KB | 节省 ~15KB |
| **JsonDocument 过滤** | 只解析需要的字段 | 节省 50-80% JSON RAM |
| **避免 String 类型** | 使用 `getStream()` 直接解析 | 避免双倍内存 |
| **避免动态内存分配** | 全局分配固定缓冲区 | 防止碎片累积 |
| **定期检查堆空间** | `ESP.getFreeHeap()` 调试 | 预警 |

#### MFLN 优化代码示例

```cpp
BearSSL::WiFiClientSecure client;
BearSSL::Session session;

void setup() {
    client.setSession(&session);
    client.setBufferSizes(1024, 512);  // 发送缓冲 1KB, 接收 512B
    client.setInsecure();  // 或使用 setFingerprint()
}

void loop() {
    // ...HTTP请求...
    http.begin(client, url);
}
```

### 5.2 HTTPS 连接稳定性

#### 问题

- ESP8266 SSL 握手慢（可能 5-10 秒）
- 某些服务器不支持 MFLN
- 证书验证可能导致连接失败

#### 对策

1. **双重验证策略**: 先用 `setInsecure()` 开发，再用 `setFingerprint()` 加固
2. **CPU 频率提升**: `ESP.setCpuFrequencyMhz(160)` 加速 SSL 握手（默认 80MHz）
3. **SSL Session 复用**: `client.setSession(&session)` 减少后续握手开销
4. **重试机制**: 失败后延迟 30s 重试，最多 3 次
5. **超时设置**: `client.setTimeout(10000)` 防止死等

### 5.3 网络断线重连

#### 问题

- 中国网络环境下 WiFi/HTTP 可能不定期中断
- 长时间运行后 WiFi 连接可能自动断开

#### 对策

```cpp
void ensureWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi 断开，重连中...");
        WiFi.reconnect();
        int retries = 20;
        while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
            delay(500);
        }
    }
}
```

### 5.4 数据完整性

#### 问题

- HTTP 请求可能返回不完整数据
- 时间戳需准确（NTP 同步）

#### 对策

1. NTP 同步获取准确时间（`configTime(8*3600, 0, "ntp.aliyun.com")`）
2. HTTP 响应码检查（仅处理 200/201）
3. JSON 解析失败时丢弃数据，不写入数据库
4. 后端 API Key 认证防止未授权写入

### 5.5 前端实时更新

#### 问题

- 传统 Web 轮询不够"实时"
- WebSocket 对简单场景过重

#### 对策

- **MVP 方案**: 前端 30 秒轮询 REST API（足够简单可靠）
- **未来升级**: Node.js 后端增加 SSE (Server-Sent Events) 推送
- 使用 ECharts 的 `appendData` 实现平滑动画过渡

---

## 6. 类似项目参考

### 6.1 国际项目对比

| 项目 | Stars | 架构 | 天气源 | 亮点 | 可借鉴 |
|------|-------|------|--------|------|--------|
| [ThingPulse/esp8266-weather-station](https://github.com/ThingPulse/esp8266-weather-station) | 1.1k | ESP8266 → OLED 直显 | OpenWeatherMap/Aeris | 最成熟的 ESP8266 天气站，模块化库 | SSL 处理、NTP 同步、SunMoonCalc |
| [AlexeyMal/esp8266-weather-station](https://github.com/AlexeyMal/esp8266-weather-station) | 活跃 | ESP8266 → OLED | **Open-Meteo** (免费) | 已从 OpenWeatherMap 迁移到免费 API | Open-Meteo 集成经验 |
| [mihir-robotics/nodemcu-weather-app](https://github.com/mihir-robotics/nodemcu-weather-app) | — | ESP8266 → Flask → MongoDB → Web | DHT11 本地 | 完整全栈架构，前后端分离 | Flask 后端设计模式 |
| [klaasnicolaas/project-sensortastic](https://github.com/klaasnicolaas/project-sensortastic) | — | ESP8266 → InfluxDB → Grafana | DHT22 本地 | Docker Compose 部署，专业仪表盘 | InfluxDB + Grafana 参考 |
| [karman-bhatti/Esp8266dashboardV2](https://github.com/karman-bhatti/Esp8266dashboardV2) | — | **ESP8266 自建 Web 服务器** | OpenWeatherMap | 多功能面板（天气+股票+3D打印） | ESPAsyncWebServer 用法 |

### 6.2 中文项目对比

| 项目 | 架构 | 天气源 | 亮点 | 可借鉴 |
|------|------|--------|------|--------|
| [ESP8266_qweather](https://github.com/shufengwu9511/ESP8266_qweather) (库) | ESP8266 专用库 | 和风天气 | 封装 HTTPS+JSON，几行代码可用 | 直接采用该库简化开发 |
| [goldhan/GDWeatherStation](https://github.com/goldhan/GDWeatherStation) | ESP8266 → Python 中转 → 和风天气 | 和风天气 | Python 中转减轻 ESP8266 负担 | 未来可增加中转层 |
| [ESP8266桌面气象站(CSDN系列)](https://blog.csdn.net/zhb1190/article/details/121970105) | ESP8266 → OLED | 和风天气 | 三步教程，中文资料齐全 | UI 设计、图标映射参考 |

### 6.3 差异化方向（本项目创新点）

相比现有项目，本方案的独特之处：

| 对比维度 | 多数现有项目 | 本项目 |
|----------|------------|--------|
| **数据持久化** | ❌ 仅 OLED 显示，不存历史 | ✅ 云端数据库全量存储 |
| **远程访问** | ❌ 需靠近设备看屏幕 | ✅ Web Dashboard 随时随地 |
| **历史趋势** | ❌ 只显示当前值 | ✅ 24h/7d 趋势图表 |
| **前端技术** | 嵌入式 HTML (简陋) | ✅ Vue 3 + ECharts 专业面板 |
| **系统监控** | ❌ 无 | ✅ ESP8266 状态远程监控 |
| **多数据源** | 单一 | ✅ 主备双 API 切换 |

---

## 7. 实施路线图

### 阶段一：ESP8266 天气数据采集（1-2天）

```
[ ] 和风天气 API 注册 + 获取 Key
[ ] ESP8266 WiFi 连接管理（配置 SSID/密码）
[ ] HTTPS 请求和风天气 API → 获取 JSON
[ ] ArduinoJson 解析 → 提取温度/湿度/风力
[ ] NTP 时间同步
[ ] 串口输出验证数据正确性
```

### 阶段二：后端 API + 数据库（1-2天）

```
[ ] Node.js/Express 项目搭建
[ ] SQLite 数据库 Schema 创建
[ ] POST /api/weather 端点（ESP8266 写入）
[ ] GET /api/weather/latest 端点
[ ] GET /api/weather/history 端点（时间范围查询）
[ ] API Key 简单认证
[ ] Railway/VPS 部署
```

### 阶段三：ESP8266 → 后端联调（1天）

```
[ ] ESP8266 HTTP POST 数据到后端
[ ] 数据库写入验证
[ ] 错误处理和重试逻辑
[ ] 定时采集（10分钟间隔）实现
[ ] 看门狗 + 断线重连
[ ] 长期稳定性测试（24h+）
```

### 阶段四：前端 Dashboard（1-2天）

```
[ ] Vue 3 + Vite 项目脚手架
[ ] ECharts 集成
[ ] 实时数据卡片（温度/湿度/风力）
[ ] 24h 温度变化曲线图
[ ] 7 天历史趋势图
[ ] 系统状态面板
[ ] 30 秒自动刷新
[ ] Vercel/GitHub Pages 部署
```

### 阶段五：集成测试与优化（1天）

```
[ ] 端到端全链路测试
[ ] ESP8266 内存优化（MFLN/JsonDocument 过滤）
[ ] 前端响应式适配（移动端/PC）
[ ] 错误页面和加载状态处理
[ ] README 和部署文档
```

**总计工期**: **5-7 天**

---

## 8. 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| ESP8266 SSL 内存不足 | 🟡 中 | 🟠 高—无法获取数据 | MFLN 优化、setInsecure()、CPU 160MHz |
| 天气 API 在中国不稳定 | 🟡 中 | 🟡 中—数据缺失 | 双数据源切换、本地缓存 |
| 和风天气 API 认证迁移 (2027) | 🟢 低 | 🟡 中—需修改代码 | 提前准备 JWT 认证支持 |
| 后端部署平台变更 | 🟢 低 | 🟡 中—服务中断 | 容器化便于迁移 |
| WiFi 长时间运行断连 | 🟡 中 | 🟡 中—数据断档 | 自动重连 + 硬件看门狗 |
| ESP8266 Flash 磨损 | 🟢 低 | 🟢 低—设备报废 | 减少 EEPROM 写入，仅 RAM 操作 |

---

## 附录

### A. 和风天气 API 参考

```bash
# 1. 城市查询
GET https://geoapi.qweather.com/v2/city/lookup?location=城市名&key=YOUR_KEY

# 2. 实时天气 (免费版)
GET https://devapi.qweather.com/v7/weather/now?location=城市ID&key=YOUR_KEY

# 3. 3天预报
GET https://devapi.qweather.com/v7/weather/3d?location=城市ID&key=YOUR_KEY
```

### B. Open-Meteo API 参考（免 Key）

```bash
# 当前天气（指定城市坐标）
GET https://api.open-meteo.com/v1/forecast?latitude=39.91&longitude=116.41&current=temperature_2m,relative_humidity_2m,apparent_temperature,wind_speed_10m,weather_code
```

### C. 硬件资源清单（已有）

| 组件 | 型号 | 状态 |
|------|------|------|
| 开发板 | NodeMCU 1.0 (ESP-12E) | ✅ 已验证 |
| USB 线 | Micro USB | ✅ |
| WiFi 环境 | 2.4GHz 已连接 | ✅ |

### D. 开发环境（已有）

| 工具 | 版本 | 状态 |
|------|------|------|
| PlatformIO | 6.1.19 | ✅ |
| Arduino Core | 3.1.2 | ✅ |
| Git + GitHub | — | ✅ |
| Node.js | — | ⬜ 需安装 |

---

> **下一步**: 评审通过后，创建 `feature/weather-station` 开发分支，按照路线图分阶段实施。
