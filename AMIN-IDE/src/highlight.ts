// Lightweight Arduino C++ syntax highlighter — no deps, safe (escapes HTML).

const KEYWORDS = new Set([
  'void', 'int', 'bool', 'char', 'float', 'double', 'long', 'short', 'unsigned', 'signed',
  'const', 'static', 'volatile', 'extern', 'inline', 'struct', 'class', 'enum',
  'public', 'private', 'protected', 'virtual', 'override', 'final', 'template',
  'typename', 'using', 'namespace', 'return', 'if', 'else', 'for', 'while', 'do',
  'switch', 'case', 'default', 'break', 'continue', 'goto', 'new', 'delete', 'this',
  'true', 'false', 'NULL', 'nullptr', 'sizeof', 'typedef', 'friend', 'explicit',
  'operator', 'try', 'catch', 'throw', 'noexcept', 'alignas', 'decltype', 'concept',
  'requires', 'and', 'or', 'not', 'asm', 'register', 'mutable', 'constexpr',
  // Arduino / AVR
  'HIGH', 'LOW', 'INPUT', 'OUTPUT', 'INPUT_PULLUP', 'LED_BUILTIN', 'ISR', 'PROGMEM',
  'F', 'S', 'PSTR', 'COLD', 'INTERNAL', 'DEFAULT', 'EXTERNAL', 'PULLUP',
]);

const TYPES = new Set([
  'String', 'byte', 'word', 'uint8_t', 'uint16_t', 'uint32_t', 'int8_t', 'int16_t',
  'int32_t', 'size_t', 'HardwareSerial', 'Stream', 'Print', 'Wire', 'SPI', 'Servo',
  'DHT', 'Client', 'Server', 'IPAddress', 'Printable', 'Tone', 'pgm_read_byte',
  'Serial', 'SD', 'Ethernet',
]);

const OP_CHARS = new Set(['{', '}', '(', ')', '[', ']', ';', ',', '.', ':', '?', '+', '-', '*', '/', '%', '=', '!', '<', '>', '&', '|', '^', '~', '@', '#', '`']);

export function escapeHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

interface Tok { t: string; v: string; }

function tokenize(src: string): Tok[] {
  const toks: Tok[] = [];
  let i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i];
    // block comment
    if (c === '/' && src[i + 1] === '*') {
      const end = src.indexOf('*/', i + 2);
      const stop = end === -1 ? n : end + 2;
      toks.push({ t: 'com', v: src.slice(i, stop) });
      i = stop;
      continue;
    }
    // line comment
    if (c === '/' && src[i + 1] === '/') {
      let j = src.indexOf('\n', i);
      if (j === -1) j = n;
      toks.push({ t: 'com', v: src.slice(i, j) });
      i = j;
      continue;
    }
    // string
    if (c === '"') {
      let j = i + 1;
      while (j < n && src[j] !== '"') {
        if (src[j] === '\\') j += 2;
        else j += 1;
      }
      toks.push({ t: 'str', v: src.slice(i, Math.min(j + 1, n)) });
      i = j + 1;
      continue;
    }
    // char
    if (c === "'") {
      let j = i + 1;
      while (j < n && src[j] !== "'") {
        if (src[j] === '\\') j += 2;
        else j += 1;
      }
      toks.push({ t: 'chr', v: src.slice(i, Math.min(j + 1, n)) });
      i = j + 1;
      continue;
    }
    // preprocessor (line starting with # after optional ws)
    if (c === '#') {
      let j = src.indexOf('\n', i);
      if (j === -1) j = n;
      toks.push({ t: 'pre', v: src.slice(i, j) });
      i = j;
      continue;
    }
    // number
    if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(src[i + 1] || ''))) {
      let j = i + 1;
      while (j < n && /[0-9a-fA-FxXoObB._]/.test(src[j])) j += 1;
      while (j < n && /[lLfFuU]/.test(src[j])) j += 1;
      toks.push({ t: 'num', v: src.slice(i, j) });
      i = j;
      continue;
    }
    // identifier
    if (/[A-Za-z_$]/.test(c)) {
      let j = i + 1;
      while (j < n && /[A-Za-z0-9_$]/.test(src[j])) j += 1;
      const word = src.slice(i, j);
      if (KEYWORDS.has(word)) toks.push({ t: 'kw', v: word });
      else if (TYPES.has(word)) toks.push({ t: 'type', v: word });
      else {
        // skip ws to see if next non-ws char is '(' → function call
        let k = j;
        while (k < n && /[ \t]/.test(src[k])) k += 1;
        if (src[k] === '(') toks.push({ t: 'fn', v: word });
        else toks.push({ t: 'id', v: word });
      }
      i = j;
      continue;
    }
    // punctuation
    if (OP_CHARS.has(c)) {
      toks.push({ t: 'op', v: c });
      i += 1;
      continue;
    }
    // anything else (ws, newline)
    let j = i + 1;
    while (j < n && !OP_CHARS.has(src[j]) && !/[A-Za-z0-9_$"'#/]/.test(src[j])) j += 1;
    toks.push({ t: 'ws', v: src.slice(i, j) });
    i = j;
  }
  return toks;
}

const CLASS: Record<string, string> = {
  kw: 'tok-kw', type: 'tok-type', fn: 'tok-fn', str: 'tok-str', chr: 'tok-chr',
  num: 'tok-num', pre: 'tok-pre', com: 'tok-com', op: 'tok-op', ws: '', id: '',
};

export function highlight(src: string): string {
  const toks = tokenize(src);
  let out = '';
  for (const t of toks) {
    const cls = CLASS[t.t];
    out += cls ? `<span class="${cls}">${escapeHtml(t.v)}</span>` : escapeHtml(t.v);
  }
  return out;
}
