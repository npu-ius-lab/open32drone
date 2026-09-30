import { defineConfig } from 'vitepress'
import { posix } from 'node:path'

const guide = [
  { text: '01 · 项目介绍', link: '/guide/01-project' },
  { text: '02 · 制作准备', link: '/guide/02-goals' },
  { text: '03 · 开始制作', link: '/guide/03-hardware' },
  { text: '04 · 刷写与首飞', link: '/guide/04-firmware-flight' },
  { text: '05 · 调参与排查', link: '/guide/05-tuning' },
  { text: '06 · ROS 2 控制', link: '/guide/06-ros' },
  { text: '07 · 仿真与强化学习', link: '/guide/07-rl' },
]

const guideEn = guide.map((item, index) => ({
  text: ['01 · Project', '02 · Plan your build', '03 · Start Building', '04 · First flight',
    '05 · Tuning and diagnosis', '06 · ROS 2', '07 · Simulation and RL'][index],
  link: `${item.link}.en`,
}))
const reference = [
  { text: '源码与编译', link: '/reference/source-build.zh-CN' },
  { text: '参数与接口', link: '/reference/firmware.zh-CN' },
]
const project = [
  { text: '更新日志', link: '/project/changelog.zh-CN' },
  { text: '参与贡献', link: '/project/contributing.zh-CN' },
  { text: '许可证与来源', link: '/project/third-party.zh-CN' },
]
const referenceEn = reference.map((item, index) => ({
  text: ['Source and build', 'Parameters and interfaces'][index],
  link: item.link.replace('.zh-CN', ''),
}))
const projectEn = project.map((item, index) => ({
  text: ['Changelog', 'Contributing', 'Licenses and origins'][index],
  link: item.link.replace('.zh-CN', ''),
}))
const sidebar = (english = false) => [
  { text: english ? 'Build and fly' : '制作与飞行', items: english ? guideEn : guide },
  { text: english ? 'Development' : '开发参考', items: english ? referenceEn : reference },
  { text: english ? 'About the project' : '关于项目', items: english ? projectEn : project },
]

export default defineConfig({
  lang: 'zh-CN',
  title: 'Open32Drone',
  description: '从 PCB 裸板和 3D 打印机架开始，亲手完成一架可以接入 ROS 2 与强化学习的开源无人机。',
  base: process.env.DOCS_BASE || '/',
  cleanUrls: true,
  lastUpdated: false,
  head: [
    ['meta', { name: 'theme-color', content: '#f4f1eb' }],
    ['meta', { name: 'color-scheme', content: 'light dark' }],
  ],
  markdown: {
    config(md) {
      // Git-relative source/download links remain usable in Git. On the website,
      // files outside docs link to the canonical repository instead of a missing route.
      md.core.ruler.after('inline', 'repository-links', state => {
        const visit = tokens => tokens.forEach(token => {
          if (token.type === 'link_open') {
            const href = token.attrGet('href')
            if (href && href.startsWith('../')) {
              const resolved = posix.normalize(posix.join('docs', posix.dirname(state.env.relativePath), href))
              if (!resolved.startsWith('docs/')) {
                const sourcePath = resolved.replace(/^software\//, '').replace(/^README_zh_CN\.md$/, 'README.zh-CN.md')
                token.attrSet('href', `https://github.com/npu-ius-lab/open32drone/blob/${process.env.DOCS_SOURCE_REF || 'featdemo'}/${sourcePath}`)
              }
            }
          }
          if (token.children) visit(token.children)
        })
        visit(state.tokens)
      })
    },
    theme: {
      light: 'github-light',
      dark: 'github-dark',
    },
  },
  themeConfig: {
    siteTitle: 'Open32Drone',
    nav: [
      { text: '项目', link: '/guide/01-project' },
      { text: '开始制作', link: '/guide/03-hardware' },
      { text: '固件与首飞', link: '/guide/04-firmware-flight' },
      { text: 'ROS 2', link: '/guide/06-ros' },
      { text: '强化学习', link: '/guide/07-rl' },
      { text: '开发与项目', items: [...reference, ...project] },
    ],
    sidebar: {
      // VitePress matches prefixes of page.relativePath (including .md).
      // Include the extension so an English path cannot shadow its .zh-CN sibling.
      ...Object.fromEntries(guideEn.map(x => [`${x.link}.md`, sidebar(true)])),
      ...Object.fromEntries([...referenceEn, ...projectEn].map(x => [`${x.link}.md`, sidebar(true)])),
      ...Object.fromEntries([...reference, ...project].map(x => [`${x.link}.md`, sidebar()])),
      '/guide/': sidebar(),
      '/reference/': sidebar(),
      '/project/': sidebar(),
    },
    outline: {
      label: '本页内容',
      level: [2, 3],
    },
    search: {
      provider: 'local',
      options: {
        translations: {
          button: {
            buttonText: '搜索',
            buttonAriaLabel: '搜索文档',
          },
          modal: {
            displayDetails: '显示详细结果',
            resetButtonTitle: '清除搜索',
            backButtonTitle: '关闭搜索',
            noResultsText: '没有找到相关内容：',
            footer: {
              selectText: '选择',
              navigateText: '切换结果',
              closeText: '关闭',
            },
          },
        },
      },
    },
    docFooter: {
      prev: '上一章',
      next: '下一章',
    },
    returnToTopLabel: '返回顶部',
    sidebarMenuLabel: '目录',
    darkModeSwitchLabel: '外观',
    lightModeSwitchTitle: '切换到浅色',
    darkModeSwitchTitle: '切换到深色',
    footer: {
      message: 'Open32Drone · 开放硬件、飞行控制、ROS 2 与强化学习',
    },
  },
})
