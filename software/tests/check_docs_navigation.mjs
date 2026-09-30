// Run after docs:build. Check rendered navigation, not just config strings.
import assert from 'node:assert/strict'
import { readFileSync, readdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const docs = new URL('../../docs/', import.meta.url)
const dist = new URL('.vitepress/dist/', docs)
const base = process.env.DOCS_BASE || '/'
let checked = 0
for (const directory of ['guide', 'reference', 'project']) {
  for (const file of readdirSync(new URL(`${directory}/`, docs)).filter(x => x.endsWith('.md'))) {
    const route = `${directory}/${file.slice(0, -3)}`
    const english = directory === 'guide' ? route.endsWith('.en') : !route.endsWith('.zh-CN')
    const counterpart = directory === 'guide'
      ? (english ? route.slice(0, -3) : `${route}.en`)
      : (english ? `${route}.zh-CN` : route.slice(0, -6))
    const html = readFileSync(new URL(`${route}.html`, dist), 'utf8')
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
    assert.equal(links.length, 12, `${route}: incomplete sidebar`)
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
assert.equal(checked, 24)
const home = readFileSync(new URL('index.html', dist), 'utf8')
assert(/<details[^>]*class="language-switch"/.test(home), 'home: missing native language menu')
assert(/<summary[^>]*aria-controls="site-language-options"/.test(home), 'home: missing clickable native trigger')
assert(home.includes(`href="${base}guide/01-project.en"`), 'home: missing English destination')
console.log(`Navigation passed: ${checked} bilingual pages, same-language sidebars and reciprocal switches (${fileURLToPath(dist)}).`)
