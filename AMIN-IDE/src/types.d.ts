// Type shims for runtime deps that ship without typings.

declare module '@horang-corp/avr-gcc-wasm' {
  export interface BuildResult {
    hex: string;
    sensors: string[];
    target: { mcu: string; board: string; appFlashBytes: number };
    bytes: number;
    flashBytes: number;
    fitsTarget: boolean;
    objectCount: number;
    memory: { linearPeakBytesByTool: Record<string, number>; linearPeakBytesTotalSequential: number };
    timings: Record<string, number>;
    stderr: string[];
  }
  export function buildFirmware(options?: {
    source?: string;
    sensors?: string[];
    assetsBase?: string | URL;
  }): Promise<BuildResult>;
  export function setAssetsBase(base: string | URL): void;
  export const SENSOR: { OLED: string; TOF: string };
}

// Minimal Web Serial typings (not in this TS lib version).
interface SerialPort {
  open(options: { baudRate: number }): Promise<void>;
  close(): Promise<void>;
  readable: ReadableStream<Uint8Array>;
  writable: WritableStream<Uint8Array>;
}
interface Navigator {
  readonly serial?: { requestPort: () => Promise<SerialPort> };
}
