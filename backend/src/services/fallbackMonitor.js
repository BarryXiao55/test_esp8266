const { getSourceState, getSensors, insertWeather } = require('../db/queries');
const { fetchWeather } = require('./weatherFetcher');

let intervalId = null;

function startFallbackMonitor() {
  // Check immediately, then every 60 seconds
  checkAndFallback().catch(err => console.error('[fallback] initial check failed:', err.message));
  intervalId = setInterval(() => {
    checkAndFallback().catch(err => console.error('[fallback] check failed:', err.message));
  }, 60_000);
  console.log('[fallback] monitor started (60s interval)');
}

function stopFallbackMonitor() {
  if (intervalId) { clearInterval(intervalId); intervalId = null; }
}

async function checkAndFallback() {
  const state = getSourceState();

  // Manual mode: follow user selection
  if (state.mode !== 'auto') {
    if (state.mode === 'backend') {
      await pullAsBackend();
    }
    // sensor:<id> — wait for that specific sensor, do nothing
    return;
  }

  // Auto mode: check if any sensor is alive
  const sensors = getSensors();
  if (sensors.length === 0) {
    // No sensors registered → pull as backend
    await pullAsBackend();
    return;
  }

  const now = new Date();
  let latestSensor = null;

  for (const s of sensors) {
    if (!s.last_seen) continue;
    const lastSeen = new Date(s.last_seen + 'Z');
    if (!latestSensor || lastSeen > latestSensor.lastSeen) {
      latestSensor = { id: s.id, name: s.name, lastSeen };
    }
  }

  if (!latestSensor) {
    // No sensor has ever reported → pull as backend
    console.log('[fallback] no sensor data ever reported, using backend');
    await pullAsBackend();
    return;
  }

  const minutesOffline = (now - latestSensor.lastSeen) / 60_000;
  if (minutesOffline > state.fallback_min) {
    console.log(`[fallback] sensor ${latestSensor.name} offline ${Math.round(minutesOffline)}min > ${state.fallback_min}min threshold, switching to backend`);
    await pullAsBackend();
  }
}

async function pullAsBackend() {
  try {
    const data = await fetchWeather();
    data.source = 'backend';
    insertWeather(data);
    console.log('[fallback] backend fetch OK');
  } catch (err) {
    console.error('[fallback] backend fetch failed:', err.message);
  }
}

module.exports = { startFallbackMonitor, stopFallbackMonitor, checkAndFallback };
