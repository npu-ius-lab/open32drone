<script setup lang="ts">
import { computed, provide } from 'vue'
import { dataSymbol, useData, withBase } from 'vitepress'
import DefaultTheme from 'vitepress/theme'
import LanguageSwitch from './LanguageSwitch.vue'

// Preserve existing .en / .zh-CN URLs. Scope the translated theme to this
// layout's descendants; do not mutate VitePress's shared site configuration.
const data = useData()
const english = computed(() => {
  const path = data.page.value.relativePath
  return path.startsWith('guide/') ? path.endsWith('.en.md')
    : /^(reference|project)\//.test(path) && !path.endsWith('.zh-CN.md')
})
const theme = computed(() => {
  const original = data.theme.value
  if (!english.value) return original
  return {
    ...original,
    nav: [
      { text: 'Tutorial', link: '/guide/01-project.en', activeMatch: '/guide/' },
      { text: 'Reference', activeMatch: '/reference/', items: [
        { text: 'Source and build', link: '/reference/source-build' },
        { text: 'Parameters and interfaces', link: '/reference/firmware' },
      ] },
      { text: 'Downloads', link: 'https://github.com/npu-ius-lab/open32drone/releases' },
    ],
    logoLink: withBase('/guide/01-project.en'),
    outline: { label: 'On this page', level: [2, 3] },
    search: { ...original.search, options: { ...original.search?.options,
      translations: {
        button: { buttonText: 'Search', buttonAriaLabel: 'Search documentation' },
        modal: { displayDetails: 'Display detailed list', resetButtonTitle: 'Reset search',
          backButtonTitle: 'Close search', noResultsText: 'No results for',
          footer: { selectText: 'to select', navigateText: 'to navigate', closeText: 'to close' } },
      },
    } },
    docFooter: { prev: 'Previous chapter', next: 'Next chapter' },
    returnToTopLabel: 'Back to top', sidebarMenuLabel: 'Menu', darkModeSwitchLabel: 'Appearance',
    lightModeSwitchTitle: 'Switch to light theme', darkModeSwitchTitle: 'Switch to dark theme',
    footer: { message: 'Open32Drone documentation' },
  }
})
provide(dataSymbol, { ...data, theme, lang: computed(() => english.value ? 'en' : 'zh-CN') })
</script>

<template>
  <DefaultTheme.Layout>
    <template #nav-bar-content-after><LanguageSwitch /></template>
  </DefaultTheme.Layout>
</template>
