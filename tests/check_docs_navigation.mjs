// Run after docs:build. Check rendered navigation, not just config strings.
import assert from 'node:assert/strict'
import { readFileSync, readdirSync, existsSync } from 'node:fs'
import { fileURLToPath, pathToFileURL } from 'node:url'
import { resolve } from 'node:path'
import { createHash } from 'node:crypto'

const docs = new URL('../docs/', import.meta.url)
const dist = process.argv[2] ? pathToFileURL(`${resolve(process.argv[2])}/`) : new URL('.vitepress/dist/', docs)
const base = process.env.DOCS_BASE || '/'
const checkIcons = (html, route) => {
  for (const [name, type] of [['favicon.ico', 'image/x-icon'], ['favicon.svg', 'image/svg+xml']]) {
    const tag = [...html.matchAll(/<link\b[^>]*>/g)].map(match => match[0])
      .find(tag => tag.includes('rel="icon"') && tag.includes(`type="${type}"`))
    assert(tag?.includes(`href="${base}${name}?v=1"`), `${route}: missing or incorrect ${name} path`)
    assert.deepEqual(readFileSync(new URL(name, dist)), readFileSync(new URL(`public/${name}`, docs)),
      `${route}: ${name} differs from its source`)
  }
}
let checked = 0
for (const directory of ['guide', 'reference', 'project']) {
  for (const file of readdirSync(new URL(`${directory}/`, docs)).filter(x => x.endsWith('.md'))) {
    const route = `${directory}/${file.slice(0, -3)}`
    const english = directory === 'guide' ? route.endsWith('.en') : !route.endsWith('.zh-CN')
    const counterpart = directory === 'guide'
      ? (english ? route.slice(0, -3) : `${route}.en`)
      : (english ? `${route}.zh-CN` : route.slice(0, -6))
    const html = readFileSync(new URL(`${route}.html`, dist), 'utf8')
    checkIcons(html, route)
    if (route.startsWith('project/changelog')) {
      const target = 'https://github.com/npu-ius-lab/open32drone/releases'
      assert(html.includes('http-equiv="refresh"'), `${route}: missing legacy redirect`)
      assert(html.includes(`content="0;url=${target}"`), `${route}: incorrect redirect destination`)
      assert(html.includes(`href="${target}"`), `${route}: missing redirect fallback link`)
      assert(!html.includes('v0.1.'), `${route}: old local version records must not be published`)
      continue
    }
    const siteTitle = html.match(/<a[^>]*class="title"[^>]*>/)?.[0]
    assert(siteTitle?.includes(`href="${base}guide/01-project${english ? '.en' : ''}"`),
      `${route}: site title must open the tutorial in the current language`)
    const topnav = html.match(/<nav[^>]*class="VPNavBarMenu[^\"]*"[\s\S]*?<\/nav>/)?.[0]
    assert(topnav, `${route}: missing top navigation`)
    assert(!html.includes('/project/downloads'), `${route}: link to removed download page`)
    assert(topnav.includes('href="https://github.com/npu-ius-lab/open32drone/releases"'),
      `${route}: Download navigation must link to Releases`)
    for (const [, href] of topnav.matchAll(/href="([^"]+)"/g)) {
      if (!href.startsWith(base) || href.startsWith('https:')) continue
      const target = href.slice(base.length)
      if (!/^(guide|reference|project)\//.test(target)) continue
      const targetEnglish = target.startsWith('guide/') ? target.endsWith('.en') : !target.endsWith('.zh-CN')
      assert.equal(targetEnglish, english, `${route}: top navigation changes language at ${href}`)
    }
    assert(html.includes(english ? 'Search documentation' : '搜索文档'), `${route}: wrong search language`)
    assert(html.includes(english ? 'On this page' : '本页内容'), `${route}: wrong outline language`)
    for (const [, image] of html.matchAll(/<img[^>]+src="([^"]*\/media\/figures\/[^"]+\.svg)"/g)) {
      assert(image.startsWith(base), `${route}: wrong diagram base ${image}`)
      readFileSync(new URL(image.slice(base.length), dist))
    }
    assert(!html.includes('language-mermaid'), `${route}: unrendered diagram source`)
    if (directory === 'guide') {
      const source = readFileSync(new URL(`${directory}/${file}`, docs), 'utf8')
      assert(!/^\[(?:上一章|下一章|Previous|Next)\]\(/m.test(source), `${route}: duplicate handwritten chapter navigation`)
      assert(html.includes('pager-link'), `${route}: missing chapter navigation cards`)
    }
    if (route === 'guide/03-hardware' || route === 'guide/03-hardware.en') {
      const table = html.match(/<div class="purchase-table">([\s\S]*?)<\/div>/)?.[1]
      assert(table, `${route}: missing purchase table`)
      const rows = [...table.matchAll(/<tr>([\s\S]*?)<\/tr>/g)].slice(1)
      assert.equal(rows.length, 16, `${route}: incomplete purchase list`)
      rows.forEach((row, index) => {
        const cells = [...row[1].matchAll(/<td[^>]*>([\s\S]*?)<\/td>/g)]
        assert.equal(cells.length, 5, `${route}: purchase table must have five columns`)
        assert.equal(cells[0][1], String(index + 1), `${route}: incorrect row number`)
        if (index === 15) {
          assert.equal(cells[3][1], '', `${route}: optional receiver quantity must be blank`)
        } else {
          assert(/^\d+$/.test(cells[3][1]), `${route}: quantities must be numeric only`)
        }
        assert(cells[3][0].includes('text-align:center'), `${route}: quantity must be centered`)
      })
      assert(!/<details[^>]*>(?:(?!<\/details>)[\s\S])*?class="purchase-grid"/.test(html), `${route}: component photos must be expanded`)
      assert(readFileSync(new URL(`${directory}/${file}`, docs), 'utf8').includes('outline: [2, 3]'), `${route}: chapter outline must include subsections`)
      assert.equal((html.match(/class="purchase-card"/g) || []).length, 12, `${route}: incomplete component gallery`)
      const images = [...html.matchAll(/<img[^>]+src="([^"]*\/media\/purchasing\/[^"]+)"/g)].map(x => x[1])
      assert.equal(images.length, 12, `${route}: missing component images`)
      for (const image of images) {
        assert(image.startsWith(base), `${route}: incorrect image base ${image}`)
        readFileSync(new URL(image.slice(base.length), dist))
      }
    }
    const sidebar = html.match(/<nav[^>]*id="VPSidebarNav"[\s\S]*?<\/nav>/)?.[0]
    assert(sidebar, `${route}: missing sidebar`)
    const links = [...sidebar.matchAll(/href="([^"]+)"/g)].map(x => x[1])
    assert.equal(links.length, 10, `${route}: incomplete sidebar`)
    assert(![...links, ...[...topnav.matchAll(/href="([^"]+)"/g)].map(x => x[1])]
      .some(href => href.includes('/project/changelog')), `${route}: duplicate changelog navigation`)
    for (const href of links) {
      assert(href.startsWith(base), `${route}: wrong base ${href}`)
      const target = href.slice(base.length)
      const targetEnglish = target.startsWith('guide/')
        ? target.endsWith('.en') : !target.endsWith('.zh-CN')
      assert.equal(targetEnglish, english, `${route}: mixed-language link ${href}`)
      readFileSync(new URL(`${target}.html`, dist)) // target must exist
    }
    const source = readFileSync(new URL(`${directory}/${file}`, docs), 'utf8')
    assert(!/^\[English\].*\[简体中文\]/m.test(source), `${route}: duplicate body language switch`)
    assert.equal((html.match(/id="site-language-options"/g) || []).length, 1, `${route}: language menu must be unique`)
    const menu = html.match(/<nav[^>]*id="site-language-options"[\s\S]*?<\/nav>/)?.[0]
    assert(menu?.includes(`href="${base}${counterpart}"`), `${route}: wrong language destination`)
    assert(menu.includes(`href="${base}${route}"`), `${route}: missing current language`)
    assert(menu.includes('简体中文') && menu.includes('English'), `${route}: missing language choices`)
    assert(/<details[^>]*class="language-switch"/.test(html), `${route}: language menu must work before hydration`)
    assert(/<summary[^>]*aria-controls="site-language-options"/.test(html), `${route}: missing native language trigger`)
    checked++
  }
}
assert.equal(checked, 20)
for (const suffix of ['', '.zh-CN']) {
  assert(!existsSync(new URL(`project/downloads${suffix}.md`, docs)), 'Download pages must be removed')
  assert(!existsSync(new URL(`project/downloads${suffix}.html`, dist)), 'Build must not retain deleted download pages')
}
const releaseTag = process.env.DOCS_RELEASE_TAG || ''
const sums = readFileSync(new URL('../releases/open32drone/SHA256SUMS', import.meta.url), 'utf8').trim().split('\n')
for (const line of sums) {
  const [digest, name] = line.split(/\s+/)
  if (!releaseTag) {
    assert.equal(createHash('sha256').update(readFileSync(new URL(`downloads/${name}`, dist))).digest('hex'), digest)
  }
}
for (const suffix of ['', '.zh-CN']) {
  const source = readFileSync(new URL(`reference/source-build${suffix}.md`, docs), 'utf8')
  for (const internal of ['私有主线', '本次批准', '获准交付', 'BUILD_INFO.md', '单独授权',
    'private mainline', 'approved deliverables', 'separate authorization', 'Hardware validation ladder']) {
    assert(!source.includes(internal), `source-build: internal maintenance text ${internal}`)
  }
}
for (const suffix of ['', '.en']) {
  const tuning = readFileSync(new URL(`guide/05-tuning${suffix}.md`, docs), 'utf8')
  assert.deepEqual([...tuning.matchAll(/^## 4\.(\d) /gm)].map(match => Number(match[1])),
    [1, 2, 3, 4, 5, 6, 7, 8], 'tuning: keep the same teaching sequence in both languages')
  assert(!tuning.includes('## The five evidence layers') && !tuning.includes('## Collect a basic evidence bundle'),
    'tuning: use reader-facing diagnostic headings')
  const rl = readFileSync(new URL(`guide/07-rl${suffix}.md`, docs), 'utf8')
  assert(!rl.includes('性能验收记录') && !rl.includes('performance acceptance'),
    'simulation: describe examples without internal acceptance terminology')
}
// Local previews must not send readers to private source or community pages.
{
  for (const directory of ['guide', 'reference', 'project']) {
    for (const name of readdirSync(new URL(`${directory}/`, dist)).filter(n => n.endsWith('.html'))) {
      const html = readFileSync(new URL(`${directory}/${name}`, dist), 'utf8')
      const hrefs = [...html.matchAll(/href="([^"]+)"/g)].map(match => match[1])
      assert(!hrefs.some(href => href.startsWith('https://github.com/osrbot/osrdrone') && !href.includes('/releases/download/')), `${name}: private source/community link`)
    }
  }
}
for (const suffix of ['', '.en']) {
  const redirect = readFileSync(new URL(`guide/02-goals${suffix}.html`, dist), 'utf8')
  assert(redirect.includes(`${base}guide/03-hardware${suffix}#preparation`), 'preparation bookmark must redirect')
  const hardware = readFileSync(new URL(`guide/03-hardware${suffix}.html`, dist), 'utf8')
  assert(hardware.includes(`${base}media/figures/motor-layout${suffix}.svg`), 'motor diagram missing')
  const flight = readFileSync(new URL(`guide/04-firmware-flight${suffix}.html`, dist), 'utf8')
  assert(flight.indexOf('id="android-first-flight"') < flight.indexOf('id="sbus"'), 'APK must precede optional RC')
}
// Check fragment destinations too: VitePress only checks that the page exists.
let fragments = 0
for (const directory of ['guide', 'reference', 'project']) {
  for (const name of readdirSync(new URL(`${directory}/`, dist)).filter(n => n.endsWith('.html'))) {
    const html = readFileSync(new URL(`${directory}/${name}`, dist), 'utf8')
    for (const [, href] of html.matchAll(/href="([^"]+)"/g)) {
      if (!href.includes('#') || /^(?:https?:|mailto:)/.test(href)) continue
      const url = new URL(href, `https://docs.invalid${base}${directory}/${name}`)
      if (!url.pathname.startsWith(base)) continue
      let target = url.pathname.slice(base.length)
      if (!target.endsWith('.html')) target += '.html'
      const file = new URL(target, dist)
      const id = decodeURIComponent(url.hash.slice(1))
      if (!id || !existsSync(file)) continue
      assert(readFileSync(file, 'utf8').includes(`id="${id}"`), `${name}: missing fragment ${href}`)
      fragments++
    }
  }
}
console.log(`Internal fragments passed: ${fragments}`)
const home = readFileSync(new URL('index.html', dist), 'utf8')
checkIcons(home, 'index')
assert(home.includes('http-equiv="refresh"') && home.includes('content="0;url=./guide/01-project"'),
  'site root must redirect to the tutorial without JavaScript')
const homeLinks = [...home.matchAll(/href="([^"]+)"/g)]
  .map(([, href]) => new URL(href, `https://docs.invalid${base}`).pathname)
assert(homeLinks.includes(`${base}guide/01-project`), 'site root: missing tutorial fallback link')
assert(!home.includes('project-hero'), 'site root must not render the removed landing page')
console.log(`Navigation passed: ${checked} documentation pages, same-language sidebars and language switches (${fileURLToPath(dist)}).`)
