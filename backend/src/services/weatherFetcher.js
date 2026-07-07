// Open-Meteo API fetcher for backend fallback
// Same API as ESP8266 uses, shared coordinates with Variant C

const OPEN_METEO_URL = 'https://api.open-meteo.com/v1/forecast'
  + '?latitude=31.23&longitude=121.47'
  + '&current=temperature_2m,relative_humidity_2m,apparent_temperature,'
  + 'weather_code,wind_speed_10m,wind_direction_10m&timezone=auto';

async function fetchWeather() {
  const res = await fetch(OPEN_METEO_URL);
  if (!res.ok) throw new Error(`Open-Meteo returned ${res.status}`);

  const json = await res.json();
  const c = json.current;

  return {
    temp: c.temperature_2m,
    feels_like: c.apparent_temperature,
    humidity: c.relative_humidity_2m,
    wind_speed: c.wind_speed_10m,
    wind_dir: c.wind_direction_10m,
    weather_code: c.weather_code,
  };
}

module.exports = { fetchWeather };
