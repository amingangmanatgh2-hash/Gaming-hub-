// AMIN IDE dev server — esbuild watch + static file server (no external deps).
// Serves the built app locally so everything (including the WASM toolchain)
// is loaded from the same origin — exactly like the offline release build.
import esbuild from 'esbuild';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));
const dist = path.join(root, 'dist');
const vendorSrc = path.join(root, 'node_modules', '@horang-corp', 'avr-gcc-wasm');
const vendorDist = path.join(dist, 'vendor', 'avr-gcc-wasm');
const PORT = Number(process.env.PORT || 5173);
const HOST = '0.0.0.0';

function copyDir(from, to) {
  fs.mkdirSync(to, { recursive: true });
  for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
    const src = path.join(from, entry.name);
    const dst = path.join(to, entry.name);
    if (entry.isDirectory()) copyDir(src, dst);
    else fs.copyFileSync(src, dst);
  }
}

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.wasm': 'application/wasm',
  '.ico': 'image/x-icon',
  '.o': 'application/octet-stream',
  '.a': 'application/octet-stream',
  '.xn': 'text/plain',
  '.h': 'text/plain',
  '.cpp': 'text/plain',
};

if (!fs.existsSync(path.join(vendorDist, 'manifest.json'))) {
  copyDir(path.join(vendorSrc, 'assets'), path.join(vendorDist, 'assets'));
  copyDir(path.join(vendorSrc, 'tools'), path.join(vendorDist, 'tools'));
  console.log('✔ offline toolchain copied to dist/vendor');
}

const ctx = await esbuild.context({
  entryPoints: [path.join(root, 'src/main.ts'), path.join(root, 'src/compiler-worker.ts')],
  bundle: true,
  minify: false,
  format: 'esm',
  target: 'es2022',
  outdir: dist,
  entryNames: '[name]',
  chunkNames: '[name]-[hash]',
  logLevel: 'info',
});

await ctx.watch();

const server = http.createServer((req, res) => {
  let p = decodeURIComponent((req.url || '/').split('?')[0]);
  if (p === '/') p = '/index.html';
  const file = path.join(dist, p);
  if (!file.startsWith(dist) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404, { 'content-type': 'text/plain' });
    res.end('404');
    return;
  }
  res.writeHead(200, { 'content-type': MIME[path.extname(file)] || 'application/octet-stream', 'cache-control': 'no-cache' });
  fs.createReadStream(file).pipe(res);
});

server.listen(PORT, HOST, () => {
  console.log(`\n🛠  AMIN IDE dev → http://localhost:${PORT}  (serving ${dist})`);
});
