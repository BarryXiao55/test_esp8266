<script setup lang="ts">
</script>

<template>
  <div class="app">
    <nav class="tabs">
      <router-link to="/">📊 概览</router-link>
      <router-link to="/history">📋 历史</router-link>
      <router-link to="/settings">⚙️ 设置</router-link>
    </nav>
    <main class="content">
      <router-view />
    </main>
  </div>
</template>

<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f0f4f8; color: #1a202c; }
.app { max-width: 960px; margin: 0 auto; padding: 16px; }
.tabs { display: flex; gap: 8px; margin-bottom: 16px; background: white; border-radius: 12px; padding: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.tabs a { padding: 8px 20px; border-radius: 8px; text-decoration: none; color: #4a5568; font-weight: 500; }
.tabs a.router-link-active { background: #3182ce; color: white; }
.content { min-height: 400px; }
.loading, .error { text-align: center; padding: 40px; }
.error { color: #e53e3e; }
button {
  background: #3182ce; color: white; border: none; padding: 8px 16px;
  border-radius: 6px; cursor: pointer; font-size: 0.85rem;
}
button:hover { background: #2c5282; }
button:disabled { background: #a0aec0; cursor: not-allowed; }
input, select {
  padding: 6px 10px; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 0.85rem;
}
.page { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.page h2 { font-size: 1.1rem; margin-bottom: 16px; color: #2d3748; }
</style>
