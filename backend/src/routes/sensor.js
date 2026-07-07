const express = require('express');
const router = express.Router();
const { upsertSensor, insertWeather } = require('../db/queries');
const { sensorAuth } = require('../middleware/auth');

// POST /api/sensor/register
// Body: { id: "esp8266-01", name: "客厅传感器" }
// Returns: { token: "sk-...", message: "..." }
router.post('/register', (req, res) => {
  const { id, name } = req.body;
  if (!id || !name) {
    return res.status(400).json({ error: '缺少 id 或 name 字段' });
  }

  const token = upsertSensor(id, name);
  res.json({ token, message: '注册成功' });
});

// POST /api/sensor/data
// Headers: Authorization: Bearer <token>, X-Device-Id: <id>
// Body: { temp, feels_like, humidity, wind_speed, wind_dir, weather_code }
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
