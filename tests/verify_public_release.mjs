// Publication gate, separate from offline documentation builds.
import { readFileSync } from 'node:fs'
import { createHash } from 'node:crypto'
import { pathToFileURL } from 'node:url'

export async function verifyPublicRelease({ tag, sums, request = fetch }) {
  if (!tag?.trim()) throw new Error('An existing public Release tag is required')
  const repository = 'npu-ius-lab/open32drone'
  const entries = sums.trim().split('\n').map(line => {
    const match = /^([0-9a-f]{64})\s+(Open32Drone-\d{8}-\d{6}-(?:full\.bin|app\.bin|android\.apk|ros2\.tar\.gz))$/.exec(line)
    if (!match) throw new Error('Invalid SHA256SUMS entry')
    return { digest: match[1], name: match[2] }
  })
  if (entries.length !== 4 || new Set(entries.map(e => e.name)).size !== 4) {
    throw new Error('Expected four distinct matching downloads')
  }
  const get = async url => {
    const response = await request(url, { signal: AbortSignal.timeout(30000) })
    if (!response.ok) throw new Error(`Public Release request failed: HTTP ${response.status}`)
    return response
  }
  const release = await (await get(`https://api.github.com/repos/${repository}/releases/tags/${encodeURIComponent(tag)}`)).json()
  if (release.draft || release.tag_name !== tag) throw new Error('Release is draft or tag does not match')
  const assets = new Set(release.assets?.map(asset => asset.name))
  for (const name of [...entries.map(e => e.name), 'SHA256SUMS']) {
    if (!assets.has(name)) throw new Error(`Release asset missing: ${name}`)
  }
  const prefix = `https://github.com/${repository}/releases/download/${encodeURIComponent(tag)}/`
  const remoteSums = await (await get(`${prefix}SHA256SUMS`)).text()
  if (remoteSums.trim() !== sums.trim()) throw new Error('Release SHA256SUMS differs from this source checkout')
  await Promise.all(entries.map(async ({ name, digest }) => {
    const bytes = await (await get(prefix + name)).arrayBuffer()
    if (createHash('sha256').update(Buffer.from(bytes)).digest('hex') !== digest) {
      throw new Error(`Release content hash mismatch: ${name}`)
    }
  }))
  return entries.map(e => e.name)
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const sums = readFileSync(new URL('../releases/open32drone/SHA256SUMS', import.meta.url), 'utf8')
  const files = await verifyPublicRelease({ tag: process.env.DOCS_RELEASE_TAG, sums })
  console.log(`Public Release verified: ${files.length} files and SHA256SUMS`)
}
