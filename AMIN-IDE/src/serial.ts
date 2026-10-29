// Serial monitor — real Web Serial API (Chrome/Edge) + demo mode fallback.
export interface SerialLine { text: string; }

export class SerialMonitor {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  private decoder = new TextDecoder();
  private encoder = new TextEncoder();
  private demoTimer: ReturnType<typeof setInterval> | null = null;
  private demoSeeds = [
    ' Uptime: 12s  temp: 24.1C  hum: 47%',
    ' ADC0: 512  ADC1: 204',
    ' loop tick',
    ' PWM duty: 128',
  ];
  private demoIdx = 0;

  onLine: ((text: string) => void) | null = null;
  onState: ((state: 'off' | 'on' | 'demo', detail: string) => void) | null = null;

  isSupported(): boolean {
    return typeof navigator !== 'undefined' && !!navigator.serial;
  }

  async connect(baud: number): Promise<void> {
    if (!this.isSupported()) throw new Error('unsupported');
    await this.disconnect(true);
    const port = await (navigator as Navigator & { serial: { requestPort: () => Promise<SerialPort> } }).serial.requestPort();
    await port.open({ baudRate: baud });
    this.port = port;
    this.reader = port.readable.getReader();
    this.writer = port.writable.getWriter();
    this.setState('on', `${baud} baud`);
    this.pump();
  }

  private async pump() {
    try {
      while (this.reader) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value) this.emit(this.decoder.decode(value, { stream: true }));
      }
    } catch {
      /* port closed */
    }
    if (this.onState) this.setState('off', '');
  }

  async disconnect(silent = false): Promise<void> {
    if (this.demoTimer) {
      clearInterval(this.demoTimer);
      this.demoTimer = null;
    }
    if (this.reader) {
      try { await this.reader.cancel(); } catch { /* ignore */ }
      this.reader = null;
    }
    if (this.writer) {
      try { await this.writer.close(); } catch { /* ignore */ }
      this.writer = null;
    }
    if (this.port) {
      try { await this.port.close(); } catch { /* ignore */ }
      this.port = null;
    }
    if (!silent) this.setState('off', '');
  }

  toggleDemo(): boolean {
    if (this.demoTimer) {
      clearInterval(this.demoTimer);
      this.demoTimer = null;
      this.setState('off', '');
      return false;
    }
    this.demoTimer = setInterval(() => {
      this.demoIdx = (this.demoIdx + 1) % this.demoSeeds.length;
      this.emit(this.demoSeeds[this.demoIdx]);
    }, 2500);
    this.emit('-- demo mode --');
    this.setState('demo', 'demo');
    return true;
  }

  isDemo(): boolean { return this.demoTimer !== null; }

  async send(text: string): Promise<void> {
    const line = text + '\r\n';
    if (this.writer) {
      await this.writer.write(this.encoder.encode(line));
    } else if (this.demoTimer) {
      this.emit('> ' + line.trimEnd());
      if (/ping/i.test(text)) this.emit('< pong');
      else this.emit('< (demo) echo: ' + text);
    }
  }

  private emit(text: string) {
    // split into lines so the UI can log them nicely
    const parts = text.split(/\r?\n/);
    let acc = '';
    for (let i = 0; i < parts.length; i += 1) {
      acc += parts[i];
      if (i === parts.length - 1 && parts[i] === '') break;
      this.onLine?.(acc);
      acc = '';
    }
    if (acc) this.onLine?.(acc);
  }

  private setState(state: 'off' | 'on' | 'demo', detail: string) {
    this.onState?.(state, detail);
  }
}
