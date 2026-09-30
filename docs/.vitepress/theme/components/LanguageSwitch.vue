<script setup lang="ts">
import { computed, ref, watch, onMounted, onUnmounted } from 'vue'
import { useData, withBase } from 'vitepress'

const { page } = useData()
const root = ref<HTMLDetailsElement>()
const trigger = ref<HTMLElement>()
const currentEnglish = computed(() => destination.value.lang === 'zh-CN')
const choices = computed(() => {
  const current = `/${page.value.relativePath.replace(/\.md$/, '')}`
  return [
    { label: '简体中文', lang: 'zh-CN', path: currentEnglish.value ? destination.value.path : current },
    { label: 'English', lang: 'en', path: currentEnglish.value ? current : destination.value.path },
  ]
})
function dismiss(event: PointerEvent) {
  if (!root.value?.contains(event.target as Node)) close()
}
function close() {
  if (root.value) root.value.open = false
}
function escape(event: KeyboardEvent) {
  if (event.key === 'Escape' && root.value?.open) {
    close()
    trigger.value?.focus()
  }
}
watch(() => page.value.relativePath, close)
onMounted(() => {
  document.addEventListener('pointerdown', dismiss)
  document.addEventListener('keydown', escape)
})
onUnmounted(() => {
  document.removeEventListener('pointerdown', dismiss)
  document.removeEventListener('keydown', escape)
})
// Tutorials use .en; reference/project pages use .zh-CN for Chinese.
const destination = computed(() => {
  const path = page.value.relativePath.replace(/\.md$/, '')
  if (path.startsWith('guide/')) {
    return path.endsWith('.en')
      ? { path: `/${path.slice(0, -3)}`, label: '简体中文', lang: 'zh-CN' }
      : { path: `/${path}.en`, label: 'English', lang: 'en' }
  }
  if (path.startsWith('reference/') || path.startsWith('project/')) {
    return path.endsWith('.zh-CN')
      ? { path: `/${path.slice(0, -6)}`, label: 'English', lang: 'en' }
      : { path: `/${path}.zh-CN`, label: '简体中文', lang: 'zh-CN' }
  }
  return { path: '/guide/01-project.en', label: 'English', lang: 'en' }
})
</script>

<template>
  <details ref="root" class="language-switch">
    <summary ref="trigger" class="language-trigger"
      aria-label="Select language / 选择语言" aria-controls="site-language-options">
      <span lang="zh-CN">中文</span>
      <span aria-hidden="true">/</span>
      <span lang="en">English</span>
      <span aria-hidden="true" class="chevron">⌄</span>
    </summary>
    <nav id="site-language-options" class="language-options" aria-label="语言 / Language">
      <a v-for="choice in choices" :key="choice.lang" :href="withBase(choice.path)"
        :lang="choice.lang" :hreflang="choice.lang"
        :aria-current="(currentEnglish ? 'en' : 'zh-CN') === choice.lang ? 'page' : undefined"
        @click="close">{{ choice.label }}</a>
    </nav>
  </details>
</template>

<style scoped>
.language-switch { position: relative; flex-shrink: 0; font-size: 14px; font-weight: 500; }
.language-trigger { display: flex; align-items: center; gap: 6px; min-height: 44px; padding: 0 12px; white-space: nowrap; cursor: pointer; }
.language-trigger { list-style: none; }
.language-trigger::-webkit-details-marker { display: none; }
.language-trigger:hover, .language-options a:hover { color: var(--vp-c-brand-1); }
.chevron { transition: transform .2s; }
.language-switch[open] .chevron { transform: rotate(180deg); }
.language-options { position: absolute; top: 100%; right: 0; min-width: 148px; padding: 8px; border: 1px solid var(--vp-c-divider); border-radius: 6px; background: var(--vp-c-bg-elv); box-shadow: var(--vp-shadow-1); }
.language-options a { display: block; padding: 8px 12px; border-radius: 3px; }
.language-options a[aria-current="page"] { color: var(--vp-c-brand-1); background: var(--vp-c-default-soft); }
.language-trigger:focus-visible, .language-options a:focus-visible { outline: 2px solid var(--vp-c-brand-1); outline-offset: 2px; }
</style>
