# Weather Station Web — 方案 A 设计文档

> **版本**: v0.1
> **日期**: 2026-07-07
> **分支**: `feature/weather-station-web`
> **依赖**: ESP8266 固件（方案 C — `feature/weather-station-esp`）

---

## 1. 项目概述

### 1.1 目标

ESP8266 作为天气数据采集传感器，从 Open-Meteo 获取实时天气，上传到 Node.js 后端持久化存储，前端 Vue 3 + ECharts Dashboard 展示。

### 1.2 与方案 C 的关系

| | 方案 C (已完成) | 方案 A (本项目) |
|---|---|---|
| ESP8266 角色 | 全功能一体机（采集+存储+Web服务） | 纯传感器（采集+上传） |
| 存储 | 内存 Ring Buffer（断电丢失） | SQLite 持久化 |
| 前端 | 嵌入式 Chart.js | Vue 3 + ECharts |
| 扩展性 | 单设备 | 多传感器注册 + fallback 数据源 |

---

## 2. 系统架构

```
                         ┌──────────────────┐
                         │   Open-Meteo API  │
                         │   (免费天气数据)    │
                         └────────┬─────────┘
                                  │ HTTPS GET
                                  ▼
┌──────────┐  register+token  ┌──────────┐  POST /api/sensor/data  ┌──────────────┐
│ ESP8266  │ ◀──────────────▶ │ Node.js  │ ◀────────────────────── │   SQLite     │
│ 传感器    │                  │ Express  │                          │  (better-    │
│          │                  │ 后端服务  │ ────────────────────────▶│  sqlite3)    │
└──────────┘                  └────┬─────┘                          └──────────────┘
                                   │
                                   │ REST API (JSON)
                                   ▼
                          ┌─────────────────┐
                          │  Vue 3 + ECharts│
                          │  + Pinia        │
                          │  Dashboard      │
                          │  Tab: 概览|历史|设置│
                          └─────────────────┘
```

### 2.2 数据流

```
1. ESP8266 启动 → POST /api/sensor/register → 获取 token
2. ESP8266 每 10min → HTTPS GET Open-Meteo → POST /api/sensor/data (带 token)
3. Node.js 接收数据 → 写入 SQLite → 标记 source = "sensor:<device_id>"
4. 后端检测 ESP8266 > 30min 无上报 → 自动 fallback:
   后端自己拉 Open-Meteo → 写入 SQLite → 标记 source = "backend"
5. 前端每 30s 轮询 GET /api/weather/current + 手动切换数据源
```

---

## 3. 目录结构

```
test_esp8266/
├── esp8266/                     # 方案 C 固件（不动）
│   ├── src/
│   │   ├── main.cpp
│   │   ├── WeatherFetcher.cpp   # 改造：POST 到后端而非本地存储
│   │   └── ...
│   └── platformio.ini
├── backend/                     # Node.js + Express + SQLite (新增)
│   ├── src/
│   │   ├── index.js             # Express 入口
│   │   ├── routes/
│   │   │   ├── sensor.js        # 传感器注册 + 数据上报
│   │   │   ├── weather.js       # 天气查询 API
│   │   │   └── source.js        # 数据源切换 API
│   │   ├── services/
│   │   │   ├── weatherFetcher.js # 后端主动拉取天气
│   │   │   ├── fallbackMonitor.js # 离线检测与自动切换
│   │   │   └── sourceTracker.js  # 数据源状态管理
│   │   ├── db/
│   │   │   ├── schema.js        # SQLite 建表语句
│   │   │   └── queries.js       # 数据查询封装
│   │   └── middleware/
│   │       └── auth.js          # Token 认证中间件
│   ├── tests/
│   │   ├── sensor.test.js
│   │   ├── weather.test.js
│   │   └── fallback.test.js
│   └── package.json
├── frontend/                    # Vue 3 + ECharts (新增)
│   ├── src/
│   │   ├── App.vue
│   │   ├── main.js
│   │   ├── router/
│   │   │   └── index.js         # /dashboard, /history, /settings
│   │   ├── stores/
│   │   │   └── weather.js       # Pinia store
│   │   ├── views/
│   │   │   ├── Dashboard.vue     # 概览仪表盘
│   │   │   ├── History.vue       # 历史数据查询
│   │   │   └── Settings.vue      # 系统设置
│   │   ├── components/
│   │   │   ├── WeatherCard.vue   # 数据卡片
│   │   │   ├── TempChart.vue     # ECharts 温度趋势
│   │   │   ├── SourceBadge.vue   # 数据来源标签
│   │   │   └── SourceSwitch.vue  # 数据源切换开关
│   │   └── stores/
│   │       └── weather.js
│   └── package.json
└── artifacts/                   # 设计文档
    ├── weather-station-tech-review.md
    ├── weather-station-esp-design.md
    └── weather-station-web-design.md  ← 本文档
```

---

## 4. 数据库设计

### 4.1 表结构

```sql
-- 传感器注册表
CREATE TABLE sensors (
    id          TEXT PRIMARY KEY,        -- 设备唯一 ID (MAC 地址)
    name        TEXT NOT NULL,           -- 设备名称
    token       TEXT NOT NULL UNIQUE,    -- 认证 token
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen   TEXT                     -- 最后上报时间
);

-- 天气数据表
CREATE TABLE weather_data (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    temp        REAL NOT NULL,           -- 温度 °C
    feels_like  REAL,                    -- 体感温度 °C
    humidity    INTEGER,                 -- 湿度 %
    wind_speed  REAL,                    -- 风速 km/h
    wind_dir    INTEGER,                 -- 风向 (度)
    weather_code INTEGER,               -- WMO weather code
    source      TEXT NOT NULL,           -- "sensor:<device_id>" | "backend"
    recorded_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- 数据源状态表
CREATE TABLE source_state (
    id          INTEGER PRIMARY KEY CHECK (id = 1),  -- 单行
    mode        TEXT NOT NULL DEFAULT 'auto',         -- 'auto' | 'sensor:<id>' | 'backend'
    fallback_min INTEGER NOT NULL DEFAULT 30,         -- 离线判定阈值 (分钟)
    updated_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_weather_data_recorded_at ON weather_data(recorded_at);
CREATE INDEX idx_weather_data_source ON weather_data(source);
```

### 4.2 数据来源标识

| source 值 | 含义 | 前端显示 |
|-----------|------|---------|
| `sensor:esp8266-01` | ESP8266 传感器上报 | 🟢 传感器 esp8266-01 |
| `backend` | 后端服务自行拉取 | 🔵 后端服务 |
| `sensor:其他设备ID` | 其他注册传感器 | 🟢 传感器 {name} |

---

## 5. API 设计

### 5.1 传感器端点

| 方法 | 路径 | 认证 | 说明 |
|------|------|------|------|
| POST | `/api/sensor/register` | 无 | 设备注册，返回 token |
| POST | `/api/sensor/data` | Bearer token | 上传天气数据 |

**POST /api/sensor/register**

```json
// Request
{ "id": "esp8266-01", "name": "客厅传感器" }

// Response 200
{ "token": "sk-xxxx-xxxx-xxxx", "message": "注册成功" }
```

**POST /api/sensor/data**

```json
// Request (Header: Authorization: Bearer sk-xxxx)
{
  "temp": 33.3,
  "feels_like": 39.9,
  "humidity": 65,
  "wind_speed": 12.4,
  "wind_dir": 170,
  "weather_code": 3
}

// Response 200
{ "message": "ok", "id": 42 }
```

### 5.2 天气查询端点

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/weather/current` | 当前最新天气 + 来源 |
| GET | `/api/weather/history?from=&to=&page=&limit=` | 历史分页查询 |

**GET /api/weather/current**

```json
{
  "data": {
    "temp": 33.3, "feels_like": 39.9, "humidity": 65,
    "wind_speed": 12.4, "wind_dir": 170, "weather_code": 3,
    "recorded_at": "2026-07-07T13:10:00Z"
  },
  "source": {
    "type": "sensor",
    "label": "客厅传感器",
    "device_id": "esp8266-01",
    "online": true
  }
}
```

### 5.3 数据源控制端点

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/source/state` | 获取当前数据源状态 |
| POST | `/api/source/switch` | 手动切换数据源 |
| PATCH | `/api/source/config` | 更新 fallback 配置 |

**POST /api/source/switch**

```json
// Request
{ "mode": "sensor:esp8266-01" }
// 可选值: "auto" | "sensor:<id>" | "backend"

// Response 200
{ "mode": "sensor:esp8266-01", "message": "已切换数据源" }
```

---

## 6. 前端设计

### 6.1 路由

| 路径 | 组件 | 说明 |
|------|------|------|
| `/` | Dashboard.vue | 概览仪表盘 |
| `/history` | History.vue | 历史数据查询 |
| `/settings` | Settings.vue | 系统设置 |

### 6.2 Tab 1: Dashboard.vue

```
┌─────────────────────────────────────────┐
│  ☀️ Weather Station                     │
│  数据源: 🟢 客厅传感器  [切换▼]          │
├──────────┬──────────┬──────────┬────────┤
│ 🌡️ 温度   │ 💧 湿度   │ 💨 风速   │ ☁️ 天气  │
│ 33.3°C   │   65%    │ 12.4km/h │  阴     │
│ 体感39.9° │          │ 风向 170° │         │
├──────────┴──────────┴──────────┴────────┤
│  📈 温度趋势 (ECharts)                   │
│  ┌──────────────────────────────────┐   │
│  │  ▁▂▃▄▅▆▇█▇▆▅▄▃▂▁               │   │
│  │  12:00  14:00  16:00  18:00     │   │
│  └──────────────────────────────────┘   │
├─────────────────────────────────────────┤
│  📈 湿度趋势 (ECharts)                   │
│  ┌──────────────────────────────────┐   │
│  │  ▁▃▅▇▇▅▃▁▁▂▃▄▅▆               │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

### 6.3 Tab 2: History.vue

- 日期范围选择器（日期从 - 到）
- 数据表格：时间、温度、湿度、风速、数据来源
- ECharts 汇总曲线（温度 + 湿度双 Y 轴）
- 导出 CSV 按钮

### 6.4 Tab 3: Settings.vue

- **数据源设置**
  - 模式选择：自动 / 仅传感器 / 仅后端
  - Fallback 阈值：离线 N 分钟后自动切换（默认 30）
  - 当前活跃数据源 + 状态指示灯
- **传感器管理**
  - 已注册传感器列表：名称、ID、最后上报时间、在线状态
  - 删除传感器按钮

### 6.5 数据来源标识组件 (SourceBadge)

```
🟢 客厅传感器     — 在线，数据来自该传感器
🟡 客厅传感器     — 离线中（超过阈值）
🔵 后端服务       — 后端自己拉取的数据
```

---

## 7. Fallback 机制

### 7.1 状态机

```
                    ┌─────────┐
        传感器上报    │  SENSOR  │
     ┌─────────────▶│  (传感器) │◀──────────┐
     │              └────┬─────┘           │
     │                   │                  │
     │       超过阈值无上报  │                  │
     │                   ▼                  │
     │              ┌─────────┐    手动切换   │
     │              │ BACKEND │─────────────┘
     │              │ (后端)   │
     │              └────┬─────┘
     │                   │
     │       传感器恢复上报 │
     │                   │
     │              ┌────▼────┐
     └──────────────│  SENSOR  │
                    └──────────┘
```

### 7.2 自动检测逻辑 (fallbackMonitor.js)

```
每 60 秒检查:
  if mode == 'auto':
    if 当前活跃传感器 last_seen > fallback_min 分钟:
      if 当前 source != 'backend':
        后端主动拉取 Open-Meteo
        写入 weather_data (source='backend')
        广播 source 变更事件
    else if 传感器恢复 && 当前 source == 'backend':
      切回 sensor (下次上报自动生效)

  if mode == 'sensor:<id>':
    只用指定传感器，不 fallback（即使离线也不切）

  if mode == 'backend':
    总是后端拉取，忽略所有传感器
```

---

## 8. ESP8266 固件改造（基于方案 C）

方案 C 的 ESP8266 固件需要少量改造：

| 改动 | 说明 |
|------|------|
| 保留 WiFiManager 配网 | 不变 |
| 保留 Open-Meteo HTTPS 拉取 | 不变 |
| **新增** 设备注册 | 启动时 POST /api/sensor/register，保存 token |
| **新增** 数据上报 | 获取天气后 POST /api/sensor/data，而非存本地 ring buffer |
| **移除** WebServer/mDNS | ESP8266 不再需要提供 Web 服务 |
| **移除** Dashboard HTML | 不嵌入网页 |

### 8.1 ESP8266 数据上报流程

```
setup():
  1. WiFiManager 配网
  2. POST /api/sensor/register → 获取 token
  3. NTP 时间同步
  4. 首次天气采集 + 上报

loop():
  每 10min:
    HTTPS GET Open-Meteo 天气
    POST /api/sensor/data (Bearer token)
  每 30min (失败时):
    重试注册 (token 过期或丢失)
```

---

## 9. 技术选型

| 组件 | 选择 | 依据 |
|------|------|------|
| 后端运行时 | Node.js LTS | tech-strategy.md 黄金路径 |
| 后端框架 | Express | 轻量，生态成熟 |
| 数据库 | better-sqlite3 | 同步 API 简单可靠，零配置 |
| 前端框架 | Vue 3 + Vite | Pinia 状态管理 + Vue Router |
| 图表库 | ECharts 5 | 方案评审时用户选定 |
| 测试框架 | Vitest (前端) + Node test runner (后端) | tech-strategy.md 一致 |
| 包管理器 | pnpm | tech-strategy.md 黄金路径 |

### 9.1 前端运行时选择

**Tinify** — Vue 3 + Vite 的项目初始化工具。

---

## 10. 非功能需求

- **数据保留**：天气数据保留 90 天，超期自动清理
- **API 限流**：传感器端点 10 req/min，查询端点 60 req/min
- **错误处理**：所有 API 返回统一 `{ error: string }` 格式
- **日志**：后端使用结构化日志（pino），包含请求 ID 追踪
