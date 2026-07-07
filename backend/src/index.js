const express = require('express');
const cors = require('cors');

const { initDb } = require('./db/schema');

const app = express();
app.use(cors());
app.use(express.json());

// 初始化 SQLite
initDb();
console.log('[db] SQLite initialized');

app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', ts: new Date().toISOString() });
});

const PORT = process.env.PORT || 3001;
app.listen(PORT, () => {
  console.log(`[weather-station] listening on :${PORT}`);
});
