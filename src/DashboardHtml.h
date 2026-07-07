#pragma once
#include <pgmspace.h>

// HTML Dashboard - 使用 PROGMEM 存于 Flash，不占 RAM
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP8266 天气站</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.7/dist/chart.umd.min.js"></script>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #f0f4f8; color: #1a202c; padding: 16px; min-height: 100vh;
  }
  .container { max-width: 800px; margin: 0 auto; }
  h1 {
    font-size: 1.3rem; text-align: center; padding: 12px 0;
    color: #2b6cb0; display: flex; align-items: center; justify-content: center;
    gap: 8px;
  }
  .update-time { text-align: center; color: #718096; font-size: 0.8rem; margin-bottom: 16px; }
  .cards {
    display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 12px; margin-bottom: 16px;
  }
  .card {
    background: white; border-radius: 12px; padding: 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1); text-align: center;
  }
  .card .label { font-size: 0.8rem; color: #718096; margin-bottom: 4px; }
  .card .value {
    font-size: 1.8rem; font-weight: 700; color: #2d3748;
  }
  .card .unit { font-size: 0.9rem; color: #a0aec0; margin-left: 2px; }
  .card .sub { font-size: 0.75rem; color: #718096; margin-top: 4px; }
  .temp-color { color: #e53e3e; }
  .humidity-color { color: #3182ce; }
  .wind-color { color: #38a169; }
  .chart-container {
    background: white; border-radius: 12px; padding: 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 16px;
  }
  .status-bar {
    background: white; border-radius: 12px; padding: 12px 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    font-size: 0.8rem; color: #718096;
    display: flex; flex-wrap: wrap; justify-content: space-between; gap: 8px;
  }
  .loading { text-align: center; color: #a0aec0; padding: 40px; }
  .error { text-align: center; color: #e53e3e; padding: 20px; display: none; }
  @media (max-width: 480px) {
    .cards { grid-template-columns: repeat(2, 1fr); }
    .card .value { font-size: 1.4rem; }
  }
</style>
</head>
<body>
<div class="container">
  <h1>&#9728;&#65039; <span id="location">天气站</span></h1>
  <div class="update-time">&#9201;&#65039; 更新于: <span id="updateTime">--</span></div>

  <div class="cards">
    <div class="card">
      <div class="label">&#127777;&#65039; 温度</div>
      <div class="value temp-color"><span id="temp">--</span><span class="unit">&#176;C</span></div>
      <div class="sub">体感 <span id="feels">--</span>&#176;C</div>
    </div>
    <div class="card">
      <div class="label">&#128167; 湿度</div>
      <div class="value humidity-color"><span id="humidity">--</span><span class="unit">%</span></div>
    </div>
    <div class="card">
      <div class="label">&#127788;&#65039; 风速</div>
      <div class="value wind-color"><span id="wind">--</span><span class="unit">km/h</span></div>
      <div class="sub">风向 <span id="windDir">--</span></div>
    </div>
    <div class="card">
      <div class="label">&#9729;&#65039; 天气</div>
      <div class="value" style="font-size:1.3rem"><span id="weatherText">--</span></div>
    </div>
  </div>

  <div class="chart-container">
    <canvas id="tempChart" height="200"></canvas>
  </div>

  <div class="status-bar">
    <span>&#128200; WiFi: <span id="rssi">--</span> dBm</span>
    <span>&#129504; 内存: <span id="heap">--</span> KB</span>
    <span>&9201;&#65039; 运行: <span id="uptime">--</span></span>
    <span>&#128202; 记录: <span id="records">0</span> 条</span>
  </div>

  <div class="error" id="errorMsg">&#9888;&#65039; 无法获取数据，正在重试...</div>
</div>

<script>
let tempChart = null;
const COLORS = { temp: '#e53e3e', fill: '#fed7d7', humidity: '#3182ce' };

function formatTime(ts) {
  const d = new Date(ts * 1000);
  return d.toLocaleString('zh-CN', { hour: '2-digit', minute: '2-digit' });
}

function formatUptime(sec) {
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (d > 0) return d + '天' + h + '时';
  if (h > 0) return h + '时' + m + '分';
  return m + '分';
}

function updateDashboard(data) {
  document.getElementById('errorMsg').style.display = 'none';
  document.getElementById('location').textContent = data.system.location || '天气站';

  const c = data.current;
  if (c) {
    document.getElementById('temp').textContent = c.temp != null ? c.temp.toFixed(1) : '--';
    document.getElementById('feels').textContent = c.feels_like != null ? c.feels_like.toFixed(1) : '--';
    document.getElementById('humidity').textContent = c.humidity != null ? c.humidity : '--';
    document.getElementById('wind').textContent = c.wind_speed != null ? c.wind_speed.toFixed(1) : '--';
    document.getElementById('windDir').textContent = c.wind_label || '--';
    document.getElementById('weatherText').textContent = c.weather_text || '--';
    document.getElementById('updateTime').textContent = c.timestamp ? formatTime(c.timestamp) : '--';
  }

  const sys = data.system;
  if (sys) {
    document.getElementById('rssi').textContent = sys.rssi || '--';
    document.getElementById('heap').textContent = sys.free_heap ? (sys.free_heap / 1024).toFixed(0) : '--';
    document.getElementById('uptime').textContent = sys.uptime ? formatUptime(sys.uptime) : '--';
    document.getElementById('records').textContent = sys.records || 0;
  }

  // Chart.js 温度曲线
  const hist = data.history || [];
  const labels = hist.map(r => r.time ? formatTime(r.time) : '');
  const temps = hist.map(r => r.temp != null ? r.temp : null);

  if (tempChart) {
    tempChart.data.labels = labels;
    tempChart.data.datasets[0].data = temps;
    tempChart.update('none');
  } else if (labels.length > 0) {
    const ctx = document.getElementById('tempChart').getContext('2d');
    tempChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: labels,
        datasets: [{
          label: '温度 (&#176;C)',
          data: temps,
          borderColor: COLORS.temp,
          backgroundColor: COLORS.fill,
          fill: true,
          tension: 0.3,
          pointRadius: 3,
          pointHoverRadius: 6,
          spanGaps: true
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: true,
        plugins: { legend: { display: false } },
        scales: {
          x: {
            ticks: { maxTicksLimit: 8, font: { size: 10 } },
            grid: { display: false }
          },
          y: {
            ticks: { font: { size: 10 } },
            grid: { color: '#e2e8f0' }
          }
        }
      }
    });
  }
}

function fetchWeather() {
  fetch('/api/weather')
    .then(r => r.json())
    .then(data => updateDashboard(data))
    .catch(err => {
      document.getElementById('errorMsg').style.display = 'block';
    });
}

// 首次加载
fetchWeather();
// 30秒轮询
setInterval(fetchWeather, 30000);
</script>
</body>
</html>
)rawliteral";

static const size_t DASHBOARD_HTML_SIZE = sizeof(DASHBOARD_HTML);
