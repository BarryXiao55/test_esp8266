const test = require('node:test');
const assert = require('node:assert');
const express = require('express');
const Database = require('better-sqlite3');
const http = require('http');

const schema = require('../src/db/schema');
const weatherRoutes = require('../src/routes/weather');

function buildApp(db) {
  schema.setDb(db);
  const app = express();
  app.use('/api/weather', weatherRoutes);
  return app;
}

function request(app, method, path) {
  return new Promise((resolve) => {
    const server = http.createServer(app);
    server.listen(0, () => {
      const port = server.address().port;
      http.get(`http://localhost:${port}${path}`, (res) => {
        let data = '';
        res.on('data', c => data += c);
        res.on('end', () => {
          try { resolve({ status: res.statusCode, body: JSON.parse(data) }); }
          catch { resolve({ status: res.statusCode, body: data }); }
          server.close();
        });
      });
    });
  });
}

function setupDb() {
  const db = new Database(':memory:');
  schema.setDb(db);  // runs schema
  return db;
}

test('GET /api/weather/current returns null when no data', async () => {
  const db = setupDb();
  const app = buildApp(db);
  const res = await request(app, 'GET', '/api/weather/current');
  assert.equal(res.status, 200);
  assert.equal(res.body.data, null);
  assert.equal(res.body.source.type, 'none');
});

test('GET /api/weather/current returns latest data with source info', async () => {
  const db = setupDb();
  db.prepare('INSERT INTO sensors (id, name, token) VALUES (?, ?, ?)').run('esp-01', '客厅', 'sk-test');
  db.prepare(`INSERT INTO weather_data (temp, humidity, source)
    VALUES (?, ?, ?)`).run(33.3, 65, 'sensor:esp-01');

  const app = buildApp(db);
  const res = await request(app, 'GET', '/api/weather/current');
  assert.equal(res.status, 200);
  assert.equal(res.body.data.temp, 33.3);
  assert.equal(res.body.data.humidity, 65);
  assert.equal(res.body.source.type, 'sensor');
  assert.equal(res.body.source.label, '客厅');
  assert.equal(res.body.source.device_id, 'esp-01');
});

test('GET /api/weather/current shows backend source correctly', async () => {
  const db = setupDb();
  db.prepare(`INSERT INTO weather_data (temp, humidity, source)
    VALUES (?, ?, ?)`).run(30.0, 70, 'backend');

  const app = buildApp(db);
  const res = await request(app, 'GET', '/api/weather/current');
  assert.equal(res.status, 200);
  assert.equal(res.body.source.type, 'backend');
  assert.equal(res.body.source.label, '后端服务');
});

test('GET /api/weather/history supports pagination', async () => {
  const db = setupDb();
  const insert = db.prepare(`INSERT INTO weather_data (temp, humidity, source, recorded_at)
    VALUES (?, ?, ?, ?)`);

  for (let i = 0; i < 5; i++) {
    insert.run(30 + i, 60 + i, 'backend', `2026-07-0${1 + i}T12:00:00`);
  }

  const app = buildApp(db);
  const res = await request(app, 'GET', '/api/weather/history?page=1&limit=3');
  assert.equal(res.status, 200);
  assert.equal(res.body.rows.length, 3);
  assert.equal(res.body.total, 5);
  assert.equal(res.body.page, 1);
});
