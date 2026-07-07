const Database = require('better-sqlite3');
const path = require('path');
const fs = require('fs');

const DB_PATH = path.join(__dirname, '..', '..', 'data', 'weather.db');

let db;

function runSchema(database) {
  database.pragma('journal_mode = WAL');
  database.pragma('foreign_keys = ON');

  database.exec(`
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
}

function initDb() {
  fs.mkdirSync(path.dirname(DB_PATH), { recursive: true });
  db = new Database(DB_PATH);
  runSchema(db);
  return db;
}

function setDb(database) {
  db = database;
  runSchema(db);
}

function getDb() {
  if (!db) throw new Error('DB not initialized — call initDb() first');
  return db;
}

module.exports = { initDb, setDb, getDb, runSchema };
