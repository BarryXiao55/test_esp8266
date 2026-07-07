const express = require('express');
const router = express.Router();
const { getSourceState, setSourceMode, setFallbackMin, getSensors, deleteSensor } = require('../db/queries');

// GET /api/source/state — current source mode + sensor list
router.get('/state', (req, res) => {
  const state = getSourceState();
  const sensors = getSensors();
  res.json({ ...state, sensors });
});

// POST /api/source/switch — switch data source
// Body: { mode: "auto" | "sensor:<id>" | "backend" }
router.post('/switch', (req, res) => {
  const { mode } = req.body;
  const valid = ['auto', 'backend'];
  const isSensorMode = mode && mode.startsWith('sensor:');

  if (!mode || (!valid.includes(mode) && !isSensorMode)) {
    return res.status(400).json({
      error: '无效的 mode 值，可选: auto, backend, sensor:<device_id>',
    });
  }

  setSourceMode(mode);
  res.json({ mode, message: `已切换数据源为 ${mode}` });
});

// PATCH /api/source/config — update fallback threshold
// Body: { fallback_min: 15 }
router.patch('/config', (req, res) => {
  const { fallback_min } = req.body;
  if (!fallback_min || fallback_min < 5 || fallback_min > 120) {
    return res.status(400).json({ error: 'fallback_min 需在 5-120 之间' });
  }
  setFallbackMin(fallback_min);
  res.json({ fallback_min, message: '配置已更新' });
});

// GET /api/source/sensors — list registered sensors
router.get('/sensors', (req, res) => {
  res.json(getSensors());
});

// DELETE /api/source/sensors/:id — delete a sensor
router.delete('/sensors/:id', (req, res) => {
  deleteSensor(req.params.id);
  res.json({ message: '传感器已删除' });
});

module.exports = router;
