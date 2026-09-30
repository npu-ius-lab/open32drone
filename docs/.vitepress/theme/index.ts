import DefaultTheme from 'vitepress/theme'
import type { Theme } from 'vitepress'
import LocalizedLayout from './components/LocalizedLayout.vue'
import './docs.css'

export default {
  extends: DefaultTheme,
  Layout: LocalizedLayout,
} satisfies Theme
