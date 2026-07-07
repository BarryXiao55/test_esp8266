const test = require('node:test');
const assert = require('node:assert');
const express = require('express');
const Database = require('better-sqlite3');
const http = require('http');

const schema = require('../src/db/schema');
const sensorRoutes = require('../src/routes/sensor');

function buildApp(db) {
  schema.setDb(db);
  const app = express();
  app.use(express.json());
  app.use('/api/sensor', sensorRoutes);
  return app;
}

function request(app, method, path, { headers = {}, body } = {}) {
  return new Promise((resolve, reject) => {
    const server = http.createServer(app);
    server.listen(0, () => {
      const port = server.address().port;
      const options = {
        hostname: 'localhost', port, path, method,
        headers: { 'Content-Type': 'application/json', ...headers },
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
      req.on('error', reject);
      if (body) req.write(JSON.stringify(body));
      req.end();
    });
  });
}

// ---- Tests ----

test('POST /api/sensor/register creates device and returns token', async () => {
  const db = new Database(':memory:');
  const app = buildApp(db);

  const res = await request(app, 'POST', '/api/sensor/register', {
    body: { id: 'esp-01', name: '客厅传感器' },
  });
  assert.equal(res.status, 200);
  assert.ok(res.body.token.startsWith('sk-'));
  assert.equal(res.body.message, '注册成功');
});

test('POST /api/sensor/register returns 400 when missing fields', async () => {
  const db = new Database(':memory:');
  const app = buildApp(db);

  const res = await request(app, 'POST', '/api/sensor/register', {
    body: { id: 'esp-01' },
  });
  assert.equal(res.status, 400);
});

test('POST /api/sensor/data rejects unauthenticated request', async () => {
  const db = new Database(':memory:');
  const app = buildApp(db);

  const res = await request(app, 'POST', '/api/sensor/data', {
    body: { temp: 30, humidity: 60 },
  });
  assert.equal(res.status, 401);
});

test('POST /api/sensor/data accepts valid upload', async () => {
  const db = new Database(':memory:');
  const app = buildApp(db);

  const reg = await request(app, 'POST', '/api/sensor/register', {
    body: { id: 'esp-02', name: '阳台传感器' },
  });
  const token = reg.body.token;

  const res = await request(app, 'POST', '/api/sensor/data', {
    headers: { 'Authorization': `Bearer ${token}`, 'X-Device-Id': 'esp-02' },
    body: { temp: 33.3, feels_like: 39.9, humidity: 65, wind_speed: 12.4, wind_dir: 170, weather_code: 3 },
  });
  assert.equal(res.status, 200);
  assert.equal(res.body.message, 'ok');
  assert.ok(res.body.id > 0);
});

test('POST /api/sensor/data rejects invalid token', async () => {
  const db = new Database(':memory:');
  const app = buildApp(db);

  await request(app, 'POST', '/api/sensor/register', {
    body: { id: 'esp-03', name: 'test' },
  });

  const res = await request(app, 'POST', '/api/sensor/data', {
    headers: { 'Authorization': 'Bearer wrong-token', 'X-Device-Id': 'esp-03' },
    body: { temp: 30, humidity: 60 },
  });
  assert.equal(res.status, 403);
});
