# Weather Station — ESP8266 天气站

基于 ESP8266 的实时天气数据采集与展示系统，提供两种部署方案。

[![PlatformIO](https://img.shields.io/badge/PlatformIO-6.1+-orange)](https://platformio.org)
[![Node.js](https://img.shields.io/badge/Node.js-24+-green)](https://nodejs.org)
[![Vue](https://img.shields.io/badge/Vue-3.5-4fc08d)](https://vuejs.org)

---

## 两种方案

| | 方案 C：一体机 | 方案 A：全栈传感器 |
|---|---|---|
| 分支 | `feature/weather-station-esp` | `feature/weather-station-web` |
| ESP8266 角色 | 完整 Web 服务器 | 数据采集传感器 |
| 存储 | 内存 Ring Buffer（断电丢失） | SQLite 持久化 |
| 前端 | 嵌入式 Chart.js | Vue 3 + ECharts |
| 多传感器 | ❌ | ✅ 设备注册 + token 认证 |
| Fallback | ❌ | ✅ 传感器离线 → 后端自动接管 |

---

## 方案 A：快速启动

### 前置条件

- **Node.js** ≥ 24（LTS）
- **pnpm** ≥ 11：`npm install -g pnpm`
- **PlatformIO**（仅 ESP8266 固件需要）：`pip install platformio`

### 一键启动后端 + 前端

```bash
# 1. 安装依赖（首次）
pnpm install
cd backend && pnpm install && cd ..
cd frontend && pnpm install && cd ..

# 2. 启动全部服务
pnpm dev
```

| 服务 | 地址 | 说明 |
|------|------|------|
| 后端 API | `http://localhost:3001` | Express + SQLite |
| 前端 Dashboard | `http://localhost:5173` | Vue 3 + ECharts |

### 验证后端

```bash
curl http://localhost:3001/api/health
# → {"status":"ok","ts":"..."}
```

### ESP8266 传感器（可选）

```bash
cd esp8266

# 1. 修改 platformio.ini 中的 BACKEND_HOST 为你的 PC IP
#    build_flags = -DBACKEND_HOST=\"192.168.3.11\"

# 2. 编译 + 烧录
pio run -t upload

# 3. 查看运行日志
pio device monitor --baud 115200
```

ESP8266 首次启动会自动进入配网模式（AP: `ESP-Weather-Station` / 密码: `weather123`），连接后通过 `192.168.4.1` 配置 WiFi。

### 关闭服务

```bash
# 关闭后端（Ctrl+C 或）
kill $(lsof -t -i:3001)

# 关闭前端
kill $(lsof -t -i:5173)
```

---

## 方案 C：快速启动

> 分支：`feature/weather-station-esp`

```bash
git checkout feature/weather-station-esp

# 编译 + 烧录
pio run -t upload

# 查看运行日志
pio device monitor --baud 115200
```

ESP8266 一体运行：WiFiManager 配网 → Open-Meteo 获取天气 → 内嵌 Web Dashboard。

访问：`http://esp-weather.local` 或 ESP8266 的 IP 地址。

---

## 方案 A：API 端点一览

### 传感器端点

| 方法 | 路径 | 说明 |
|------|------|------|
| `POST` | `/api/sensor/register` | 设备注册 `{id, name}` → `{token}` |
| `POST` | `/api/sensor/data` | 上报天气数据（需 Bearer token） |

### 天气查询

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/weather/current` | 最新天气 + 数据来源 |
| `GET` | `/api/weather/history?from=&to=&page=&limit=` | 历史分页查询 |

### 数据源控制

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/source/state` | 数据源状态 + 传感器列表 |
| `POST` | `/api/source/switch` | 切换数据源 `{mode: "auto"|"backend"|"sensor:<id>"}` |
| `PATCH` | `/api/source/config` | 修改 fallback 阈值 `{fallback_min}` |

### 健康检查

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/health` | 服务状态 |

---

## 项目结构

```
test_esp8266/
├── esp8266/                     # 方案 A 的 ESP8266 传感器固件
│   ├── src/
│   │   ├── main.cpp             # 传感器主程序
│   │   └── config.h             # 编译期配置
│   └── platformio.ini           # PlatformIO 构建配置
├── backend/                     # Node.js + Express + SQLite
│   ├── src/
│   │   ├── index.js             # Express 入口
│   │   ├── routes/              # API 路由
│   │   ├── services/            # 天气拉取 + fallback 监控
│   │   ├── db/                  # SQLite schema + queries
│   │   └── middleware/          # token 认证
│   └── tests/                   # 22 个集成测试
├── frontend/                    # Vue 3 + ECharts
│   └── src/
│       ├── views/               # 概览 / 历史 / 设置
│       ├── components/          # 卡片 / 图表 / 来源标签
│       ├── stores/              # Pinia 状态管理
│       └── router/              # Vue Router
├── artifacts/                   # 设计文档 + 计划 + 测试笔记
├── package.json                 # monorepo 根（pnpm dev 一键启动）
└── README.md                    # 本文件
```

---

## 运行测试

```bash
# 后端测试（22 个）
cd backend && node --test tests/db.test.js
node --test tests/sensor.test.js
node --test tests/weather.test.js
node --test tests/source.test.js
```

---

## 架构文档

| 文档 | 路径 |
|------|------|
| 技术评审 | `artifacts/weather-station-tech-review.md` |
| 方案 C 设计 | `artifacts/weather-station-esp-design.md` |
| 方案 C 计划 | `artifacts/weather-station-esp-plan.md` |
| 方案 C 调试笔记 | `artifacts/weather-station-debug-lessons.md` |
| 方案 A 设计 | `artifacts/weather-station-web-design.md` |
| 方案 A 计划 | `artifacts/weather-station-web-plan.md` |
| 方案 A 联调笔记 | `artifacts/weather-station-web-testing-notes.md` |
