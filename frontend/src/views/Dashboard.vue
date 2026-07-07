<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useWeatherStore } from '../stores/weather'
import WeatherCard from '../components/WeatherCard.vue'
import TempChart from '../components/TempChart.vue'
import SourceBadge from '../components/SourceBadge.vue'

const store = useWeatherStore()
const sourceMode = ref('auto')
let timer: ReturnType<typeof setInterval> | null = null

const labels = computed(() => {
  return store.history?.rows?.slice().reverse().map((r: any) =>
    r.recorded_at ? new Date(r.recorded_at + 'Z').toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }) : ''
  ) ?? []
})

const temps = computed(() => store.history?.rows?.slice().reverse().map((r: any) => r.temp) ?? [])
const humidities = computed(() => store.history?.rows?.slice().reverse().map((r: any) => r.humidity) ?? [])

async function onSourceChange() {
  if (sourceMode.value) await store.switchSource(sourceMode.value)
}

async function refreshAll() {
  await Promise.all([
    store.fetchCurrent(),
    store.fetchHistory({ limit: 24 }),
    store.fetchSourceState(),
  ])
}

onMounted(async () => {
  await refreshAll()
  timer = setInterval(refreshAll, 30_000)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<template>
  <div v-if="store.loading && !store.current" class="loading">加载中...</div>
  <div v-else-if="store.error" class="error">⚠️ {{ store.error }}</div>
  <template v-else>
    <!-- Header -->
    <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:16px">
      <h1 style="font-size:1.3rem">☀️ Weather Station</h1>
      <SourceBadge v-if="store.source" v-bind="store.source" />
    </div>

    <!-- Data Cards -->
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-bottom:16px">
      <WeatherCard label="🌡️ 温度"
        :displayValue="store.current?.temp?.toFixed(1)" unit="°C"
        :sub="'体感 ' + (store.current?.feels_like?.toFixed(1) ?? '--') + '°C'"
        colorClass="temp" />
      <WeatherCard label="💧 湿度"
        :displayValue="store.current?.humidity" unit="%"
        colorClass="humidity" />
      <WeatherCard label="💨 风速"
        :displayValue="store.current?.wind_speed?.toFixed(1)" unit="km/h"
        :sub="'风向 ' + (store.current?.wind_dir ?? '--') + '°'"
        colorClass="wind" />
      <WeatherCard label="☁️ 天气"
        :displayValue="store.weatherText" />
    </div>

    <!-- Charts -->
    <TempChart title="📈 温度趋势 (°C)" :labels="labels" :data="temps" color="#e53e3e" />
    <TempChart title="📈 湿度趋势 (%)" :labels="labels" :data="humidities" color="#3182ce" />

    <!-- Source Switch -->
    <div style="background:white;border-radius:12px;padding:16px;box-shadow:0 1px 3px rgba(0,0,0,0.1)">
      <div style="display:flex;align-items:center;gap:12px">
        <label style="font-size:0.85rem;color:#4a5568;font-weight:500">数据源</label>
        <select v-model="sourceMode" @change="onSourceChange" style="flex:1;max-width:300px">
          <option value="auto">🤖 自动</option>
          <option v-for="s in (store.sourceState?.sensors ?? [])" :key="s.id" :value="'sensor:' + s.id">
            🟢 {{ s.name }} ({{ s.id }})
          </option>
          <option value="backend">🔵 后端服务</option>
        </select>
      </div>
    </div>
  </template>
</template>

<style scoped>
h1 { color: #2d3748; }
</style>
