const { getDb } = require('./schema');
const crypto = require('crypto');

// ---- Helpers ----

function generateToken() {
  return 'sk-' + crypto.randomBytes(16).toString('hex');
}

// ---- Sensor Management ----

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

function getSensors() {
  const db = getDb();
  return db.prepare('SELECT * FROM sensors ORDER BY created_at DESC').all();
}

function deleteSensor(id) {
  const db = getDb();
  db.prepare('DELETE FROM sensors WHERE id = ?').run(id);
}

// ---- Weather Data ----

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
    SELECT id, temp, feels_like, humidity, wind_speed, wind_dir, weather_code, source, recorded_at
    FROM weather_data
    WHERE recorded_at BETWEEN ? AND ?
    ORDER BY recorded_at DESC
    LIMIT ? OFFSET ?
  `).all(from || '1970-01-01', to || '2099-12-31', limit, offset);

  const { count: total } = db.prepare(`
    SELECT COUNT(*) as count FROM weather_data
    WHERE recorded_at BETWEEN ? AND ?
  `).get(from || '1970-01-01', to || '2099-12-31');

  return { rows, total, page, limit };
}

// ---- Source State ----

function getSourceState() {
  const db = getDb();
  return db.prepare('SELECT * FROM source_state WHERE id = 1').get();
}

function setSourceMode(mode) {
  const db = getDb();
  db.prepare("UPDATE source_state SET mode = ?, updated_at = datetime('now') WHERE id = 1").run(mode);
}

function setFallbackMin(minutes) {
  const db = getDb();
  db.prepare("UPDATE source_state SET fallback_min = ?, updated_at = datetime('now') WHERE id = 1").run(minutes);
}

// ---- Maintenance ----

function cleanupOldData(days = 90) {
  const db = getDb();
  const result = db.prepare(`
    DELETE FROM weather_data WHERE recorded_at < datetime('now', '-' || ? || ' days')
  `).run(days);
  return result.changes;
}

module.exports = {
  upsertSensor, validateToken, updateLastSeen,
  getSensors, deleteSensor,
  insertWeather, getLatest, getHistory,
  getSourceState, setSourceMode, setFallbackMin,
  cleanupOldData,
};
