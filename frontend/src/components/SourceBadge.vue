<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(defineProps<{
  type: string
  label: string
  online?: boolean
}>(), { online: false })

const typeClass = computed(() => {
  if (props.type === 'backend') return 'badge-backend'
  return props.online ? 'badge-sensor-on' : 'badge-sensor-off'
})

const statusText = computed(() => {
  if (props.type === 'backend') return '🔵 ' + props.label
  return (props.online ? '🟢 ' : '🟡 ') + props.label
})
</script>

<template>
  <span class="badge" :class="typeClass">{{ statusText }}</span>
</template>

<style scoped>
.badge { display: inline-block; padding: 2px 12px; border-radius: 12px; font-size: 0.75rem; font-weight: 600; }
.badge-sensor-on { background: #c6f6d5; color: #276749; }
.badge-sensor-off { background: #fefcbf; color: #975a16; }
.badge-backend { background: #bee3f8; color: #2a4365; }
</style>
