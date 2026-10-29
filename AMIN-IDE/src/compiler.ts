// Main-thread compiler wrapper — spawns the bundled worker, resolves per build.
import type { CompileError, CompileSuccess } from './compiler-worker';

export type CompileOutcome =
  | { ok: true; result: CompileSuccess }
  | { ok: false; error: CompileError };

let worker: Worker | null = null;
let seq = 0;
const pending = new Map<number, (o: CompileOutcome) => void>();

function ensureWorker(): Worker {
  if (!worker) {
    worker = new Worker(new URL('./compiler-worker.js', import.meta.url), { type: 'module' });
    worker.addEventListener('message', (ev: MessageEvent) => {
      const { id, ok, result, error } = ev.data as {
        id: number; ok: boolean; result?: CompileSuccess; error?: CompileError;
      };
      const resolve = pending.get(id);
      if (!resolve) return;
      pending.delete(id);
      resolve(ok ? { ok: true, result: result! } : { ok: false, error: error! });
    });
  }
  return worker;
}

export function compileSketch(
  source: string,
  sensors: string[],
  onStage?: (stage: string) => void,
): Promise<CompileOutcome> {
  const w = ensureWorker();
  const id = (seq += 1);
  return new Promise((resolve) => {
    pending.set(id, resolve);
    w.postMessage({ type: 'compile', id, source, sensors });
    if (onStage) {
      // toolchain is monolithic; animate stage hints for UX
      const stages = ['cc1plus · پردازش C++', 'avr-as · تدوین', 'avr-ld · پیوند', 'objcopy · HEX'];
      let i = 0;
      const t = setInterval(() => {
        i = (i + 1) % stages.length;
        onStage(stages[i]);
        if (i === 0) clearInterval(t);
      }, 1400);
    }
  });
}
