<script setup lang="ts">
defineProps<{
  label: string
  displayValue?: string | number
  unit?: string
  sub?: string
  colorClass?: string
}>()
</script>

<template>
  <div class="card">
    <div class="label">{{ label }}</div>
    <div class="value" :class="colorClass">
      {{ displayValue ?? '--' }}<span v-if="unit" class="unit">{{ unit }}</span>
    </div>
    <div v-if="sub" class="sub">{{ sub }}</div>
  </div>
</template>

<style scoped>
.card { background: white; border-radius: 12px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); text-align: center; }
.label { font-size: 0.8rem; color: #718096; margin-bottom: 4px; }
.value { font-size: 1.8rem; font-weight: 700; }
.unit { font-size: 0.9rem; color: #a0aec0; margin-left: 2px; font-weight: 400; }
.sub { font-size: 0.75rem; color: #718096; margin-top: 4px; }
.temp { color: #e53e3e; }
.humidity { color: #3182ce; }
.wind { color: #38a169; }
</style>
