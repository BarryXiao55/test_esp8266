<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useWeatherStore } from '../stores/weather'
import SourceBadge from '../components/SourceBadge.vue'

const store = useWeatherStore()
const from = ref('')
const to = ref('')
const page = ref(1)
const limit = ref(50)

const hasMore = ref(false)

async function loadHistory() {
  await store.fetchHistory({ from: from.value, to: to.value, page: page.value, limit: limit.value })
  hasMore.value = store.history.rows.length >= limit.value
}

function exportCSV() {
  const rows = store.history.rows ?? []
  let csv = '时间,温度(°C),湿度(%),风速(km/h),数据来源\n'
  for (const r of rows) {
    csv += `${r.recorded_at},${r.temp},${r.humidity},${r.wind_speed},${r.source}\n`
  }
  const blob = new Blob(['﻿' + csv], { type: 'text/csv' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url; a.download = 'weather-history.csv'; a.click()
  URL.revokeObjectURL(url)
}

function prevPage() { if (page.value > 1) { page.value--; loadHistory() } }
function nextPage() { page.value++; loadHistory() }

onMounted(() => loadHistory())
</script>

<template>
  <div class="page">
    <h2>📋 历史数据</h2>

    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:16px">
      <input type="date" v-model="from" style="width:140px" />
      <span style="color:#718096">至</span>
      <input type="date" v-model="to" style="width:140px" />
      <button @click="page=1; loadHistory()">查询</button>
      <button @click="exportCSV" style="background:#38a169">导出 CSV</button>
    </div>

    <table v-if="store.history?.rows?.length" style="width:100%;border-collapse:collapse">
      <thead>
        <tr style="border-bottom:2px solid #e2e8f0;text-align:left">
          <th style="padding:8px">时间</th>
          <th>温度 (°C)</th>
          <th>湿度 (%)</th>
          <th>风速 (km/h)</th>
          <th>数据来源</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="row in store.history.rows" :key="row.id" style="border-bottom:1px solid #e2e8f0">
          <td style="padding:8px">{{ row.recorded_at }}</td>
          <td>{{ row.temp?.toFixed(1) ?? '--' }}</td>
          <td>{{ row.humidity ?? '--' }}</td>
          <td>{{ row.wind_speed?.toFixed(1) ?? '--' }}</td>
          <td><SourceBadge
            :type="row.source === 'backend' ? 'backend' : 'sensor'"
            :label="row.source"
          /></td>
        </tr>
      </tbody>
    </table>
    <p v-else style="color:#a0aec0;text-align:center;padding:40px">暂无数据</p>

    <div style="display:flex;justify-content:center;align-items:center;gap:16px;margin-top:16px">
      <button :disabled="page <= 1" @click="prevPage">← 上一页</button>
      <span style="font-size:0.85rem;color:#718096">第 {{ page }} 页</span>
      <button :disabled="!hasMore" @click="nextPage">下一页 →</button>
    </div>
  </div>
</template>
