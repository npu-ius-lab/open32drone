<script setup lang="ts">
import { computed } from 'vue'
import { updates } from './updates'
const props = defineProps<{ english?: boolean; limit?: number }>()
const entries = computed(() => updates.slice(0, props.limit).map(entry => ({
  ...entry, content: props.english ? entry.en : entry.zh,
})))
</script>

<template>
  <ol class="update-list">
    <li v-for="entry in entries" :key="entry.id">
      <time :datetime="entry.date">{{ entry.date }}</time>
      <p class="update-copy"><strong>{{ entry.content.title }}</strong><span>{{ entry.content.items.join(' ') }}</span></p>
    </li>
  </ol>
</template>

<style scoped>
.update-list { list-style: none; margin: 20px 0 0; padding: 0; }
.update-list > li { display: grid; grid-template-columns: 96px minmax(0, 1fr); gap: 16px; padding: 16px 0; margin: 0; border-bottom: 1px solid var(--vp-c-divider); }
.update-list > li:first-child { padding-top: 0; }
.update-list > li:last-child { padding-bottom: 0; border-bottom: 0; }
time { padding-top: 2px; color: var(--vp-c-text-2); font-size: 13px; line-height: 1.9; font-variant-numeric: tabular-nums; }
.update-copy { margin: 0; font-size: 14px; line-height: 1.9; }
.update-copy strong { margin-right: .6em; font-weight: 600; }
.update-copy span { color: var(--vp-c-text-2); }
@media (max-width: 600px) {
  .update-list > li { grid-template-columns: 1fr; gap: 4px; }
  time { padding-top: 0; font-size: 12px; }
}
</style>
