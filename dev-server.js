/**
 * ============================================================
 *  Local / Sandbox preview server for worker.js
 *  شبیه‌ساز Cloudflare Workers: KV در حافظه + fetch handler
 *  اجرا:  node dev-server.js   (پورت 3000)
 *  ⚠️ فقط برای پیش‌نمایش — در پروداکشن از `wrangler deploy` استفاده کنید.
 * ============================================================
 */
import http from 'node:http';
import worker from './worker.js';

/* ---------- In-memory KV shim (سازگار با API کلادفلر) ---------- */
class MemoryKV {
  constructor() { this.map = new Map(); }
  async get(key) { return this.map.has(key) ? this.map.get(key) : null; }
  async put(key, value) { this.map.set(key, String(value)); }
  async delete(key) { this.map.delete(key); }
  async list({ prefix = '' } = {}) {
    return { keys: [...this.map.keys()].filter(k => k.startsWith(prefix)).map(name => ({ name })) };
  }
}

const env = { KV: new MemoryKV() }; // DB (D1) عمداً تعریف نشده — کد آن را اختیاری هندل می‌کند
const ctx = { waitUntil: p => Promise.resolve(p).catch(e => console.error('waitUntil:', e)) };

const PORT = process.env.PORT || 3000;

const server = http.createServer(async (req, res) => {
  try {
    const proto = req.headers['x-forwarded-proto'] || 'http';
    const host = req.headers.host || `localhost:${PORT}`;
    const url = `${proto}://${host}${req.url}`;

    const chunks = [];
    for await (const c of req) chunks.push(c);
    const body = Buffer.concat(chunks);

    const request = new Request(url, {
      method: req.method,
      headers: req.headers,
      body: ['GET', 'HEAD'].includes(req.method) ? undefined : body,
    });

    const response = await worker.fetch(request, env, ctx);

    res.writeHead(response.status, Object.fromEntries(response.headers));
    const buf = Buffer.from(await response.arrayBuffer());
    res.end(buf);
  } catch (e) {
    console.error(e);
    res.writeHead(500, { 'content-type': 'text/plain; charset=utf-8' });
    res.end('Internal Error: ' + e.message);
  }
});

server.listen(PORT, '0.0.0.0', () => {
  console.log(`🤖 Self-Bot Hub preview  →  http://0.0.0.0:${PORT}`);
  console.log('   /        فرم راه‌اندازی (Setup Landing Page)');
  console.log('   /panel   پنل مدیریت وب');
});
