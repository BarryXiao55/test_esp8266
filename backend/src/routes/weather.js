const express = require('express');
const router = express.Router();
const { getLatest, getHistory, getSensors } = require('../db/queries');

// Parse source field into structured info
function buildSourceInfo(source) {
  if (!source) return { type: 'none', label: '无数据', online: false };

  if (source === 'backend') {
    return { type: 'backend', label: '后端服务', online: true };
  }

  const match = source.match(/^sensor:(.+)$/);
  if (match) {
    const deviceId = match[1];
    const sensors = getSensors();
    const sensor = sensors.find(s => s.id === deviceId);
    const online = !!sensor?.last_seen;
    return {
      type: 'sensor',
      label: sensor ? sensor.name : deviceId,
      device_id: deviceId,
      online,
    };
  }

  return { type: 'unknown', label: source, online: false };
}

// GET /api/weather/current
router.get('/current', (req, res) => {
  const data = getLatest();

  if (!data) {
    return res.json({ data: null, source: { type: 'none', label: '无数据', online: false } });
  }

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
    source: buildSourceInfo(data.source),
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

module.exports = router;
