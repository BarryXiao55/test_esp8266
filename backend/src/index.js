const express = require('express');
const cors = require('cors');

const { initDb } = require('./db/schema');

const app = express();
app.use(cors());
app.use(express.json());

// 初始化 SQLite
initDb();
console.log('[db] SQLite initialized');

// 路由
app.use('/api/sensor', require('./routes/sensor'));
app.use('/api/weather', require('./routes/weather'));
app.use('/api/source', require('./routes/source'));

app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', ts: new Date().toISOString() });
});

// 启动 fallback 监控（每 60 秒检查传感器在线状态）
const { startFallbackMonitor } = require('./services/fallbackMonitor');
startFallbackMonitor();

// 定期清理 90 天前的旧数据（每小时）
const { cleanupOldData } = require('./db/queries');
setInterval(() => {
  const deleted = cleanupOldData(90);
  if (deleted > 0) console.log(`[cleanup] removed ${deleted} old records`);
}, 60 * 60 * 1000);

const PORT = process.env.PORT || 3001;
app.listen(PORT, () => {
  console.log(`[weather-station] listening on :${PORT}`);
});
