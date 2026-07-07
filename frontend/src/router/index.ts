import { createRouter, createWebHistory } from 'vue-router'
import Dashboard from '../views/Dashboard.vue'
import History from '../views/History.vue'
import Settings from '../views/Settings.vue'

const routes = [
  { path: '/', name: 'Dashboard', component: Dashboard },
  { path: '/history', name: 'History', component: History },
  { path: '/settings', name: 'Settings', component: Settings },
]

export default createRouter({ history: createWebHistory(), routes })
