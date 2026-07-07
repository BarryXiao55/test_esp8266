<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useWeatherStore } from '../stores/weather'

const store = useWeatherStore()
const mode = ref('auto')
const fallbackMin = ref(30)

async function saveMode() {
  await store.switchSource(mode.value)
}

async function saveFallback() {
  await store.saveFallbackConfig(fallbackMin.value)
}

async function removeSensor(id: string) {
  if (confirm('确定删除此传感器？')) await store.deleteSensor(id)
}

onMounted(async () => {
  await store.fetchSourceState()
  if (store.sourceState) {
    mode.value = store.sourceState.mode
    fallbackMin.value = store.sourceState.fallback_min
  }
})
</script>

<template>
  <div class="page">
    <h2>⚙️ 系统设置</h2>

    <!-- Source Mode -->
    <section style="margin-bottom:24px">
      <h3 style="font-size:0.9rem;color:#4a5568;margin-bottom:8px">数据源模式</h3>
      <div style="display:flex;gap:24px">
        <label style="display:flex;align-items:center;gap:4px;cursor:pointer">
          <input type="radio" v-model="mode" value="auto" @change="saveMode" /> 🤖 自动
        </label>
        <label style="display:flex;align-items:center;gap:4px;cursor:pointer">
          <input type="radio" v-model="mode" value="backend" @change="saveMode" /> 🔵 仅后端
        </label>
      </div>
      <p style="font-size:0.75rem;color:#a0aec0;margin-top:4px">
        当前模式: <strong>{{ mode }}</strong>
      </p>
    </section>

    <!-- Fallback Threshold -->
    <section style="margin-bottom:24px">
      <h3 style="font-size:0.9rem;color:#4a5568;margin-bottom:8px">Fallback 阈值</h3>
      <div style="display:flex;align-items:center;gap:8px">
        <label style="font-size:0.85rem">传感器离线</label>
        <input type="number" v-model.number="fallbackMin" min="5" max="120" style="width:70px" />
        <span style="font-size:0.85rem">分钟后自动切换</span>
        <button @click="saveFallback">保存</button>
      </div>
    </section>

    <!-- Sensor Management -->
    <section>
      <h3 style="font-size:0.9rem;color:#4a5568;margin-bottom:8px">传感器管理</h3>
      <div v-if="!store.sourceState?.sensors?.length" style="color:#a0aec0;font-size:0.85rem;padding:12px 0">
        暂无注册传感器
      </div>
      <div v-for="s in store.sourceState?.sensors ?? []" :key="s.id"
        style="display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid #e2e8f0;gap:12px">
        <span style="font-weight:500;font-size:0.85rem">{{ s.name }}</span>
        <span style="font-size:0.75rem;color:#718096">{{ s.id }}</span>
        <span :style="{color: s.last_seen ? '#38a169' : '#a0aec0', fontSize:'0.75rem'}">
          {{ s.last_seen ? '🟢 ' + s.last_seen : '⚫ 从未上报' }}
        </span>
        <button @click="removeSensor(s.id)" style="background:#e53e3e;font-size:0.75rem;padding:4px 10px">删除</button>
      </div>
    </section>
  </div>
</template>
