// AMIN IDE — app controller
import './styles.css';
import { Editor } from './editor';
import { compileSketch } from './compiler';
import { SerialMonitor } from './serial';
import { COMPILE_BOARDS, boardById } from './boards';
import { LIBS, SENSOR_OLED, SENSOR_TOF, type LibInfo } from './libs';
import { EXAMPLES } from './sketches';
import { loadProjects, saveProjects, defaultProject, newSketchFile, newId, type Project, type SketchFile } from './storage';
import { tr, type Lang } from './i18n';
import { $, toast, modal, applyI18n } from './ui';

/* ===================== state ===================== */
interface State {
  lang: Lang;
  projects: Project[];
  projectId: string | null;
  fileId: string | null;
  boardId: string;
  sensors: Set<string>;
  lastFlash: number | null;
  lastErrors: number[];
}

const state: State = {
  lang: 'fa',
  projects: [],
  projectId: null,
  fileId: null,
  boardId: 'uno',
  sensors: new Set<string>(),
  lastFlash: null,
  lastErrors: [],
};

let editor: Editor;
let serial: SerialMonitor;
let busy = false;

/* ===================== bootstrap ===================== */
function boot() {
  state.projects = loadProjects();
  if (state.projects.length === 0) {
    state.projects = [defaultProject()];
    saveProjects(state.projects);
  }
  state.projectId = state.projects[0].id;

  editor = new Editor($('#editorWrap'));
  serial = new SerialMonitor();

  buildBoardSelect();
  wireToolbar();
  wireSidebar();
  wireEditor();
  wireSerial();
  wireShortcuts();

  serial.onLine = (text) => logSerial(text);
  serial.onState = (st, detail) => {
    const dot = $('#serialDot');
    dot.className = 'dot ' + (st === 'on' ? 'on' : st === 'demo' ? 'demo' : '');
    const stEl = $('#serialState');
    stEl.textContent = st === 'on' ? tr(state.lang, 'serialOn') + ' · ' + detail : st === 'demo' ? 'demo' : tr(state.lang, 'serialOff');
    $<HTMLInputElement>('#serialInput').disabled = st === 'off';
  };

  applyI18n(state.lang);
  $('#verLabel').textContent = '1.0.0';

  openProject(state.projectId, true);
  renderSidebar('files');
  renderExamples();
  renderLibs();
}

/* ===================== helpers ===================== */
function currentProject(): Project {
  const p = state.projects.find((x) => x.id === state.projectId);
  if (!p) throw new Error('no project');
  return p;
}

function currentFile(): SketchFile | null {
  const p = currentProject();
  return p.files.find((f) => f.id === state.fileId) ?? null;
}

function persist() {
  currentProject().updatedAt = Date.now();
  saveProjects(state.projects);
}

function setFile(id: string | null) {
  state.fileId = id;
  renderTabs();
  const f = currentFile();
  if (f) {
    editor.setValue(f.content);
    editor.focus();
    renderEditorVisible(true);
    updateStatusCursor();
  } else {
    renderEditorVisible(false);
  }
}

function renderEditorVisible(show: boolean) {
  $('#editorWrap').classList.toggle('hidden', !show);
  $('#emptyState').classList.toggle('hidden', show);
}

/* ===================== boards ===================== */
function buildBoardSelect() {
  const sel = $<HTMLSelectElement>('#boardSelect');
  sel.innerHTML = COMPILE_BOARDS.map((b) => `<option value="${b.id}">${b.name}</option>`).join('');
  sel.value = state.boardId;
  sel.addEventListener('change', () => {
    state.boardId = sel.value;
    updateStatusBar();
  });
  updateStatusBar();
}

function updateStatusBar() {
  const board = boardById(state.boardId) ?? COMPILE_BOARDS[0];
  $('#statBoard').innerHTML = `<b>${board.name}</b>`;
  $('#statChip').textContent = board.chip;
  const used = state.lastFlash ?? 0;
  const pct = Math.min(100, (used / 32256) * 100);
  const bar = $('#flashBar');
  bar.style.width = pct + '%';
  $('#statFlash').innerHTML = used > 0
    ? `${used} B / 32256 B (${pct.toFixed(1)}%)`
    : 'flash —';
  bar.parentElement!.className = 'flash-bar' + (pct > 85 ? ' over' : pct > 60 ? ' warn' : '');
}

function updateStatusCursor() {
  const { line, col } = editor.getCursor();
  $('#statPos').textContent = `Ln ${line}, Col ${col}`;
}

/* ===================== console ===================== */
function consoleEl(): HTMLPreElement {
  return $('#console');
}

function consoleClear() {
  consoleEl().innerHTML = '';
}

function logConsole(text: string, cls = 'lvl-info') {
  const pre = consoleEl();
  pre.querySelector('.console-empty')?.remove();
  const line = document.createElement('div');
  line.className = cls;
  line.textContent = text;
  pre.appendChild(line);
  pre.scrollTop = pre.scrollHeight;
}

function logSerial(text: string) {
  const pre = $('#serialLog');
  pre.querySelector('.console-empty')?.remove();
  const line = document.createElement('div');
  line.textContent = text;
  pre.appendChild(line);
  if ($<HTMLInputElement>('#chkAutoscroll').checked) pre.scrollTop = pre.scrollHeight;
}

/* ===================== compile ===================== */
async function verify(showGuide = false) {
  if (busy) return;
  const f = currentFile();
  if (!f) { toast(tr(state.lang, 'welcomeHint'), 'bad'); return; }
  busy = true;
  const btn = $<HTMLButtonElement>('#btnVerify');
  btn.disabled = true;
  consoleClear();
  logConsole(tr(state.lang, 'compiling'), 'lvl-stage');
  const progress = document.createElement('div');
  progress.className = 'progress';
  progress.textContent = '…';
  consoleEl().appendChild(progress);

  try {
    const outcome = await compileSketch(f.content, [...state.sensors], (stage) => {
      progress.textContent = stage;
    });
    progress.remove();
    if (outcome.ok) {
      const r = outcome.result;
      state.lastFlash = r.flashBytes;
      state.lastErrors = [];
      editor.setErrorLines([]);
      logConsole(`✔ ${tr(state.lang, 'compileOk')} — ${r.flashBytes} B / ${r.appFlashBytes} B (${(r.totalMs / 1000).toFixed(1)} s)`, 'lvl-ok');
      if (showGuide) {
        logConsole(r.fitsTarget ? '✔ HEX آماده بارگذاری — دانلود شد' : '✖ Flash از حد تجاوز کرد — مناسب نیست', r.fitsTarget ? 'lvl-ok' : 'lvl-err');
        downloadHex(r.hex);
        showFlashGuide(r.fitsTarget);
      } else {
        logConsole('✔ HEX قابل بارگذاری آماده است — با دکمه بارگذاری دانلود می‌شود.', 'lvl-ok');
      }
      toast(`${tr(state.lang, 'compileOk')} — ${r.flashBytes} B`, 'ok');
    } else {
      const err = outcome.error;
      state.lastErrors = err.errors.map((e) => e.line);
      editor.setErrorLines(state.lastErrors);
      logConsole(`✖ ${tr(state.lang, 'compileFail')}`, 'lvl-err');
      for (const line of err.message.split('\n')) {
        if (/sketch:\d+/.test(line)) {
          const m = line.match(/sketch:(\d+)/);
          const ln = Number(m![1]);
          const div = document.createElement('div');
          div.className = 'errline lvl-err';
          div.textContent = line;
          div.title = tr(state.lang, 'line') + ' ' + ln;
          div.addEventListener('click', () => editor.goToLine(ln));
          consoleEl().appendChild(div);
        } else {
          logConsole(line, 'lvl-err');
        }
      }
      toast(tr(state.lang, 'compileFail'), 'bad');
    }
  } catch (e) {
    progress.remove();
    logConsole('✖ ' + String(e), 'lvl-err');
    toast(String(e), 'bad');
  } finally {
    busy = false;
    btn.disabled = false;
    updateStatusBar();
    editor.focus();
  }
}

function downloadHex(hex: string) {
  const blob = new Blob([hex], { type: 'text/plain' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  const f = currentFile();
  a.download = `${f ? f.name : 'sketch'}.hex`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

function showFlashGuide(fits: boolean) {
  modal({
    title: tr(state.lang, 'flashGuide'),
    body: `<p>${tr(state.lang, 'flashGuideBody')}</p>`,
    actions: [{ label: tr(state.lang, 'close'), kind: 'btn-ghost', onClick: () => {} }],
  });
}

/* ===================== files / tabs ===================== */
function renderTabs() {
  const p = currentProject();
  const bar = $('#tabsBar');
  bar.innerHTML = '';
  for (const f of p.files) {
    const tab = document.createElement('div');
    tab.className = 'tab' + (f.id === state.fileId ? ' active' : '');
    tab.innerHTML = `<span class="name">${f.name}.ino</span>`;
    const close = document.createElement('button');
    close.className = 'close';
    close.textContent = '✕';
    close.title = tr(state.lang, 'deleteFile');
    close.addEventListener('click', (e) => {
      e.stopPropagation();
      if (p.files.length === 1) { toast('حداقل یک فایل وجود دارد', 'bad'); return; }
      p.files = p.files.filter((x) => x.id !== f.id);
      if (state.fileId === f.id) setFile(p.files[0].id);
      persist();
      renderTabs();
      renderSidebar('files');
    });
    tab.addEventListener('click', () => setFile(f.id));
    tab.appendChild(close);
    bar.appendChild(tab);
  }
}

function renderSidebar(panel: string) {
  document.querySelectorAll('.side-btn').forEach((b) => {
    b.classList.toggle('active', (b as HTMLElement).dataset.panel === panel);
  });
  $('#panelFiles').classList.toggle('hidden', panel !== 'files');
  $('#panelExamples').classList.toggle('hidden', panel !== 'examples');
  $('#panelLibs').classList.toggle('hidden', panel !== 'libs');
  $('#panelSettings').classList.toggle('hidden', panel !== 'settings');
  renderFileList();
}

function renderFileList() {
  const p = currentProject();
  const list = $('#fileList');
  list.innerHTML = '';
  for (const f of p.files) {
    const item = document.createElement('div');
    item.className = 'list-item' + (f.id === state.fileId ? ' active' : '');
    item.innerHTML = `<span class="name">${f.name}.ino</span><span class="tag">.ino</span>`;
    item.addEventListener('click', () => setFile(f.id));
    list.appendChild(item);
  }
}

function renderExamples() {
  const list = $('#exampleList');
  list.innerHTML = '';
  for (const ex of EXAMPLES) {
    const item = document.createElement('div');
    item.className = 'list-item';
    item.innerHTML = `<span class="name">${ex.title}</span><span class="tag">#</span>`;
    item.title = ex.desc;
    item.addEventListener('click', () => loadExample(ex.id));
    list.appendChild(item);
  }
}

function loadExample(id: string) {
  const ex = EXAMPLES.find((e) => e.id === id);
  if (!ex) return;
  const p = currentProject();
  const f = newSketchFile(ex.id, ex.code);
  p.files.push(f);
  setFile(f.id);
  persist();
  renderTabs();
  renderSidebar('files');
  switchBottomTab('console');
  toast(ex.title, 'ok');
}

function renderLibs() {
  const list = $('#libList');
  list.innerHTML = '';
  for (const lib of LIBS) {
    const item = document.createElement('div');
    item.className = 'list-item';
    const active = lib.kind === 'sensor' && state.sensors.has(lib.nameEn === 'SSD1306 (OLED)' ? SENSOR_OLED : SENSOR_TOF);
    item.classList.toggle('active', active);
    item.innerHTML = `<span class="name">${lib.name}<br><span class="muted" style="font-size:11px">${lib.desc}</span></span><span class="tag">offline</span>`;
    if (lib.kind === 'sensor') {
      item.addEventListener('click', () => toggleSensor(lib));
    }
    list.appendChild(item);
  }
}

function sensorKey(lib: LibInfo): string {
  return lib.nameEn === 'SSD1306 (OLED)' ? SENSOR_OLED : SENSOR_TOF;
}

function toggleSensor(lib: LibInfo) {
  const key = sensorKey(lib);
  if (state.sensors.has(key)) state.sensors.delete(key);
  else state.sensors.add(key);
  renderLibs();
  toast(state.sensors.has(key) ? lib.name + ': فعال' : lib.name + ': غیرفعال', 'ok');
}

/* ===================== actions ===================== */
function newProject() {
  const p = defaultProject();
  state.projects.push(p);
  state.projectId = p.id;
  state.fileId = p.files[0].id;
  setFile(state.fileId);
  persist();
  renderTabs();
  renderSidebar('files');
  renderEditorVisible(true);
}

function openSketchFile() {
  $('#filePicker').click();
}

function onFilePicked(e: Event) {
  const input = e.target as HTMLInputElement;
  const file = input.files?.[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => {
    const p = currentProject();
    const f = newSketchFile(file.name.replace(/\.[^.]+$/, ''), String(reader.result));
    p.files.push(f);
    setFile(f.id);
    persist();
    renderTabs();
    renderSidebar('files');
    toast(file.name, 'ok');
  };
  reader.readAsText(file);
  input.value = '';
}

function downloadIno() {
  const f = currentFile();
  if (!f) return;
  const blob = new Blob([f.content], { type: 'text/plain' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `${f.name}.ino`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  toast(f.name + '.ino', 'ok');
}

function addFile() {
  const p = currentProject();
  const n = Math.max(2, p.files.length + 1);
  const f = newSketchFile('sketch' + n);
  p.files.push(f);
  setFile(f.id);
  persist();
  renderTabs();
  renderSidebar('files');
}

/* ===================== wiring ===================== */
function wireToolbar() {
  $('#btnNew').addEventListener('click', newProject);
  $('#btnOpen').addEventListener('click', openSketchFile);
  $('#filePicker').addEventListener('change', onFilePicked as EventListener);
  $('#btnSave').addEventListener('click', () => {
    const f = currentFile();
    if (f) { f.content = editor.value; persist(); toast(tr(state.lang, 'save'), 'ok'); }
  });
  $('#btnDownload').addEventListener('click', downloadIno);
  $('#btnVerify').addEventListener('click', () => verify(false));
  $('#btnUpload').addEventListener('click', () => verify(true));
  $('#btnLang').addEventListener('click', () => {
    state.lang = state.lang === 'fa' ? 'en' : 'fa';
    applyI18n(state.lang);
    $('#langLabel').textContent = state.lang === 'fa' ? 'EN' : 'فارسی';
    renderSidebar(currentPanel());
    toast(tr(state.lang, 'langSwitched'), 'ok');
  });
  $('#btnAddFile').addEventListener('click', addFile);
}

function currentPanel(): string {
  return document.querySelector('.side-btn.active')?.getAttribute('data-panel') || 'files';
}

function wireSidebar() {
  document.querySelectorAll('.side-btn').forEach((b) => {
    b.addEventListener('click', () => renderSidebar((b as HTMLElement).dataset.panel!));
  });
}

function wireEditor() {
  $('#ta').addEventListener('keyup', updateStatusCursor);
  $('#ta').addEventListener('click', updateStatusCursor);
  $('#ta').addEventListener('input', () => {
    const f = currentFile();
    if (f) f.content = editor.value;
    updateStatusCursor();
    schedulePersist();
  });
  $('#btnWelcomeNew').addEventListener('click', newProject);
  $('#btnWelcomeExamples').addEventListener('click', () => {
    renderSidebar('examples');
    toast(tr(state.lang, 'examples'), 'ok');
  });
}

let persistTimer: ReturnType<typeof setTimeout> | null = null;
function schedulePersist() {
  if (persistTimer) clearTimeout(persistTimer);
  persistTimer = setTimeout(persist, 500);
}

function wireSerial() {
  $('#btnSerial').addEventListener('click', () => {
    if ($('#consolePanel').classList.contains('hidden')) switchBottomTab('serial');
    else switchBottomTab('console');
  });
  $('#btnSerialConnect').addEventListener('click', async () => {
    const baud = Number(($('#baudSelect') as HTMLSelectElement).value);
    if (serial.isDemo()) { serial.toggleDemo(); return; }
    try {
      await serial.connect(baud);
      logSerial('-- connected @ ' + baud + ' baud --');
    } catch (e) {
      logSerial('-- error: ' + String(e) + ' --');
      toast(tr(state.lang, 'portsUnsupported'), 'bad');
    }
  });
  $('#btnSerialDemo').addEventListener('click', () => {
    const on = serial.toggleDemo();
    logSerial(on ? '-- demo started --' : '-- demo stopped --');
  });
  $('#btnSerialClear').addEventListener('click', () => {
    $('#serialLog').innerHTML = '<span class="console-empty">' + tr(state.lang, 'serialEmpty') + '</span>';
  });
  $('#btnSerialSend').addEventListener('click', () => {
    const input = $('#serialInput') as HTMLInputElement;
    if (input.value) {
      serial.send(input.value).then(() => { input.value = ''; });
    }
  });
  $('#serialInput').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      if (input.value) {
        serial.send(input.value).then(() => { input.value = ''; });
      }
    }
  });
}

function switchBottomTab(which: 'console' | 'serial') {
  document.querySelectorAll('.btab').forEach((b) => {
    b.classList.toggle('active', b.getAttribute('data-bt') === which);
  });
  $('#consolePanel').classList.toggle('hidden', which !== 'console');
  $('#serialPanel').classList.toggle('hidden', which !== 'serial');
}

function wireShortcuts() {
  document.addEventListener('keydown', (e) => {
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key === 'Enter') {
      e.preventDefault();
      verify(false);
    }
    if (mod && (e.key === 's' || e.key === 'S')) {
      e.preventDefault();
      const f = currentFile();
      if (f) { f.content = editor.value; persist(); toast(tr(state.lang, 'save'), 'ok'); }
    }
  });
  $('#btnCollapse').addEventListener('click', () => $('#bottomPanel').classList.toggle('collapsed'));
  document.querySelectorAll('.btab').forEach((b) => {
    b.addEventListener('click', () => switchBottomTab(b.getAttribute('data-bt') as 'console' | 'serial'));
  });
}

/* ===================== entry ===================== */
function openProject(id: string, initial: boolean) {
  const p = state.projects.find((x) => x.id === id);
  if (!p) return;
  state.projectId = id;
  state.fileId = p.files[0]?.id ?? null;
  setFile(state.fileId);
  renderTabs();
  if (!initial) renderSidebar(currentPanel());
}

boot();
