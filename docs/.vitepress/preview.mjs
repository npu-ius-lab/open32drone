import { preview } from 'vite'
import { createReadStream, existsSync, readFileSync, statSync } from 'node:fs'
import { resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const args = process.argv.slice(2)
const option = (name, fallback) => args.includes(name) ? args[args.indexOf(name) + 1] : fallback
const root = fileURLToPath(new URL('../', import.meta.url))
const outDir = resolve(root, '.vitepress/dist')
const base = process.env.DOCS_BASE || '/'
const manifest = resolve(outDir, 'downloads/SHA256SUMS')
const downloads = existsSync(manifest)
  ? [...readFileSync(manifest, 'utf8').trim().split('\n').map(line => line.split(/\s+/)[1]), 'SHA256SUMS']
  : []

const server = await preview({
  configFile: false,
  root,
  base,
  build: { outDir },
  preview: { host: option('--host', '127.0.0.1'), port: Number(option('--port', '5173')), strictPort: true },
  plugins: [{
    name: 'unaltered-downloads',
    configurePreviewServer(server) {
      // A .tar.gz attachment is a gzip file, not HTTP Content-Encoding: gzip.
      // Handle exact manifest entries before the static server can decompress it.
      server.middlewares.use((req, res, next) => {
        const pathname = new URL(req.url || '/', 'http://localhost').pathname
        const name = downloads.find(name => pathname === `${base}downloads/${name}`)
        if (!name) return next()
        if (req.method !== 'GET' && req.method !== 'HEAD') {
          res.writeHead(405, { Allow: 'GET, HEAD' }).end()
          return
        }
        const path = resolve(outDir, 'downloads', name)
        res.writeHead(200, {
          'Content-Type': 'application/octet-stream',
          'Content-Disposition': `attachment; filename="${name}"`,
          'Content-Length': statSync(path).size,
          'Cache-Control': 'no-store',
          'X-Content-Type-Options': 'nosniff',
        })
        if (req.method === 'HEAD') res.end()
        else createReadStream(path).on('error', error => res.destroy(error)).pipe(res)
      })
    },
  }],
})
server.printUrls()
