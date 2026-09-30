// Verify actual HTTP attachment bytes: .tar.gz must not be HTTP-decoded.
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { readFileSync } from 'node:fs'
import { createHash } from 'node:crypto'
import { setTimeout as delay } from 'node:timers/promises'

if (process.env.DOCS_RELEASE_TAG) process.exit(0)
const port = 15173
const base = process.env.DOCS_BASE || '/'
const child = spawn(process.execPath, ['docs/.vitepress/preview.mjs', '--port', String(port)], { stdio: 'pipe' })
let logs = ''
child.stdout.on('data', chunk => { logs += chunk })
child.stderr.on('data', chunk => { logs += chunk })
try {
  const url = `http://127.0.0.1:${port}${base}downloads/`
  let response
  for (let attempt = 0; attempt < 50; attempt++) {
    assert(child.exitCode === null, logs)
    try { response = await fetch(`${url}SHA256SUMS`); break } catch { await delay(100) }
  }
  assert(response?.ok, `preview did not start: ${logs}`)
  const expected = readFileSync('releases/open32drone/SHA256SUMS', 'utf8')
  assert.equal(await response.text(), expected)
  for (const line of expected.trim().split('\n')) {
    const [hash, name] = line.split(/\s+/)
    const file = await fetch(`${url}${name}`)
    assert(file.ok, name)
    assert.equal(file.headers.get('content-encoding'), null, name)
    assert(file.headers.get('content-disposition')?.startsWith('attachment;'), name)
    assert.equal(createHash('sha256').update(Buffer.from(await file.arrayBuffer())).digest('hex'), hash, name)
  }
  for (const [name, type] of [['favicon.ico', 'image/x-icon'], ['favicon.svg', 'image/svg+xml']]) {
    const icon = await fetch(`http://127.0.0.1:${port}${base}${name}?v=1`)
    assert(icon.ok, `${name}: missing icon`)
    assert.equal(icon.headers.get('content-type')?.split(';')[0], type, name)
    assert.deepEqual(Buffer.from(await icon.arrayBuffer()), readFileSync(`docs/public/${name}`), name)
  }
  console.log(`HTTP favicon checks passed (${base}): SVG and ICO`)
  console.log(`HTTP download checks passed (${base}): manifest and four unchanged attachments`)
} finally {
  child.kill('SIGTERM')
}
