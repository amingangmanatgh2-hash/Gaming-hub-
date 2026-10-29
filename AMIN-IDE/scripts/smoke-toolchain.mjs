// Smoke test: run the real AVR toolchain (avr-gcc-wasm) in Node against a static server.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const PKG = 'node_modules/@horang-corp/avr-gcc-wasm';
const PORT = 8399;

const MIME = { '.json': 'application/json', '.wasm': 'application/wasm', '.mjs': 'text/javascript', '.js': 'text/javascript', '.h': 'text/plain', '.cpp': 'text/plain', '.o': 'application/octet-stream', '.a': 'application/octet-stream', '.xn': 'text/plain' };

const PKGABS = path.resolve(PKG);
const server = http.createServer((req, res) => {
  let p = decodeURIComponent(req.url.split('?')[0]);
  if (p === '/') p = '/index.js';
  const file = path.join(PKGABS, p);
  if (!file.startsWith(PKGABS) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404); res.end('nf'); return;
  }
  res.writeHead(200, { 'content-type': MIME[path.extname(file)] || 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});

const blink = `#include <Arduino.h>
// تست Blink — چراغ داخلی Uno را می‌سوزد
int led = LED_BUILTIN;
void setup() {
  pinMode(led, OUTPUT);
}
void loop() {
  digitalWrite(led, HIGH);
  delay(500);
  digitalWrite(led, LOW);
  delay(500);
}
`;

server.listen(PORT, '127.0.0.1', async () => {
  const { buildFirmware } = await import(new URL(`../${PKG}/firmware-builder.js`, import.meta.url).href);
  try {
    const t0 = Date.now();
    const r = await buildFirmware({ source: blink, assetsBase: `http://127.0.0.1:${PORT}/` });
    console.log('=== SUCCESS ===');
    console.log('hex lines:', r.hex.split('\n').length, '| flashBytes:', r.flashBytes, '| fits:', r.fitsTarget, '| totalMs:', Math.round(r.timings.totalMs));
    console.log('hex head:', r.hex.split('\n').slice(0, 3).join(' | '));
    console.log('objects:', r.objectCount, '| stderr lines:', r.stderr.length);
    if (r.stderr.length) console.log('stderr sample:', r.stderr.slice(0, 3).join('\n'));
    // negative test: bad code must throw
    try {
      await buildFirmware({ source: 'void setup() { this is not code } void loop(){}', assetsBase: `http://127.0.0.1:${PORT}/` });
      console.log('!!! BAD CODE COMPILED — WRONG');
    } catch (e) {
      console.log('=== NEGATIVE TEST OK === error msg head:', String(e.message).split('\n')[0].slice(0, 120));
    }
  } catch (e) {
    console.error('=== FAILED ===');
    console.error(String(e.message || e).slice(0, 800));
    console.error((e.stderr || []).join('\n').slice(0, 400));
    process.exitCode = 1;
  } finally {
    server.close();
  }
});
