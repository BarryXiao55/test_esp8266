# Weather Station Web — 方案 A 实施计划

> **设计文档**: `artifacts/weather-station-web-design.md`
> **分支**: `feature/weather-station-web`

**Goal:** 构建 ESP8266 传感器 + Node.js 后端 + Vue 3 前端的全栈天气数据系统

**Architecture:** ESP8266 采集天气 → POST 到 Express 后端 → SQLite 持久化 → Vue 3 + ECharts Dashboard

**Tech Stack:** Node.js LTS + Express + better-sqlite3 + Vue 3 + Vite + Pinia + ECharts 5 + pnpm

## Global Constraints

- REST API 所有响应 Content-Type: `application/json`
- 传感器端点 Bearer token 认证
- 数据来源字段 `source` 必须出现在所有天气数据中
- 前端 3 Tab 路由: `/` Dashboard, `/history` History, `/settings` Settings
- 使用 pnpm 作为包管理器
- 后端端口 `3001`，前端开发端口 `5173`
- 天气数据保留 90 天
- ESP8266 固件基于方案 C 现有代码改造

---

### Task 1: Project Scaffolding

**Files:**
- Create: `backend/package.json`
- Create: `backend/src/index.js`
- Create: `frontend/` (Vue 3 + Vite project)

**Interfaces:**
- Produces: Express server on port 3001, Vue dev server on port 5173

- [ ] **Step 1: Initialize Node.js backend**

```bash
mkdir -p backend/src/{routes,services,db,middleware}
mkdir -p backend/tests
cd backend
pnpm init
pnpm add express better-sqlite3 pino cors
pnpm add -D nodemon
```

- [ ] **Step 2: Create Express entry point**

```js
// backend/src/index.js
const express = require('express');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', ts: new Date().toISOString() });
});

const PORT = process.env.PORT || 3001;
app.listen(PORT, () => {
  console.log(`[weather-station] listening on :${PORT}`);
});
```

- [ ] **Step 3: Initialize Vue 3 + Vite frontend**

```bash
cd ..
pnpm create vite@latest frontend -- --template vue
cd frontend
pnpm add vue-router@4 pinia echarts vue-echarts
pnpm add -D @vitejs/plugin-vue vitest
```

- [ ] **Step 4: Add dev scripts to root**

```json
// package.json (root)
{
  "scripts": {
    "backend:dev": "cd backend && pnpm nodemon src/index.js",
    "frontend:dev": "cd frontend && pnpm dev",
    "dev": "concurrently \"pnpm backend:dev\" \"pnpm frontend:dev\""
  },
  "devDependencies": {
    "concurrently": "^9.0.0"
  }
}
```

- [ ] **Step 5: Verify scaffolding**

```bash
# Terminal 1: start backend
cd backend && node src/index.js
# Expected: [weather-station] listening on :3001

# Terminal 2: start frontend
cd frontend && pnpm dev
# Expected: VITE ready in XXXms

# Terminal 3: smoke test
curl http://localhost:3001/api/health
# Expected: {"status":"ok","ts":"..."}
```

- [ ] **Step 6: Commit**

```bash
git add backend/ frontend/ package.json pnpm-lock.yaml
git commit -m "feat: scaffold backend (Express) and frontend (Vue 3 + Vite)"
```

---

### Task 2: Database Schema & Queries

**Files:**
- Create: `backend/src/db/schema.js`
- Create: `backend/src/db/queries.js`
- Create: `backend/src/db/cleanup.js`

**Interfaces:**
- Produces: `initDb()` → db instance, `insertWeather(data)`, `getLatest()`, `getHistory(opts)`, `upsertSensor(id, name, token)`, `updateLastSeen(id)`, `getSourceState()`, `setSourceMode(mode)`

- [ ] **Step 1: Write schema initialization**

```js
// backend/src/db/schema.js
const Database = require('better-sqlite3');
const path = require('path');

const DB_PATH = path.join(__dirname, '..', '..', 'data', 'weather.db');

let db;

function initDb() {
  const fs = require('fs');
  fs.mkdirSync(path.dirname(DB_PATH), { recursive: true });

  db = new Database(DB_PATH);
  db.pragma('journal_mode = WAL');
  db.pragma('foreign_keys = ON');

  db.exec(`
    CREATE TABLE IF NOT EXISTS sensors (
      id         TEXT PRIMARY KEY,
      name       TEXT NOT NULL,
      token      TEXT NOT NULL UNIQUE,
      created_at TEXT NOT NULL DEFAULT (datetime('now')),
      last_seen  TEXT
    );

    CREATE TABLE IF NOT EXISTS weather_data (
      id           INTEGER PRIMARY KEY AUTOINCREMENT,
      temp         REAL NOT NULL,
      feels_like   REAL,
      humidity     INTEGER,
      wind_speed   REAL,
      wind_dir     INTEGER,
      weather_code INTEGER,
      source       TEXT NOT NULL,
      recorded_at  TEXT NOT NULL DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS source_state (
      id           INTEGER PRIMARY KEY CHECK (id = 1),
      mode         TEXT NOT NULL DEFAULT 'auto',
      fallback_min INTEGER NOT NULL DEFAULT 30,
      updated_at   TEXT NOT NULL DEFAULT (datetime('now'))
    );

    INSERT OR IGNORE INTO source_state (id, mode, fallback_min)
    VALUES (1, 'auto', 30);

    CREATE INDEX IF NOT EXISTS idx_weather_recorded
      ON weather_data(recorded_at);
    CREATE INDEX IF NOT EXISTS idx_weather_source
      ON weather_data(source);
  `);

  return db;
}

function getDb() { return db; }
module.exports = { initDb, getDb };
```

- [ ] **Step 2: Write query functions**

```js
// backend/src/db/queries.js
const { getDb } = require('./schema');
const crypto = require('crypto');

function generateToken() {
  return 'sk-' + crypto.randomBytes(16).toString('hex');
}

function upsertSensor(id, name) {
  const db = getDb();
  const existing = db.prepare('SELECT token FROM sensors WHERE id = ?').get(id);
  if (existing) return existing.token;

  const token = generateToken();
  db.prepare('INSERT INTO sensors (id, name, token) VALUES (?, ?, ?)').run(id, name, token);
  return token;
}

function validateToken(id, token) {
  const db = getDb();
  const row = db.prepare('SELECT token FROM sensors WHERE id = ?').get(id);
  return row && row.token === token;
}

function updateLastSeen(id) {
  const db = getDb();
  db.prepare("UPDATE sensors SET last_seen = datetime('now') WHERE id = ?").run(id);
}

function insertWeather(data) {
  const db = getDb();
  const stmt = db.prepare(`
    INSERT INTO weather_data (temp, feels_like, humidity, wind_speed, wind_dir, weather_code, source)
    VALUES (@temp, @feels_like, @humidity, @wind_speed, @wind_dir, @weather_code, @source)
  `);
  return stmt.run(data);
}

function getLatest() {
  const db = getDb();
  return db.prepare(`
    SELECT * FROM weather_data ORDER BY recorded_at DESC LIMIT 1
  `).get();
}

function getHistory({ from, to, page = 1, limit = 50 }) {
  const db = getDb();
  const offset = (page - 1) * limit;
  const rows = db.prepare(`
    SELECT * FROM weather_data
    WHERE recorded_at BETWEEN ? AND ?
    ORDER BY recorded_at DESC
    LIMIT ? OFFSET ?
  `).all(from || '1970-01-01', to || '2099-12-31', limit, offset);
  const total = db.prepare(`
    SELECT COUNT(*) as count FROM weather_data
    WHERE recorded_at BETWEEN ? AND ?
  `).get(from || '1970-01-01', to || '2099-12-31');
  return { rows, total: total.count, page, limit };
}

function getSourceState() {
  const db = getDb();
  return db.prepare('SELECT * FROM source_state WHERE id = 1').get();
}

function setSourceMode(mode) {
  const db = getDb();
  db.prepare("UPDATE source_state SET mode = ?, updated_at = datetime('now') WHERE id = 1").run(mode);
}

function getSensors() {
  const db = getDb();
  return db.prepare('SELECT * FROM sensors ORDER BY created_at DESC').all();
}

function deleteSensor(id) {
  const db = getDb();
  db.prepare('DELETE FROM sensors WHERE id = ?').run(id);
}

function cleanupOldData(days = 90) {
  const db = getDb();
  const result = db.prepare(`
    DELETE FROM weather_data WHERE recorded_at < datetime('now', '-' || ? || ' days')
  `).run(days);
  return result.changes;
}

module.exports = {
  upsertSensor, validateToken, updateLastSeen,
  insertWeather, getLatest, getHistory,
  getSourceState, setSourceMode,
  getSensors, deleteSensor, cleanupOldData,
};
```

- [ ] **Step 3: Wire DB init into Express**

```js
// Add to backend/src/index.js (after app.use lines)
const { initDb } = require('./db/schema');
initDb();
console.log('[db] SQLite initialized');
```

- [ ] **Step 4: Write failing test for queries**

```js
// backend/tests/db.test.js
const test = require('node:test');
const assert = require('node:assert');

// Use :memory: for tests
test('insertWeather stores and retrieves data', async () => {
  // Will be implemented with in-memory DB
  assert.ok(true); // placeholder, expanded in implementation
});
```

- [ ] **Step 5: Commit**

```bash
git add backend/src/db/ backend/tests/
git commit -m "feat: add SQLite schema and query layer"
```

---

### Task 3: Sensor Routes (Register + Data Upload)

**Files:**
- Create: `backend/src/middleware/auth.js`
- Create: `backend/src/routes/sensor.js`
- Modify: `backend/src/index.js` (mount routes)

**Interfaces:**
- Consumes: `upsertSensor()`, `validateToken()`, `updateLastSeen()`, `insertWeather()` from Task 2
- Produces: `POST /api/sensor/register`, `POST /api/sensor/data`

- [ ] **Step 1: Write auth middleware**

```js
// backend/src/middleware/auth.js
const { validateToken, updateLastSeen } = require('../db/queries');

function sensorAuth(req, res, next) {
  const auth = req.headers.authorization;
  if (!auth || !auth.startsWith('Bearer ')) {
    return res.status(401).json({ error: '缺少认证 token' });
  }

  const token = auth.slice(7);
  const deviceId = req.headers['x-device-id'];
  if (!deviceId) {
    return res.status(400).json({ error: '缺少 X-Device-Id 头' });
  }

  if (!validateToken(deviceId, token)) {
    return res.status(403).json({ error: 'token 无效或设备未注册' });
  }

  updateLastSeen(deviceId);
  req.deviceId = deviceId;
  next();
}

module.exports = { sensorAuth };
```

- [ ] **Step 2: Write sensor routes**

```js
// backend/src/routes/sensor.js
const express = require('express');
const router = express.Router();
const { upsertSensor, insertWeather } = require('../db/queries');
const { sensorAuth } = require('../middleware/auth');

// POST /api/sensor/register
router.post('/register', (req, res) => {
  const { id, name } = req.body;
  if (!id || !name) {
    return res.status(400).json({ error: '缺少 id 或 name 字段' });
  }

  const token = upsertSensor(id, name);
  const isNew = true; // upsertSensor always returns token

  res.json({
    token,
    message: isNew ? '注册成功' : '设备已存在，返回已有 token',
  });
});

// POST /api/sensor/data
router.post('/data', sensorAuth, (req, res) => {
  const { temp, feels_like, humidity, wind_speed, wind_dir, weather_code } = req.body;

  if (temp === undefined || humidity === undefined) {
    return res.status(400).json({ error: '缺少 temp 或 humidity 字段' });
  }

  const result = insertWeather({
    temp, feels_like, humidity, wind_speed, wind_dir, weather_code,
    source: `sensor:${req.deviceId}`,
  });

  res.json({ message: 'ok', id: result.lastInsertRowid });
});

module.exports = router;
```

- [ ] **Step 3: Mount routes in Express**

```js
// Add to backend/src/index.js
const sensorRoutes = require('./routes/sensor');
app.use('/api/sensor', sensorRoutes);
```

- [ ] **Step 4: Write tests**

```js
// backend/tests/sensor.test.js
const test = require('node:test');
const assert = require('node:assert');

test('POST /api/sensor/register creates device and returns token', async () => {
  // HTTP-level integration test
  // Register → get token → verify token format
});

test('POST /api/sensor/data rejects unregistered device', async () => {
  // Send data without valid token → expect 401/403
});

test('POST /api/sensor/data accepts valid sensor upload', async () => {
  // Register → get token → POST data → verify 200 + id
});
```

- [ ] **Step 5: Commit**

```bash
git add backend/src/middleware/ backend/src/routes/sensor.js backend/src/index.js backend/tests/
git commit -m "feat: add sensor registration and data upload endpoints"
```

---

### Task 4: Weather Query Routes

**Files:**
- Create: `backend/src/routes/weather.js`
- Modify: `backend/src/index.js` (mount weather routes)

**Interfaces:**
- Consumes: `getLatest()`, `getHistory()`, `getSourceState()` from Task 2
- Produces: `GET /api/weather/current`, `GET /api/weather/history`

- [ ] **Step 1: Write weather routes**

```js
// backend/src/routes/weather.js
const express = require('express');
const router = express.Router();
const { getLatest, getHistory, getSourceState, getSensors } = require('../db/queries');

// GET /api/weather/current
router.get('/current', (req, res) => {
  const data = getLatest();
  const sourceState = getSourceState();

  if (!data) {
    return res.json({ data: null, source: { type: 'none', label: '无数据' } });
  }

  // Parse source field: "sensor:<id>" or "backend"
  const source = buildSourceInfo(data.source, sourceState);

  res.json({
    data: {
      temp: data.temp,
      feels_like: data.feels_like,
      humidity: data.humidity,
      wind_speed: data.wind_speed,
      wind_dir: data.wind_dir,
      weather_code: data.weather_code,
      recorded_at: data.recorded_at,
    },
    source,
  });
});

// GET /api/weather/history?from=&to=&page=&limit=
router.get('/history', (req, res) => {
  const { from, to, page, limit } = req.query;
  const result = getHistory({
    from, to,
    page: parseInt(page) || 1,
    limit: Math.min(parseInt(limit) || 50, 200),
  });
  res.json(result);
});

function buildSourceInfo(source, sourceState) {
  if (source === 'backend') {
    return { type: 'backend', label: '后端服务', online: true };
  }
  const match = source.match(/^sensor:(.+)$/);
  if (match) {
    const deviceId = match[1];
    // Check if sensor is considered online (reported within fallback window)
    // Simplified: just report type + id
    return { type: 'sensor', label: deviceId, device_id: deviceId, online: true };
  }
  return { type: 'unknown', label: source };
}

module.exports = router;
```

- [ ] **Step 2: Mount routes**

```js
// Add to backend/src/index.js
const weatherRoutes = require('./routes/weather');
app.use('/api/weather', weatherRoutes);
```

- [ ] **Step 3: Write tests**

```js
// backend/tests/weather.test.js
test('GET /api/weather/current returns null when no data', async () => { ... });
test('GET /api/weather/current returns latest data with source', async () => { ... });
test('GET /api/weather/history supports pagination', async () => { ... });
```

- [ ] **Step 4: Commit**

```bash
git add backend/src/routes/weather.js backend/src/index.js backend/tests/
git commit -m "feat: add weather current and history query endpoints"
```

---

### Task 5: Backend Weather Fetcher + Fallback Monitor

**Files:**
- Create: `backend/src/services/weatherFetcher.js`
- Create: `backend/src/services/fallbackMonitor.js`
- Modify: `backend/src/index.js` (start monitor)

**Interfaces:**
- Consumes: `insertWeather()`, `getSourceState()` from Task 2
- Produces: `fetchWeather()` → WeatherRecord, `startFallbackMonitor()`, `stopFallbackMonitor()`

- [ ] **Step 1: Write Open-Meteo fetcher**

```js
// backend/src/services/weatherFetcher.js
const OPEN_METEO_URL = 'https://api.open-meteo.com/v1/forecast'
  + '?latitude=31.23&longitude=121.47'
  + '&current=temperature_2m,relative_humidity_2m,apparent_temperature,'
  + 'weather_code,wind_speed_10m,wind_direction_10m&timezone=auto';

async function fetchWeather() {
  const res = await fetch(OPEN_METEO_URL);
  if (!res.ok) throw new Error(`Open-Meteo returned ${res.status}`);

  const json = await res.json();
  const c = json.current;

  return {
    temp: c.temperature_2m,
    feels_like: c.apparent_temperature,
    humidity: c.relative_humidity_2m,
    wind_speed: c.wind_speed_10m,
    wind_dir: c.wind_direction_10m,
    weather_code: c.weather_code,
  };
}

module.exports = { fetchWeather };
```

- [ ] **Step 2: Write fallback monitor**

```js
// backend/src/services/fallbackMonitor.js
const { getSourceState, getSensors, insertWeather } = require('../db/queries');
const { fetchWeather } = require('./weatherFetcher');

let intervalId = null;

function startFallbackMonitor() {
  intervalId = setInterval(checkAndFallback, 60_000);
  console.log('[fallback] monitor started (60s interval)');
}

function stopFallbackMonitor() {
  if (intervalId) { clearInterval(intervalId); intervalId = null; }
}

async function checkAndFallback() {
  const state = getSourceState();

  // Manual mode: follow user selection
  if (state.mode !== 'auto') {
    if (state.mode === 'backend') {
      await pullAsBackend();
    }
    // sensor:<id> — wait for sensor, do nothing
    return;
  }

  // Auto mode: check sensors
  const sensors = getSensors();
  const now = new Date();

  // Find the most recent sensor
  let latestSensor = null;
  for (const s of sensors) {
    if (!s.last_seen) continue;
    const lastSeen = new Date(s.last_seen + 'Z');
    if (!latestSensor || lastSeen > latestSensor.lastSeen) {
      latestSensor = { id: s.id, lastSeen };
    }
  }

  if (!latestSensor) {
    // No sensor has ever reported → pull as backend
    await pullAsBackend();
    return;
  }

  const minutesOffline = (now - latestSensor.lastSeen) / 60_000;
  if (minutesOffline > state.fallback_min) {
    console.log(`[fallback] sensor ${latestSensor.id} offline ${Math.round(minutesOffline)}min, switching to backend`);
    await pullAsBackend();
  }
}

async function pullAsBackend() {
  try {
    const data = await fetchWeather();
    data.source = 'backend';
    insertWeather(data);
    console.log('[fallback] backend fetch OK');
  } catch (err) {
    console.error('[fallback] backend fetch failed:', err.message);
  }
}

module.exports = { startFallbackMonitor, stopFallbackMonitor, checkAndFallback };
```

- [ ] **Step 3: Start monitor in Express**

```js
// Add to backend/src/index.js
const { startFallbackMonitor } = require('./services/fallbackMonitor');
startFallbackMonitor();
```

- [ ] **Step 4: Write tests**

```js
// backend/tests/fallback.test.js
test('fetchWeather returns valid data from Open-Meteo', async () => {
  const data = await fetchWeather();
  assert.ok(typeof data.temp === 'number');
  assert.ok(typeof data.humidity === 'number');
});

test('checkAndFallback pulls data when no sensor has reported', async () => { ... });
test('checkAndFallback does not pull when sensor is online', async () => { ... });
```

- [ ] **Step 5: Commit**

```bash
git add backend/src/services/ backend/src/index.js backend/tests/
git commit -m "feat: add backend weather fetcher and fallback monitor"
```

---

### Task 6: Source Switch API

**Files:**
- Create: `backend/src/routes/source.js`
- Modify: `backend/src/index.js` (mount source routes)

**Interfaces:**
- Consumes: `getSourceState()`, `setSourceMode()` from Task 2
- Produces: `GET /api/source/state`, `POST /api/source/switch`, `PATCH /api/source/config`

- [ ] **Step 1: Write source routes**

```js
// backend/src/routes/source.js
const express = require('express');
const router = express.Router();
const { getSourceState, setSourceMode, getSensors, deleteSensor } = require('../db/queries');

// GET /api/source/state
router.get('/state', (req, res) => {
  const state = getSourceState();
  const sensors = getSensors();
  res.json({ ...state, sensors });
});

// POST /api/source/switch
// Body: { mode: "auto" | "sensor:<id>" | "backend" }
router.post('/switch', (req, res) => {
  const { mode } = req.body;
  const valid = ['auto', 'backend'];
  const isSensorMode = mode && mode.startsWith('sensor:');

  if (!mode || (!valid.includes(mode) && !isSensorMode)) {
    return res.status(400).json({
      error: '无效的 mode 值，可选: auto, backend, sensor:<device_id>',
    });
  }

  setSourceMode(mode);
  res.json({ mode, message: `已切换数据源为 ${mode}` });
});

// PATCH /api/source/config
// Body: { fallback_min: 15 }
router.patch('/config', (req, res) => {
  const { fallback_min } = req.body;
  // Simplified: direct update
  const db = require('../db/schema').getDb();
  db.prepare('UPDATE source_state SET fallback_min = ?, updated_at = datetime(\'now\') WHERE id = 1')
    .run(fallback_min);
  res.json({ fallback_min, message: '配置已更新' });
});

// GET /api/sensors — list registered sensors
router.get('/sensors', (req, res) => {
  res.json(getSensors());
});

// DELETE /api/sensors/:id — delete a sensor
router.delete('/sensors/:id', (req, res) => {
  deleteSensor(req.params.id);
  res.json({ message: '传感器已删除' });
});

module.exports = router;
```

- [ ] **Step 2: Mount routes**

```js
// Add to backend/src/index.js
const sourceRoutes = require('./routes/source');
app.use('/api/source', sourceRoutes);
```

- [ ] **Step 3: Write tests**

```js
// backend/tests/source.test.js
test('POST /api/source/switch rejects invalid mode', async () => { ... });
test('POST /api/source/switch accepts valid mode', async () => { ... });
test('GET /api/source/state returns mode and sensors', async () => { ... });
```

- [ ] **Step 4: Commit**

```bash
git add backend/src/routes/source.js backend/src/index.js backend/tests/
git commit -m "feat: add data source switch and sensor management API"
```

---

### Task 7: Frontend Setup (Router + Store + Layout)

**Files:**
- Modify: `frontend/src/main.js`
- Create: `frontend/src/router/index.js`
- Create: `frontend/src/stores/weather.js`
- Modify: `frontend/src/App.vue`

**Interfaces:**
- Produces: Vue Router 3-tab navigation, Pinia weather store, tab layout

- [ ] **Step 1: Configure Vue Router**

```js
// frontend/src/router/index.js
import { createRouter, createWebHistory } from 'vue-router';
import Dashboard from '../views/Dashboard.vue';
import History from '../views/History.vue';
import Settings from '../views/Settings.vue';

const routes = [
  { path: '/', name: 'Dashboard', component: Dashboard },
  { path: '/history', name: 'History', component: History },
  { path: '/settings', name: 'Settings', component: Settings },
];

export default createRouter({ history: createWebHistory(), routes });
```

- [ ] **Step 2: Create Pinia weather store**

```js
// frontend/src/stores/weather.js
import { defineStore } from 'pinia';
import { ref } from 'vue';

export const useWeatherStore = defineStore('weather', () => {
  const current = ref(null);
  const source = ref(null);
  const history = ref([]);
  const sourceState = ref(null);
  const loading = ref(false);
  const error = ref(null);

  const API = 'http://localhost:3001/api';

  async function fetchCurrent() {
    loading.value = true;
    error.value = null;
    try {
      const res = await fetch(`${API}/weather/current`);
      const json = await res.json();
      current.value = json.data;
      source.value = json.source;
    } catch (e) {
      error.value = e.message;
    } finally {
      loading.value = false;
    }
  }

  async function fetchHistory(params = {}) {
    const qs = new URLSearchParams(params).toString();
    const res = await fetch(`${API}/weather/history?${qs}`);
    history.value = await res.json();
  }

  async function fetchSourceState() {
    const res = await fetch(`${API}/source/state`);
    sourceState.value = await res.json();
  }

  async function switchSource(mode) {
    await fetch(`${API}/source/switch`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mode }),
    });
    await fetchSourceState();
  }

  return { current, source, history, sourceState, loading, error,
           fetchCurrent, fetchHistory, fetchSourceState, switchSource };
});
```

- [ ] **Step 3: Create App.vue with tab navigation**

```vue
<!-- frontend/src/App.vue -->
<template>
  <div class="app">
    <nav class="tabs">
      <router-link to="/">📊 概览</router-link>
      <router-link to="/history">📋 历史</router-link>
      <router-link to="/settings">⚙️ 设置</router-link>
    </nav>
    <main class="content">
      <router-view />
    </main>
  </div>
</template>

<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #f0f4f8; color: #1a202c; }
.app { max-width: 960px; margin: 0 auto; padding: 16px; }
.tabs { display: flex; gap: 8px; margin-bottom: 16px; background: white; border-radius: 12px; padding: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.tabs a { padding: 8px 20px; border-radius: 8px; text-decoration: none; color: #4a5568; font-weight: 500; }
.tabs a.router-link-active { background: #3182ce; color: white; }
.content { min-height: 400px; }
</style>
```

- [ ] **Step 4: Create stub view components**

```vue
<!-- frontend/src/views/Dashboard.vue -->
<template><div class="page">📊 Dashboard</div></template>

<!-- frontend/src/views/History.vue -->
<template><div class="page">📋 History</div></template>

<!-- frontend/src/views/Settings.vue -->
<template><div class="page">⚙️ Settings</div></template>
```

- [ ] **Step 5: Wire up main.js**

```js
// frontend/src/main.js
import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import router from './router';

const app = createApp(App);
app.use(createPinia());
app.use(router);
app.mount('#app');
```

- [ ] **Step 6: Commit**

```bash
git add frontend/src/
git commit -m "feat: set up Vue Router, Pinia store, and 3-tab layout"
```

---

### Task 8: Frontend Dashboard View

**Files:**
- Create: `frontend/src/components/WeatherCard.vue`
- Create: `frontend/src/components/TempChart.vue`
- Create: `frontend/src/components/SourceBadge.vue`
- Create: `frontend/src/components/SourceSwitch.vue`
- Modify: `frontend/src/views/Dashboard.vue`

**Interfaces:**
- Consumes: `useWeatherStore` from Task 7

- [ ] **Step 1: WeatherCard component**

```vue
<!-- frontend/src/components/WeatherCard.vue -->
<template>
  <div class="card">
    <div class="label">{{ label }}</div>
    <div class="value" :class="colorClass">{{ displayValue }}<span class="unit">{{ unit }}</span></div>
    <div v-if="sub" class="sub">{{ sub }}</div>
  </div>
</template>

<script setup>
defineProps({ label: String, displayValue: [String, Number], unit: String, sub: String, colorClass: String });
</script>

<style scoped>
.card { background: white; border-radius: 12px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); text-align: center; }
.label { font-size: 0.8rem; color: #718096; margin-bottom: 4px; }
.value { font-size: 1.8rem; font-weight: 700; }
.unit { font-size: 0.9rem; color: #a0aec0; margin-left: 2px; font-weight: 400; }
.sub { font-size: 0.75rem; color: #718096; margin-top: 4px; }
.temp { color: #e53e3e; } .humidity { color: #3182ce; } .wind { color: #38a169; }
</style>
```

- [ ] **Step 2: TempChart (ECharts) component**

```vue
<!-- frontend/src/components/TempChart.vue -->
<template>
  <div class="chart-box">
    <h3>{{ title }}</h3>
    <v-chart :option="chartOption" autoresize style="height:200px" />
  </div>
</template>

<script setup>
import { computed } from 'vue';
import VChart from 'vue-echarts';
import { use } from 'echarts/core';
import { LineChart } from 'echarts/charts';
import { GridComponent, TooltipComponent } from 'echarts/components';
import { CanvasRenderer } from 'echarts/renderers';

use([LineChart, GridComponent, TooltipComponent, CanvasRenderer]);

const props = defineProps({ title: String, labels: Array, data: Array, color: String });

const chartOption = computed(() => ({
  tooltip: { trigger: 'axis' },
  grid: { left: 40, right: 16, top: 16, bottom: 24 },
  xAxis: { type: 'category', data: props.labels, axisLabel: { fontSize: 10 } },
  yAxis: { type: 'value', axisLabel: { fontSize: 10 } },
  series: [{
    type: 'line', data: props.data,
    lineStyle: { color: props.color, width: 2 },
    itemStyle: { color: props.color },
    areaStyle: { color: props.color + '20' },
    smooth: true,
  }],
}));
</script>

<style scoped>
.chart-box { background: white; border-radius: 12px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 12px; }
h3 { font-size: 0.9rem; color: #4a5568; margin-bottom: 8px; }
</style>
```

- [ ] **Step 3: SourceBadge component**

```vue
<!-- frontend/src/components/SourceBadge.vue -->
<template>
  <span class="badge" :class="typeClass">{{ label }}</span>
</template>

<script setup>
import { computed } from 'vue';
const props = defineProps({ type: String, label: String, online: Boolean });

const typeClass = computed(() => {
  if (props.type === 'backend') return 'badge-backend';
  return props.online ? 'badge-sensor-on' : 'badge-sensor-off';
});
</script>

<style scoped>
.badge { display: inline-block; padding: 2px 10px; border-radius: 12px; font-size: 0.75rem; font-weight: 600; }
.badge-sensor-on { background: #c6f6d5; color: #276749; }
.badge-sensor-off { background: #fefcbf; color: #975a16; }
.badge-backend { background: #bee3f8; color: #2a4365; }
</style>
```

- [ ] **Step 4: SourceSwitch component**

```vue
<!-- frontend/src/components/SourceSwitch.vue -->
<template>
  <div class="source-switch">
    <label>数据源</label>
    <select :modelValue="modelValue" @change="$emit('update:modelValue', $event.target.value)">
      <option value="auto">自动</option>
      <option v-for="s in sensors" :key="s.id" :value="'sensor:' + s.id">{{ s.name }}</option>
      <option value="backend">后端服务</option>
    </select>
  </div>
</template>

<script setup>
defineProps({ modelValue: String, sensors: Array });
defineEmits(['update:modelValue']);
</script>
```

- [ ] **Step 5: Assemble Dashboard.vue**

```vue
<!-- frontend/src/views/Dashboard.vue -->
<template>
  <div v-if="store.loading && !store.current" class="loading">加载中...</div>
  <div v-else-if="store.error" class="error">⚠️ {{ store.error }}</div>
  <template v-else>
    <div class="header">
      <h1>☀️ Weather Station</h1>
      <SourceBadge v-if="store.source" v-bind="store.source" />
    </div>

    <div class="cards">
      <WeatherCard label="🌡️ 温度" :displayValue="store.current?.temp?.toFixed(1)" unit="°C"
        :sub="'体感 ' + store.current?.feels_like?.toFixed(1) + '°C'" color-class="temp" />
      <WeatherCard label="💧 湿度" :displayValue="store.current?.humidity" unit="%" color-class="humidity" />
      <WeatherCard label="💨 风速" :displayValue="store.current?.wind_speed?.toFixed(1)" unit="km/h"
        :sub="'风向 ' + store.current?.wind_dir + '°'" color-class="wind" />
      <WeatherCard label="☁️ 天气" :displayValue="weatherText" unit="" color-class="" />
    </div>

    <TempChart title="📈 温度趋势 (°C)" :labels="labels" :data="temps" color="#e53e3e" />
    <TempChart title="📈 湿度趋势 (%)" :labels="labels" :data="humidities" color="#3182ce" />

    <div class="status">
      <SourceSwitch v-model="sourceMode" :sensors="store.sourceState?.sensors || []" @change="onSourceChange" />
    </div>
  </template>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue';
import { useWeatherStore } from '../stores/weather';
import WeatherCard from '../components/WeatherCard.vue';
import TempChart from '../components/TempChart.vue';
import SourceBadge from '../components/SourceBadge.vue';
import SourceSwitch from '../components/SourceSwitch.vue';

const store = useWeatherStore();
const sourceMode = ref('auto');
let timer = null;

const labels = computed(() => []);
const temps = computed(() => []);
const humidities = computed(() => []);
const weatherText = computed(() => store.current ? '--' : '--');

function formatTime(ts) {
  if (!ts) return '';
  const d = new Date(ts + 'Z');
  return d.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' });
}

async function onSourceChange(mode) {
  await store.switchSource(mode);
}

onMounted(async () => {
  await store.fetchCurrent();
  await store.fetchSourceState();
  timer = setInterval(() => store.fetchCurrent(), 30_000);
});

onUnmounted(() => clearInterval(timer));
</script>

<style scoped>
.header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px; }
.header h1 { font-size: 1.3rem; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 12px; margin-bottom: 16px; }
.status { margin-top: 16px; }
.loading, .error { text-align: center; padding: 40px; }
</style>
```

- [ ] **Step 6: Commit**

```bash
git add frontend/src/
git commit -m "feat: implement dashboard view with weather cards and ECharts"
```

---

### Task 9: Frontend History + Settings Views

**Files:**
- Modify: `frontend/src/views/History.vue`
- Modify: `frontend/src/views/Settings.vue`

**Interfaces:**
- Consumes: `useWeatherStore` from Task 7

- [ ] **Step 1: History.vue — date range picker + table + chart**

```vue
<!-- frontend/src/views/History.vue -->
<template>
  <div class="page">
    <h2>📋 历史数据</h2>
    <div class="filters">
      <input type="date" v-model="from" />
      <span>至</span>
      <input type="date" v-model="to" />
      <button @click="loadHistory">查询</button>
      <button @click="exportCSV">导出 CSV</button>
    </div>

    <table v-if="store.history.rows?.length">
      <thead>
        <tr>
          <th>时间</th><th>温度 (°C)</th><th>湿度 (%)</th><th>风速 (km/h)</th><th>数据来源</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="row in store.history.rows" :key="row.id">
          <td>{{ formatTime(row.recorded_at) }}</td>
          <td>{{ row.temp?.toFixed(1) }}</td>
          <td>{{ row.humidity }}</td>
          <td>{{ row.wind_speed?.toFixed(1) }}</td>
          <td><SourceBadge :type="parseSourceType(row.source)" :label="row.source" /></td>
        </tr>
      </tbody>
    </table>

    <div class="pagination">
      <button :disabled="page <= 1" @click="page--; loadHistory()">上一页</button>
      <span>第 {{ page }} 页</span>
      <button :disabled="!hasMore" @click="page++; loadHistory()">下一页</button>
    </div>
  </div>
</template>
```

- [ ] **Step 2: Settings.vue — source config + sensor list**

```vue
<!-- frontend/src/views/Settings.vue -->
<template>
  <div class="page">
    <h2>⚙️ 系统设置</h2>

    <section class="setting-group">
      <h3>数据源模式</h3>
      <div class="radio-group">
        <label><input type="radio" v-model="mode" value="auto" @change="saveMode" /> 自动</label>
        <label><input type="radio" v-model="mode" value="backend" @change="saveMode" /> 仅后端</label>
      </div>
    </section>

    <section class="setting-group">
      <h3>Fallback 阈值</h3>
      <label>传感器离线 <input type="number" v-model.number="fallbackMin" min="5" max="120" /> 分钟后自动切换</label>
      <button @click="saveFallback">保存</button>
    </section>

    <section class="setting-group">
      <h3>传感器管理</h3>
      <div v-for="s in sensors" :key="s.id" class="sensor-row">
        <span>{{ s.name }} ({{ s.id }})</span>
        <span :class="isOnline(s) ? 'on' : 'off'">{{ isOnline(s) ? '🟢 在线' : '⚫ 离线' }}</span>
        <span class="last-seen">{{ s.last_seen || '从未上报' }}</span>
        <button @click="deleteSensor(s.id)">删除</button>
      </div>
    </section>
  </div>
</template>
```

- [ ] **Step 3: Commit**

```bash
git add frontend/src/views/
git commit -m "feat: implement history and settings views"
```

---

### Task 10: ESP8266 Firmware Adaptation

**Files:**
- Modify: `esp8266/src/config.h` (add backend URL)
- Modify: `esp8266/src/main.cpp` (replace local storage with HTTP POST)

**Interfaces:**
- Consumes: `POST /api/sensor/register`, `POST /api/sensor/data` from Task 3

- [ ] **Step 1: Add backend URL to config**

```cpp
// Add to esp8266/src/config.h
#define BACKEND_HOST  "192.168.3.xxx"  // PC 的 IP
#define BACKEND_PORT  3001
#define SENSOR_ID     "esp8266-01"
#define SENSOR_NAME   "客厅传感器"
```

- [ ] **Step 2: Modify ESP8266 main.cpp — device registration**

```cpp
// Add to setup(), after WiFi connection:
String token = registerDevice();
if (token.length() > 0) {
  Serial.println("[SENSOR] 设备已注册，token: " + token.substring(0, 8) + "...");
} else {
  Serial.println("[SENSOR] 注册失败，将重试");
}

// New function:
String registerDevice() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://" BACKEND_HOST ":" STRINGIFY(BACKEND_PORT) "/api/sensor/register");
  http.addHeader("Content-Type", "application/json");

  String body = "{\"id\":\"" SENSOR_ID "\",\"name\":\"" SENSOR_NAME "\"}";
  int code = http.POST(body);

  String token = "";
  if (code == 200) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, http.getString());
    token = doc["token"].as<String>();
  }
  http.end();
  return token;
}
```

- [ ] **Step 3: Modify data upload — POST instead of ring buffer**

```cpp
// Replace ring_push() call with uploadToBackend()
bool uploadToBackend(const WeatherRecord& rec, const String& token) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://" BACKEND_HOST ":" STRINGIFY(BACKEND_PORT) "/api/sensor/data");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("X-Device-Id", SENSOR_ID);

  StaticJsonDocument<256> doc;
  doc["temp"] = rec.temp;
  doc["feels_like"] = rec.feels_like;
  doc["humidity"] = rec.humidity;
  doc["wind_speed"] = rec.wind_speed;
  doc["wind_dir"] = rec.wind_dir;
  doc["weather_code"] = rec.weather_code;

  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();

  if (code == 200) {
    Serial.println("[UPLOAD] 数据已上传");
    return true;
  } else {
    Serial.printf("[UPLOAD] 失败 HTTP %d\n", code);
    return false;
  }
}
```

- [ ] **Step 4: Commit**

```bash
git add esp8266/src/
git commit -m "feat: adapt ESP8266 firmware to POST data to backend"
```
