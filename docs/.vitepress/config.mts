import { defineConfig } from 'vitepress'
import { posix, resolve } from 'node:path'
import { readFileSync, mkdirSync, copyFileSync, writeFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const repository = process.env.DOCS_REPOSITORY || 'osrbot/osrdrone'
const releaseTag = process.env.DOCS_RELEASE_TAG || ''
if (!['osrbot/osrdrone', 'npu-ius-lab/open32drone'].includes(repository)) {
  throw new Error('Unsupported DOCS_REPOSITORY')
}
if (repository === 'npu-ius-lab/open32drone' && !releaseTag) {
  throw new Error('Public documentation requires DOCS_RELEASE_TAG for matching downloads')
}
// Reader-facing source and community links always lead to the public project.
// The build repository only selects where tagged download assets are hosted.
const repositoryUrl = 'https://github.com/npu-ius-lab/open32drone'
const base = process.env.DOCS_BASE || '/'
const releaseDir = fileURLToPath(new URL('../../releases/open32drone/', import.meta.url))
const downloadFiles = readFileSync(resolve(releaseDir, 'SHA256SUMS'), 'utf8').trim().split('\n').map(line => line.split(/\s+/)[1])
if (downloadFiles.length !== 4 || downloadFiles.some(name => !/^Open32Drone-\d{8}-\d{6}-(?:full\.bin|app\.bin|android\.apk|ros2\.tar\.gz)$/.test(name))) {
  throw new Error('Expected four timestamped Open32Drone downloads')
}
downloadFiles.push('SHA256SUMS')
const downloadUrl = (name: string) => releaseTag
  ? `https://github.com/${repository}/releases/download/${encodeURIComponent(releaseTag)}/${name}`
  : `${base}downloads/${name}`

const guide = [
  { text: '01 · 项目介绍', link: '/guide/01-project' },
  { text: '02 · 开始制作', link: '/guide/03-hardware' },
  { text: '03 · 刷写与首飞', link: '/guide/04-firmware-flight' },
  { text: '04 · 调参与排查', link: '/guide/05-tuning' },
  { text: '05 · ROS 2 控制', link: '/guide/06-ros' },
  { text: '06 · 仿真与强化学习', link: '/guide/07-rl' },
]

const guideEn = guide.map((item, index) => ({
  text: ['01 · Project', '02 · Start Building', '03 · First flight',
    '04 · Tuning and diagnosis', '05 · ROS 2', '06 · Simulation and RL'][index],
  link: `${item.link}.en`,
}))
const reference = [
  { text: '源码与编译', link: '/reference/source-build.zh-CN' },
  { text: '参数与接口', link: '/reference/firmware.zh-CN' },
]
const project = [
  { text: '参与贡献', link: '/project/contributing.zh-CN' },
  { text: '许可证与来源', link: '/project/third-party.zh-CN' },
]
const referenceEn = reference.map((item, index) => ({
  text: ['Source and build', 'Parameters and interfaces'][index],
  link: item.link.replace('.zh-CN', ''),
}))
const projectEn = [
  ...project.map((item, index) => ({
    text: ['Contributing', 'Licenses and origins'][index],
    link: item.link.replace('.zh-CN', ''),
  })),
]
const sidebar = (english = false) => [
  { text: english ? 'Build and fly' : '制作与飞行', items: english ? guideEn : guide },
  { text: english ? 'Development' : '开发参考', items: english ? referenceEn : reference },
  { text: english ? 'About the project' : '关于项目', items: english ? projectEn : project },
]

export default defineConfig({
  lang: 'zh-CN',
  title: 'Open32Drone',
  description: '从 PCB 裸板和 3D 打印机架开始，亲手完成一架可以接入 ROS 2 与强化学习的开源无人机。',
  base,
  cleanUrls: true,
  // These exact files are emitted by buildEnd and hash-checked by docs:check.
  // Keep normal dead-link validation enabled for all other routes.
  ignoreDeadLinks: [(link) => downloadFiles.some(name => link === `/downloads/${name}` || link === `${base}downloads/${name}`)],
  lastUpdated: false,
  async buildEnd(siteConfig) {
    // Preserve old bookmarks after merging preparation into the build chapter.
    for (const suffix of ['', '.en']) {
      const target = `${base}guide/03-hardware${suffix}#preparation`
      const title = suffix ? 'Preparation has moved' : '制作准备已合并到开始制作'
      const destination = resolve(siteConfig.outDir, 'guide')
      mkdirSync(destination, { recursive: true })
      writeFileSync(resolve(destination, `02-goals${suffix}.html`),
        `<!doctype html><html lang="${suffix ? 'en' : 'zh-CN'}"><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=${target}"><title>${title}</title><a href="${target}">${title}</a></html>`)
    }
    // Only the five public-facing artifacts are copied for local/private preview.
    // Public builds link to a pinned Release instead; internal build records stay local.
    if (!releaseTag) {
      const destination = resolve(siteConfig.outDir, 'downloads')
      mkdirSync(destination, { recursive: true })
      for (const name of downloadFiles) copyFileSync(resolve(releaseDir, name), resolve(destination, name))
    }
  },
  head: [
    ['link', { rel: 'icon', type: 'image/x-icon', sizes: '16x16 32x32 48x48', href: `${base}favicon.ico?v=1` }],
    ['link', { rel: 'icon', type: 'image/svg+xml', sizes: 'any', href: `${base}favicon.svg?v=1` }],
    ['meta', { name: 'theme-color', content: '#ffffff', media: '(prefers-color-scheme: light)' }],
    ['meta', { name: 'theme-color', content: '#1b1b1f', media: '(prefers-color-scheme: dark)' }],
    ['meta', { name: 'color-scheme', content: 'light dark' }],
  ],
  markdown: {
    config(md) {
      md.core.ruler.before('normalize', 'repository-content', state => {
        state.src = state.src.replaceAll('https://github.com/osrbot/osrdrone', repositoryUrl)
      })
      // Git-relative source/download links remain usable in Git. On the website,
      // files outside docs link to the canonical repository instead of a missing route.
      md.core.ruler.after('inline', 'repository-links', state => {
        const visit = tokens => tokens.forEach(token => {
          if (token.type === 'link_open') {
            const href = token.attrGet('href')
            if (href && href.startsWith('../')) {
              const resolved = posix.normalize(posix.join('docs', posix.dirname(state.env.relativePath), href))
              if (resolved.startsWith('releases/open32drone/')) {
                const name = posix.basename(resolved)
                if (downloadFiles.includes(name)) {
                  token.attrSet('href', downloadUrl(name))
                  token.attrSet('download', name)
                }
                else if (name.startsWith('README')) token.attrSet('href', `${repositoryUrl}/releases`)
              } else if (!resolved.startsWith('docs/')) {
                token.attrSet('href', `${repositoryUrl}/blob/main/${resolved}`)
              }
            }
            // Private markdown remains readable on GitHub; the public build uses
            // its own repository for source, issue and contributor links.
            const target = token.attrGet('href')
            if (target?.startsWith('https://github.com/osrbot/osrdrone')) {
              token.attrSet('href', target.replace('https://github.com/osrbot/osrdrone', repositoryUrl))
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
    logoLink: `${base}guide/01-project`,
    nav: [
      { text: '教程', link: '/guide/01-project', activeMatch: '/guide/' },
      { text: '参考', items: reference, activeMatch: '/reference/' },
      { text: '下载', link: `${repositoryUrl}/releases` },
    ],
    socialLinks: [{ icon: 'github', link: repositoryUrl, ariaLabel: 'GitHub' }],
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
      message: 'Open32Drone 文档',
    },
  },
})
