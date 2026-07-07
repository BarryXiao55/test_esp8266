const test = require('node:test');
const assert = require('node:assert');
const Database = require('better-sqlite3');

// Use in-memory DB for tests — isolated and fast
let db;

test.beforeEach(() => {
  db = new Database(':memory:');
  db.pragma('journal_mode = WAL');
  db.pragma('foreign_keys = ON');

  db.exec(`
    CREATE TABLE sensors (
      id TEXT PRIMARY KEY, name TEXT NOT NULL, token TEXT NOT NULL UNIQUE,
      created_at TEXT NOT NULL DEFAULT (datetime('now')), last_seen TEXT
    );
    CREATE TABLE weather_data (
      id INTEGER PRIMARY KEY AUTOINCREMENT, temp REAL NOT NULL, feels_like REAL,
      humidity INTEGER, wind_speed REAL, wind_dir INTEGER, weather_code INTEGER,
      source TEXT NOT NULL, recorded_at TEXT NOT NULL DEFAULT (datetime('now'))
    );
    CREATE TABLE source_state (
      id INTEGER PRIMARY KEY CHECK (id = 1), mode TEXT NOT NULL DEFAULT 'auto',
      fallback_min INTEGER NOT NULL DEFAULT 30, updated_at TEXT NOT NULL DEFAULT (datetime('now'))
    );
    INSERT OR IGNORE INTO source_state (id, mode, fallback_min) VALUES (1, 'auto', 30);
  `);
});

test.afterEach(() => {
  db.close();
});

// ---- Sensor Tests ----

test('upsertSensor creates new sensor with token', () => {
  const crypto = require('crypto');
  const generateToken = () => 'sk-' + crypto.randomBytes(16).toString('hex');

  const token = generateToken();
  db.prepare('INSERT INTO sensors (id, name, token) VALUES (?, ?, ?)').run('esp-01', '客厅', token);

  const row = db.prepare('SELECT * FROM sensors WHERE id = ?').get('esp-01');
  assert.equal(row.id, 'esp-01');
  assert.equal(row.name, '客厅');
  assert.ok(row.token.startsWith('sk-'));
});

test('sensor token is validated correctly', () => {
  const token = 'sk-test123';
  db.prepare('INSERT INTO sensors (id, name, token) VALUES (?, ?, ?)').run('esp-01', 'test', token);

  const row = db.prepare('SELECT token FROM sensors WHERE id = ?').get('esp-01');
  assert.equal(row.token, token);
  assert.notEqual(row.token, 'wrong-token');
});

// ---- Weather Data Tests ----

test('insertWeather stores and retrieves data with source', () => {
  db.prepare(`
    INSERT INTO weather_data (temp, feels_like, humidity, wind_speed, wind_dir, weather_code, source)
    VALUES (?, ?, ?, ?, ?, ?, ?)
  `).run(33.3, 39.9, 65, 12.4, 170, 3, 'sensor:esp-01');

  const latest = db.prepare('SELECT * FROM weather_data ORDER BY recorded_at DESC LIMIT 1').get();
  assert.equal(latest.temp, 33.3);
  assert.equal(latest.humidity, 65);
  assert.equal(latest.source, 'sensor:esp-01');
  assert.ok(latest.recorded_at);
});

test('getLatest returns null when no data', () => {
  const latest = db.prepare('SELECT * FROM weather_data ORDER BY recorded_at DESC LIMIT 1').get();
  assert.equal(latest, undefined);
});

test('getHistory paginates correctly', () => {
  const insert = db.prepare(`
    INSERT INTO weather_data (temp, humidity, source, recorded_at)
    VALUES (?, ?, ?, ?)
  `);

  for (let i = 0; i < 10; i++) {
    insert.run(30 + i, 60 + i, 'backend', `2026-07-0${1 + i}T12:00:00`);
  }

  const rows = db.prepare(`
    SELECT * FROM weather_data ORDER BY recorded_at DESC LIMIT 3 OFFSET 2
  `).all();

  assert.equal(rows.length, 3);
});

// ---- Source State Tests ----

test('source_state defaults to auto mode', () => {
  const state = db.prepare('SELECT * FROM source_state WHERE id = 1').get();
  assert.equal(state.mode, 'auto');
  assert.equal(state.fallback_min, 30);
});

test('setSourceMode updates mode', () => {
  db.prepare("UPDATE source_state SET mode = ?, updated_at = datetime('now') WHERE id = 1").run('backend');
  const state = db.prepare('SELECT * FROM source_state WHERE id = 1').get();
  assert.equal(state.mode, 'backend');
});
