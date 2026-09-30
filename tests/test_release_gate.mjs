import { test } from 'node:test'
import assert from 'node:assert/strict'
import { createHash } from 'node:crypto'
import { verifyPublicRelease } from './verify_public_release.mjs'

const names = ['full.bin', 'app.bin', 'android.apk', 'ros2.tar.gz'].map(n => `Open32Drone-20000101-000000-${n}`)
const payload = 'test release bytes'
const digest = createHash('sha256').update(payload).digest('hex')
const sums = names.map(n => `${digest}  ${n}`).join('\n') + '\n'
const request = (options = {}) => async url => {
  if (url.includes('api.github.com')) return new Response(JSON.stringify({
    tag_name: 'test-only', draft: options.draft || false,
    assets: [...names.slice(options.missing ? 1 : 0), 'SHA256SUMS'].map(name => ({ name })),
  }), { status: options.notFound ? 404 : 200 })
  return new Response(url.endsWith('SHA256SUMS') ? (options.manifest ?? sums) : options.corrupt ? 'wrong bytes' : payload)
}
test('accepts a complete, matching Release', async () => {
  assert.deepEqual(await verifyPublicRelease({ tag: 'test-only', sums, request: request() }), names)
})

test('accepts LF and CRLF manifests on either side', async () => {
  const windowsSums = sums.replaceAll('\n', '\r\n')
  for (const local of [sums, windowsSums]) {
    for (const remote of [sums, windowsSums]) {
      assert.deepEqual(await verifyPublicRelease({
        tag: 'test-only', sums: local, request: request({ manifest: remote }),
      }), names)
    }
  }
})

test('rejects changed hashes and filenames even with CRLF line endings', async () => {
  for (const manifest of [
    sums.replace(digest, '0'.repeat(64)),
    sums.replace(names[0], 'different-full.bin'),
  ]) {
    await assert.rejects(verifyPublicRelease({
      tag: 'test-only', sums, request: request({ manifest: manifest.replaceAll('\n', '\r\n') }),
    }), /SHA256SUMS differs/)
  }
})
test('rejects absent or draft Releases', async () => {
  for (const options of [{ notFound: true }, { draft: true }]) {
    await assert.rejects(verifyPublicRelease({ tag: 'test-only', sums, request: request(options) }))
  }
})
test('rejects missing or corrupted assets', async () => {
  for (const options of [{ missing: true }, { corrupt: true }]) {
    await assert.rejects(verifyPublicRelease({ tag: 'test-only', sums, request: request(options) }))
  }
})
test('rejects unspecified tags and invalid manifests before requesting', async () => {
  const never = () => { throw new Error('must not request') }
  await assert.rejects(verifyPublicRelease({ tag: '', sums, request: never }), /tag is required/)
  await assert.rejects(verifyPublicRelease({ tag: 'test', sums: 'bad', request: never }), /Invalid/)
})
