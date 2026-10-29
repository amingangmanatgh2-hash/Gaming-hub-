// Custom lightweight code editor: transparent textarea over a highlighted <pre>,
// line numbers, auto-indent, bracket matching. Zero dependencies.
import { highlight } from './highlight';

export class Editor {
  private ta: HTMLTextAreaElement;
  private hl: HTMLPreElement;
  private ln: HTMLDivElement;
  private raf = 0;
  private errLines = new Set<number>();

  constructor(private host: HTMLElement) {
    host.innerHTML = `
      <div class="line-numbers" data-ln></div>
      <div class="code-area">
        <pre class="highlight" data-hl aria-hidden="true"><code></code></pre>
        <textarea class="ta" spellcheck="false" autocomplete="off" autocorrect="off" autocapitalize="off" wrap="off"></textarea>
      </div>`;
    this.ln = host.querySelector('[data-ln]') as HTMLDivElement;
    this.hl = host.querySelector('[data-hl]') as HTMLPreElement;
    this.ta = host.querySelector('.ta') as HTMLTextAreaElement;

    this.ta.addEventListener('input', () => this.schedule());
    this.ta.addEventListener('keydown', (e) => this.onKey(e));
    this.ta.addEventListener('scroll', () => this.syncScroll());
    this.hl.addEventListener('scroll', () => {
      this.hl.scrollTop = this.ta.scrollTop;
      this.hl.scrollLeft = this.ta.scrollLeft;
    });

    this.update();
  }

  get value(): string { return this.ta.value; }

  setValue(v: string, keepCursor = false) {
    const pos = this.ta.selectionStart;
    this.ta.value = v;
    this.update();
    if (keepCursor) {
      try { this.ta.setSelectionRange(Math.min(pos, v.length), Math.min(pos, v.length)); } catch { /* ignore */ }
    }
  }

  focus() { this.ta.focus(); }

  /** jump to a line (1-based), flash it red */
  goToLine(line: number) {
    const lines = this.ta.value.split('\n');
    let pos = 0;
    for (let i = 0; i < line - 1 && i < lines.length; i += 1) pos += lines[i].length + 1;
    this.ta.focus();
    this.ta.setSelectionRange(pos, pos + (lines[line - 1]?.length || 0));
    this.hl.scrollTop = this.ta.scrollTop = Math.max(0, (line - 1) * 1.55 * 13 - 120);
    this.ta.scrollIntoView({ block: 'center', behavior: 'smooth' });
    this.flashLine(line);
  }

  private flashLine(line: number) {
    this.hl.querySelectorAll('.flash').forEach((el) => el.classList.remove('flash'));
    const rows = this.hl.querySelectorAll('.hlrow');
    if (rows[line - 1]) {
      const row = rows[line - 1] as HTMLElement;
      row.classList.add('flash');
      row.animate(
        [{ background: 'rgba(248,113,113,.28)' }, { background: 'rgba(248,113,113,0)' }],
        { duration: 900, easing: 'ease-out' },
      );
    }
  }

  setErrorLines(lines: number[]) {
    this.errLines = new Set(lines);
    this.schedule();
  }

  private schedule() {
    cancelAnimationFrame(this.raf);
    this.raf = requestAnimationFrame(() => this.update());
  }

  private update() {
    const v = this.ta.value;
    this.hl.querySelector('code')!.innerHTML = highlight(v);
    // split into rows so we can mark error lines
    const lines = v.split('\n');
    let html = '';
    for (let i = 0; i < lines.length; i += 1) {
      const rowCls = this.errLines.has(i + 1) ? 'hlrow errline' : 'hlrow';
      html += `<span class="${rowCls}">${highlight(lines[i] || ' ') || '&nbsp;'}</span>`;
      if (i < lines.length - 1) html += '\n';
    }
    this.hl.querySelector('code')!.innerHTML = html;

    // line numbers
    let lhtml = '';
    for (let i = 1; i <= lines.length; i += 1) {
      lhtml += this.errLines.has(i)
        ? `<div style="color:var(--red)">${i}</div>`
        : `<div>${i}</div>`;
    }
    this.ln.innerHTML = lhtml;
    this.syncScroll();
  }

  private syncScroll() {
    this.hl.scrollTop = this.ta.scrollTop;
    this.hl.scrollLeft = this.ta.scrollLeft;
    this.ln.scrollTop = this.ta.scrollTop;
  }

  private onKey(e: KeyboardEvent) {
    const mod = e.ctrlKey || e.metaKey;

    // Ctrl+Enter / Ctrl+S handled by app; stop here
    if (mod && (e.key === 'Enter' || e.key === 's' || e.key === 'S')) {
      // let app handle (it listens on document too); just blur-safe
      return;
    }

    if (e.key === 'Tab') {
      e.preventDefault();
      const { selectionStart: s, selectionEnd: en, value } = this.ta;
      if (s !== en && value.slice(s, en).includes('\n')) {
        // indent selected lines
        const before = value.slice(0, s);
        const sel = value.slice(s, en);
        const after = value.slice(en);
        const indented = sel.split('\n').map((l, i) => (e.shiftKey ? l.replace(/^ {1,2}/, '') : '  ' + l)).join('\n');
        this.ta.value = before + indented + after;
        this.ta.selectionStart = s;
        this.ta.selectionEnd = s + indented.length;
        this.update();
      } else {
        this.insertAt(s, en, '  ');
      }
      return;
    }

    if (e.key === 'Enter') {
      e.preventDefault();
      const { selectionStart: s, selectionEnd: en, value } = this.ta;
      if (s !== en) return; // let default multiline paste-like behavior happen
      const lineStart = value.lastIndexOf('\n', s - 1) + 1;
      const line = value.slice(lineStart, s);
      const indent = (line.match(/^[ \t]*/) || [''])[0];
      const prev = line.trimEnd();
      let extra = '';
      if (/[([{]$/.test(prev)) extra = '  ';
      else if (/[)\]}]$/.test(prev)) extra = '';
      const insert = '\n' + indent + extra;
      this.insertAt(s, en, insert);
      return;
    }

    // bracket auto-close
    const openers: Record<string, string> = { '(': ')', '{': '}', '[': ']' };
    const closers: Record<string, string> = { ')': '(', '}': '{', ']': '[' };
    if (openers[e.key]) {
      const s = this.ta.selectionStart;
      const en = this.ta.selectionEnd;
      if (s === en) {
        e.preventDefault();
        this.insertAt(s, en, e.key + openers[e.key]);
        this.ta.setSelectionRange(s + 1, s + 1);
      }
      return;
    }
    if (closers[e.key]) {
      const s = this.ta.selectionStart;
      const en = this.ta.selectionEnd;
      if (s === en) {
        const before = this.ta.value[s - 1];
        if (before === closers[e.key]) {
          e.preventDefault();
          this.ta.setSelectionRange(s - 1, s - 1);
          return;
        }
      }
    }
  }

  private insertAt(s: number, en: number, text: string) {
    const { value } = this.ta;
    this.ta.value = value.slice(0, s) + text + value.slice(en);
    const pos = s + text.length;
    this.ta.selectionStart = this.ta.selectionEnd = pos;
    this.update();
    this.ta.focus();
  }

  getCursor(): { line: number; col: number } {
    const { selectionStart: s, value } = this.ta;
    const before = value.slice(0, s);
    const line = before.split('\n').length;
    const col = s - before.lastIndexOf('\n');
    return { line, col };
  }
}
