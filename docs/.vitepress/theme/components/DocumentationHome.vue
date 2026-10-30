<script setup lang="ts">
import { computed } from 'vue'
import { withBase } from 'vitepress'
import UpdateList from './UpdateList.vue'

const props = defineProps<{ english?: boolean }>()
const chapters = computed(() => [
  { path: '01-project', zh: ['项目介绍', '平台组成、功能与控制方式'], en: ['Project overview', 'Components, capabilities and control options'] },
  { path: '03-hardware', zh: ['开始制作', '零件采购、机架打印与电路焊接'], en: ['Start building', 'Parts, frame printing and soldering'] },
  { path: '04-firmware-flight', zh: ['刷写与首飞', '固件安装、传感器校准与首次飞行'], en: ['Firmware and first flight', 'Installation, calibration and first flight'] },
  { path: '05-tuning', zh: ['调参与排查', 'PID 调整、飞行日志与故障排查'], en: ['Tuning and diagnosis', 'PID tuning, flight logs and troubleshooting'] },
  { path: '06-ros', zh: ['ROS 2 控制', '连接飞机、读取状态与编写飞行程序'], en: ['ROS 2 control', 'Connection, telemetry and flight programming'] },
  { path: '07-rl', zh: ['仿真与强化学习', '数值仿真、控制器与策略训练'], en: ['Simulation and reinforcement learning', 'Simulation, controllers and policy training'] },
].map(item => ({ path: `/guide/${item.path}${props.english ? '.en' : ''}`, text: props.english ? item.en : item.zh })))
const suffix = computed(() => props.english ? '' : '.zh-CN')
</script>

<template>
  <div class="documentation-home">
    <header>
      <h1>{{ english ? 'Open32Drone documentation' : 'Open32Drone 文档' }}</h1>
      <p class="home-description">{{ english
        ? 'Assembly, flight setup and development guides for the Open32Drone micro quadrotor.'
        : 'Open32Drone 微型四旋翼的制作、飞行与开发指南。' }}</p>
    </header>

    <section id="guide" aria-labelledby="guide-title">
      <h2 id="guide-title">{{ english ? 'Tutorials' : '教程' }}</h2>
      <table class="chapter-list" :aria-label="english ? 'Tutorial directory' : '教程目录'">
        <thead><tr><th scope="col">{{ english ? 'Chapter' : '章节' }}</th><th scope="col">{{ english ? 'Contents' : '主要内容' }}</th></tr></thead>
        <tbody>
          <tr v-for="chapter in chapters" :key="chapter.path">
            <th scope="row"><a :href="withBase(chapter.path)">{{ chapter.text[0] }}</a></th>
            <td>{{ chapter.text[1] }}</td>
          </tr>
        </tbody>
      </table>
    </section>

    <section class="home-resources" aria-labelledby="resources-title">
      <h2 id="resources-title">{{ english ? 'References and resources' : '参考与资源' }}</h2>
      <dl class="resource-list">
        <div>
          <dt>{{ english ? 'Development' : '开发参考' }}</dt>
          <dd>
            <a :href="withBase(`/reference/source-build${suffix}`)">{{ english ? 'Source and build' : '源码与编译' }}</a>
            <a :href="withBase(`/reference/firmware${suffix}`)">{{ english ? 'Parameters and interfaces' : '参数与接口' }}</a>
          </dd>
        </div>
        <div>
          <dt>{{ english ? 'Downloads' : '软件与硬件' }}</dt>
          <dd>
            <a href="https://github.com/npu-ius-lab/open32drone/releases">{{ english ? 'Software releases' : '软件下载' }}</a>
            <a href="https://oshwhub.com/fanchewang/open32drone">{{ english ? 'PCB and BOM' : 'PCB 与 BOM' }}</a>
            <a href="https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842">{{ english ? 'Frame files' : '机架文件' }}</a>
          </dd>
        </div>
        <div>
          <dt>{{ english ? 'Project' : '参与项目' }}</dt>
          <dd>
            <a :href="withBase(`/project/contributing${suffix}`)">{{ english ? 'Contributing' : '参与贡献' }}</a>
            <a href="https://github.com/npu-ius-lab/open32drone/issues">{{ english ? 'Issues' : '问题反馈' }}</a>
            <a :href="withBase(`/project/third-party${suffix}`)">{{ english ? 'Licenses and origins' : '许可证与来源' }}</a>
          </dd>
        </div>
      </dl>
    </section>

    <section class="home-updates" aria-labelledby="updates-title">
      <div class="updates-heading">
        <h2 id="updates-title">{{ english ? 'News' : '重要更新' }}</h2>
        <a :href="withBase(`/project/changelog${suffix}`)">{{ english ? 'All updates' : '全部更新' }}</a>
      </div>
      <UpdateList :english="english" :limit="3" />
    </section>
  </div>
</template>

<style scoped>
/* Use the documentation theme's typography, colors and heading rules. */
.documentation-home { max-width: 688px; margin: 0 auto; }
.home-description { color: var(--vp-c-text-2); }
h2 { margin-top: 32px; padding-top: 24px; font-size: 22px; }
.chapter-list { display: table; width: 100%; table-layout: fixed; margin: 20px 0 0; border-collapse: collapse; font-size: 14px; }
.chapter-list th, .chapter-list td { padding: 13px 16px; border: 0; border-bottom: 1px solid var(--vp-c-divider); text-align: left; line-height: 1.7; overflow-wrap: anywhere; }
.chapter-list thead th { color: var(--vp-c-brand-1); background: var(--vp-c-brand-soft); font-size: 13px; font-weight: 600; }
.chapter-list th:first-child { width: 35%; }
.chapter-list tbody th { font-weight: 500; }
.chapter-list tbody tr { background: transparent; }
.chapter-list tbody tr:hover { background: var(--vp-c-bg-soft); }
.chapter-list td { color: var(--vp-c-text-2); }
.chapter-list a { text-decoration: none; }
.resource-list { margin: 20px 0 0; font-size: 14px; border: 1px solid var(--vp-c-divider); border-radius: 6px; overflow: hidden; }
.resource-list > div { display: grid; grid-template-columns: 112px minmax(0, 1fr); }
.resource-list > div + div { border-top: 1px solid var(--vp-c-divider); }
.resource-list dt { padding: 12px 16px; color: var(--vp-c-text-2); background: var(--vp-c-bg-soft); }
.resource-list dd { display: flex; flex-wrap: wrap; align-content: center; column-gap: 24px; row-gap: 6px; margin: 0; padding: 12px 16px; }
.resource-list a { text-decoration: none; }
.updates-heading { display: flex; align-items: baseline; justify-content: space-between; gap: 16px; border-top: 1px solid var(--vp-c-divider); margin-top: 32px; padding-top: 24px; }
.updates-heading h2 { border: 0; margin: 0; padding: 0; }
.updates-heading > a { font-size: 13px; text-decoration: none; }
.home-updates :deep(.update-list) { padding: 0; margin: 24px 0 0; }
.chapter-list a:hover, .resource-list a:hover { text-decoration: underline; }
a:focus-visible { outline: 2px solid var(--vp-c-brand-1); outline-offset: 4px; }
#guide { scroll-margin-top: 90px; }
@media (max-width: 600px) {
  .chapter-list th, .chapter-list td { padding: 10px; font-size: 13px; }
  .chapter-list th:first-child { width: 40%; }
  .resource-list > div { grid-template-columns: 1fr; }
  .resource-list dt { padding: 8px 12px; }
  .resource-list dd { column-gap: 20px; padding: 10px 12px; }
}
</style>
