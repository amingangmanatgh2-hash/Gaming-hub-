// Compiler worker — runs the real AVR toolchain (@horang-corp/avr-gcc-wasm,
// bundled into this file by esbuild) off the main thread.
import { buildFirmware, setAssetsBase } from '@horang-corp/avr-gcc-wasm';

const DEFAULT_ASSETS_BASE = new URL('vendor/avr-gcc-wasm/', import.meta.url.replace(/[^/]*$/, '')).href;

let assetsBaseOverride: string | null = null;

/** Test/ops hook — lets Node smoke tests point at a served asset dir. */
export function configureAssetsBase(url: string): void {
  assetsBaseOverride = url;
  setAssetsBase(url);
}

export interface CompileSuccess {
  hex: string;
  flashBytes: number;
  appFlashBytes: number;
  fitsTarget: boolean;
  timings: Record<string, number>;
  totalMs: number;
}

export interface CompileError {
  message: string;       // cleaned, multi-line
  errors: { line: number; col: number; text: string }[];
}

// The Arduino toolchain preprocessor normally prepends <Arduino.h> to every
// sketch; we do it here for the single-file build.
function prependArduino(src: string): string {
  return `#include <Arduino.h>\n\n${src}`;
}

// Map a reported line inside HorangFirmware.cpp back to the user's sketch line.
const LINE_OFFSET = 2;

export function formatBuildError(err: unknown): CompileError {
  const raw = err instanceof Error ? err.message : String(err);
  const errors: { line: number; col: number; text: string }[] = [];
  const lines: string[] = [];

  const re = /HorangFirmware\.cpp:(\d+):(\d+):\s*(error|warning):\s*(.*)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    const line = Number(m[1]) - LINE_OFFSET;
    if (line > 0) errors.push({ line, col: Number(m[2]), text: m[4].trim() });
    lines.push(`sketch:${line}:${m[2]}: ${m[3]}: ${m[4]}`);
  }

  if (lines.length === 0) {
    // fallback: keep the raw message but clean it up
    const cleaned = raw
      .replace(/\[object Object\][^\n]*/g, '')
      .replace(/\[cc1plus\]\s*/g, '')
      .replace(/\[avr-as\]\s*/g, '')
      .replace(/\[avr-ld\]\s*/g, '')
      .replace(/\[avr-objcopy\]\s*/g, '')
      .trim();
    lines.push(cleaned || 'خطای نامشخص در کامپایل');
  }

  return { message: lines.join('\n'), errors };
}

export async function runCompile(
  source: string,
  sensors: string[],
  base?: string,
): Promise<CompileSuccess> {
  if (base) configureAssetsBase(base);
  const result = await buildFirmware({
    source: prependArduino(source),
    sensors,
    assetsBase: assetsBaseOverride || DEFAULT_ASSETS_BASE,
  });
  return {
    hex: result.hex,
    flashBytes: result.flashBytes,
    appFlashBytes: result.target.appFlashBytes,
    fitsTarget: result.fitsTarget,
    timings: result.timings,
    totalMs: result.timings.totalMs,
  };
}

interface Msg {
  type: 'compile' | 'ping';
  id?: number;
  source?: string;
  sensors?: string[];
}

if (typeof self !== 'undefined' && self.addEventListener) {
  self.addEventListener('message', async (ev: MessageEvent<Msg>) => {
    const msg = ev.data;
    if (msg.type === 'ping') {
      (self as unknown as Worker).postMessage({ type: 'pong' });
      return;
    }
    if (msg.type === 'compile') {
      const id = msg.id as number;
      try {
        const result = await runCompile(msg.source || '', msg.sensors || []);
        (self as unknown as Worker).postMessage({ id, ok: true, result });
      } catch (err) {
        const formatted = formatBuildError(err);
        (self as unknown as Worker).postMessage({ id, ok: false, error: formatted });
      }
    }
  });
}
