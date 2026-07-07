<script setup lang="ts">
import { computed } from 'vue'
import VChart from 'vue-echarts'
import { use } from 'echarts/core'
import { LineChart } from 'echarts/charts'
import { GridComponent, TooltipComponent } from 'echarts/components'
import { CanvasRenderer } from 'echarts/renderers'

use([LineChart, GridComponent, TooltipComponent, CanvasRenderer])

const props = withDefaults(defineProps<{
  title: string
  labels: string[]
  data: (number | null)[]
  color: string
}>(), { labels: () => [], data: () => [] })

const chartOption = computed(() => ({
  tooltip: { trigger: 'axis' as const },
  grid: { left: 40, right: 16, top: 16, bottom: 24 },
  xAxis: { type: 'category' as const, data: props.labels, axisLabel: { fontSize: 10 } },
  yAxis: { type: 'value' as const, axisLabel: { fontSize: 10 } },
  series: [{
    type: 'line' as const,
    data: props.data,
    lineStyle: { color: props.color, width: 2 },
    itemStyle: { color: props.color },
    areaStyle: { color: props.color + '20' },
    smooth: true,
    connectNulls: true,
  }],
}))
</script>

<template>
  <div class="chart-box" v-if="labels.length > 0">
    <h3>{{ title }}</h3>
    <v-chart :option="chartOption" autoresize style="height:200px" />
  </div>
</template>

<style scoped>
.chart-box { background: white; border-radius: 12px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 12px; }
h3 { font-size: 0.9rem; color: #4a5568; margin-bottom: 8px; font-weight: 500; }
</style>
