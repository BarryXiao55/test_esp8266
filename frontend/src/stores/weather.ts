import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

const API = 'http://localhost:3001/api'

export const useWeatherStore = defineStore('weather', () => {
  const current = ref<any>(null)
  const source = ref<any>(null)
  const history = ref<any>({ rows: [], total: 0, page: 1, limit: 50 })
  const sourceState = ref<any>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  async function fetchCurrent() {
    loading.value = true
    error.value = null
    try {
      const res = await fetch(`${API}/weather/current`)
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const json = await res.json()
      current.value = json.data
      source.value = json.source
    } catch (e: any) {
      error.value = e.message
    } finally {
      loading.value = false
    }
  }

  async function fetchHistory(params: Record<string, string | number> = {}) {
    const qs = new URLSearchParams()
    Object.entries(params).forEach(([k, v]) => { if (v) qs.set(k, String(v)) })
    try {
      const res = await fetch(`${API}/weather/history?${qs}`)
      history.value = await res.json()
    } catch (e: any) {
      error.value = e.message
    }
  }

  async function fetchSourceState() {
    try {
      const res = await fetch(`${API}/source/state`)
      sourceState.value = await res.json()
    } catch (e: any) {
      error.value = e.message
    }
  }

  async function switchSource(mode: string) {
    try {
      await fetch(`${API}/source/switch`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode }),
      })
      await fetchSourceState()
    } catch (e: any) {
      error.value = e.message
    }
  }

  async function saveFallbackConfig(fallback_min: number) {
    try {
      await fetch(`${API}/source/config`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ fallback_min }),
      })
      await fetchSourceState()
    } catch (e: any) {
      error.value = e.message
    }
  }

  async function deleteSensor(id: string) {
    try {
      await fetch(`${API}/source/sensors/${id}`, { method: 'DELETE' })
      await fetchSourceState()
    } catch (e: any) {
      error.value = e.message
    }
  }

  const weatherText = computed(() => {
    if (!current.value) return '--'
    const code = current.value.weather_code
    const labels: Record<number, string> = {
      0: '☀️ 晴', 1: '🌤️ 大部晴', 2: '⛅ 多云', 3: '☁️ 阴',
      45: '🌫️ 雾', 48: '🌫️ 雾凇', 51: '🌧️ 小毛毛雨', 53: '🌧️ 中毛毛雨', 55: '🌧️ 大毛毛雨',
      61: '🌧️ 小雨', 63: '🌧️ 中雨', 65: '🌧️ 大雨',
      71: '❄️ 小雪', 73: '❄️ 中雪', 75: '❄️ 大雪',
      80: '🌦️ 小阵雨', 81: '🌦️ 中阵雨', 82: '🌦️ 大阵雨',
      95: '⛈️ 雷暴', 96: '⛈️ 冰雹雷暴', 99: '⛈️ 大冰雹雷暴',
    }
    return labels[code] || `WMO ${code}`
  })

  return {
    current, source, history, sourceState, loading, error,
    fetchCurrent, fetchHistory, fetchSourceState, switchSource, saveFallbackConfig, deleteSensor,
    weatherText,
  }
})
