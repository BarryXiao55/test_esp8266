const test = require('node:test');
const assert = require('node:assert');
const express = require('express');
const Database = require('better-sqlite3');
const http = require('http');

const schema = require('../src/db/schema');
const sourceRoutes = require('../src/routes/source');

function buildApp(db) {
  schema.setDb(db);
  const app = express();
  app.use(express.json());
  app.use('/api/source', sourceRoutes);
  return app;
}

function request(app, method, path, body) {
  return new Promise((resolve) => {
    const server = http.createServer(app);
    server.listen(0, () => {
      const port = server.address().port;
      const options = {
        hostname: 'localhost', port, path, method,
        headers: { 'Content-Type': 'application/json' },
      };
      const req = http.request(options, (res) => {
        let data = '';
        res.on('data', c => data += c);
        res.on('end', () => {
          try { resolve({ status: res.statusCode, body: JSON.parse(data) }); }
          catch { resolve({ status: res.statusCode, body: data }); }
          server.close();
        });
      });
      if (body) req.write(JSON.stringify(body));
      req.end();
    });
  });
}

function setupDb() {
  const db = new Database(':memory:');
  schema.setDb(db);
  return db;
}

test('GET /api/source/state returns default auto mode', async () => {
  const db = setupDb();
  const app = buildApp(db);
  const res = await request(app, 'GET', '/api/source/state');
  assert.equal(res.status, 200);
  assert.equal(res.body.mode, 'auto');
  assert.equal(res.body.fallback_min, 30);
  assert.deepEqual(res.body.sensors, []);
});

test('POST /api/source/switch rejects invalid mode', async () => {
  const db = setupDb();
  const app = buildApp(db);
  const res = await request(app, 'POST', '/api/source/switch', { mode: 'invalid' });
  assert.equal(res.status, 400);
});

test('POST /api/source/switch accepts valid mode', async () => {
  const db = setupDb();
  const app = buildApp(db);
  const res = await request(app, 'POST', '/api/source/switch', { mode: 'backend' });
  assert.equal(res.status, 200);
  assert.equal(res.body.mode, 'backend');
});

test('POST /api/source/switch accepts sensor mode', async () => {
  const db = setupDb();
  const app = buildApp(db);
  const res = await request(app, 'POST', '/api/source/switch', { mode: 'sensor:esp-01' });
  assert.equal(res.status, 200);
  assert.equal(res.body.mode, 'sensor:esp-01');
});

test('PATCH /api/source/config updates fallback threshold', async () => {
  const db = setupDb();
  const app = buildApp(db);
  const res = await request(app, 'PATCH', '/api/source/config', { fallback_min: 15 });
  assert.equal(res.status, 200);
  assert.equal(res.body.fallback_min, 15);
});

test('GET /api/source/sensors lists registered sensors', async () => {
  const db = setupDb();
  db.prepare('INSERT INTO sensors (id, name, token) VALUES (?, ?, ?)').run('esp-01', '客厅', 'sk-abc');
  const app = buildApp(db);
  const res = await request(app, 'GET', '/api/source/sensors');
  assert.equal(res.status, 200);
  assert.equal(res.body.length, 1);
  assert.equal(res.body[0].id, 'esp-01');
});
