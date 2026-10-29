// AMIN IDE production build.
// - bundles src (esbuild)
// - copies the offline AVR toolchain vendor files into dist/vendor
// - copies index.html + favicon
import esbuild from 'esbuild';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));
const dist = path.join(root, 'dist');
const pkg = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
const vendorSrc = path.join(root, 'node_modules', '@horang-corp', 'avr-gcc-wasm');
const vendorDist = path.join(dist, 'vendor', 'avr-gcc-wasm');

function copyDir(from, to) {
  fs.mkdirSync(to, { recursive: true });
  for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
    const src = path.join(from, entry.name);
    const dst = path.join(to, entry.name);
    if (entry.isDirectory()) copyDir(src, dst);
    else fs.copyFileSync(src, dst);
  }
}

fs.rmSync(dist, { recursive: true, force: true });
fs.mkdirSync(dist, { recursive: true });

await esbuild.build({
  entryPoints: [path.join(root, 'src/main.ts'), path.join(root, 'src/compiler-worker.ts')],
  bundle: true,
  minify: true,
  format: 'esm',
  target: 'es2022',
  outdir: dist,
  entryNames: '[name]',
  chunkNames: '[name]-[hash]',
  logLevel: 'info',
  metafile: true,
});

// offline toolchain: emscripten WASM tools + sysroot assets (headers, objects, libs)
copyDir(path.join(vendorSrc, 'assets'), path.join(vendorDist, 'assets'));
copyDir(path.join(vendorSrc, 'tools'), path.join(vendorDist, 'tools'));

fs.copyFileSync(path.join(root, 'index.html'), path.join(dist, 'index.html'));
fs.copyFileSync(path.join(root, 'public/favicon.svg'), path.join(dist, 'favicon.svg'));

const sizeMb = (fs.statSync(dist) && 0) || 0; // noop keep
console.log(`\n✔ AMIN IDE ${pkg.version} built → dist/ (offline toolchain bundled)`);
console.log(`  ${fs.readdirSync(dist).length} top-level outputs`);
