// Validates the ESBUILD-BUNDLED compiler worker (dist/compiler-worker.js)
// against a served dist directory — the exact code path the browser runs.
// Compiles EVERY bundled example (including Persian strings) + a negative case.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, '..');
const dist = path.join(root, 'dist');
const PORT = 8399;

const MIME = {
  '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json', '.wasm': 'application/wasm', '.h': 'text/plain',
  '.cpp': 'text/plain', '.o': 'application/octet-stream', '.a': 'application/octet-stream',
  '.xn': 'text/plain', '.css': 'text/css', '.html': 'text/html',
};

const server = http.createServer((req, res) => {
  const p = decodeURIComponent((req.url || '/').split('?')[0]);
  const file = path.join(dist, p);
  if (!file.startsWith(dist) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404); res.end('nf'); return;
  }
  res.writeHead(200, { 'content-type': MIME[path.extname(file)] || 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});

// extract examples from src/sketches.ts
function extractExamples() {
  const src = fs.readFileSync(path.join(root, 'src/sketches.ts'), 'utf8');
  const re = /id: '([a-z0-9-]+)'[\s\S]*?code: `([\s\S]*?)`,\n\s*\},/g;
  const out = [];
  let m;
  while ((m = re.exec(src)) !== null) out.push({ id: m[1], code: m[2] });
  return out;
}

const examples = extractExamples();
console.log(`found ${examples.length} examples to compile`);

server.listen(PORT, '127.0.0.1', async () => {
  const w = await import(new URL('../dist/compiler-worker.js', import.meta.url).href);
  w.configureAssetsBase(`http://127.0.0.1:${PORT}/vendor/avr-gcc-wasm/`);

  let fail = 0;
  for (const ex of examples) {
    if (ex.id === 'error-demo') {
      // must FAIL with parsed diagnostics
      try {
        await w.runCompile(ex.code, []);
        console.log(`  ✖ ${ex.id}: should have failed but compiled`);
        fail += 1;
      } catch (e) {
        const fmt = w.formatBuildError(e);
        console.log(`  ✔ ${ex.id}: correctly failed, parsed errors=${fmt.errors.length}`);
      }
      continue;
    }
    const t0 = Date.now();
    try {
      const r = await w.runCompile(ex.code, []);
      console.log(`  ✔ ${ex.id}: ${r.flashBytes} B flash, fits=${r.fitsTarget} (${Date.now() - t0}ms)`);
    } catch (e) {
      const fmt = w.formatBuildError(e);
      console.log(`  ✖ ${ex.id}: FAILED — ${fmt.message.split('\n')[0].slice(0, 140)}`);
      fail += 1;
    }
  }
  console.log(fail === 0 ? '\n=== ALL EXAMPLES PASSED ===' : `\n=== ${fail} FAILURES ===`);
  server.close();
  if (fail > 0) process.exitCode = 1;
});
