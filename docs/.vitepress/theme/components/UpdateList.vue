<script setup lang="ts">
import { computed } from 'vue'
import { updates } from './updates'
const props = defineProps<{ english?: boolean; limit?: number }>()
const entries = computed(() => updates.slice(0, props.limit).map(entry => ({
  ...entry, content: props.english ? entry.en : entry.zh,
})))
</script>

<template>
  <ol class="update-list" :class="{ compact: limit }">
    <li v-for="entry in entries" :key="entry.commit">
      <div class="update-meta">
        <time :datetime="entry.date">{{ entry.date }}</time>
      </div>
      <h3>{{ entry.content.title }}</h3>
      <ul><li v-for="item in entry.content.items" :key="item">{{ item }}</li></ul>
    </li>
  </ol>
</template>

<style scoped>
.update-list { list-style: none; margin: 0; padding: 0; }
.update-list > li { padding: 24px 0; border-bottom: 1px solid var(--vp-c-divider); }
.update-list > li:first-child { padding-top: 0; }
.update-meta { display: flex; flex-wrap: wrap; justify-content: space-between; gap: 8px; font-size: 12px; line-height: 1.6; font-variant-numeric: tabular-nums; }
time { color: var(--vp-c-text-2); }
h3 { margin: 10px 0 8px; font-size: 17px; line-height: 1.6; font-weight: 600; letter-spacing: 0; }
ul { margin: 0; padding-left: 18px; color: var(--vp-c-text-2); font-size: 14px; line-height: 1.8; }
ul li { margin: 4px 0; }
.compact ul { list-style: none; padding: 0; }
</style>
