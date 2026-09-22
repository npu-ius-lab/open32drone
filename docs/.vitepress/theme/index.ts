import DefaultTheme from 'vitepress/theme'
import type { Theme } from 'vitepress'
import { h } from 'vue'
import ProjectHome from './components/ProjectHome.vue'
import LanguageSwitch from './components/LanguageSwitch.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  Layout: () => h(DefaultTheme.Layout, null, {
    'nav-bar-content-after': () => h(LanguageSwitch),
  }),
  enhanceApp({ app }) {
    app.component('ProjectHome', ProjectHome)
  },
} satisfies Theme
