// Local project storage — everything stays in the browser (localStorage).
export interface SketchFile {
  id: string;
  name: string;     // e.g. "sketch1"
  content: string;
}

export interface Project {
  id: string;
  name: string;
  files: SketchFile[];
  createdAt: number;
  updatedAt: number;
}

const KEY = 'aminide.v1.projects';

export function loadProjects(): Project[] {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw) as Project[];
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

let saveTimer: ReturnType<typeof setTimeout> | null = null;
export function saveProjects(projects: Project[]): void {
  if (saveTimer) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    try {
      localStorage.setItem(KEY, JSON.stringify(projects));
    } catch {
      /* quota — ignore */
    }
  }, 400);
}

export function newId(): string {
  return Math.random().toString(36).slice(2, 10);
}

export function newSketchFile(name: string, content = 'void setup() {\n\n}\n\nvoid loop() {\n\n}\n'): SketchFile {
  return { id: newId(), name, content };
}

export function defaultProject(): Project {
  const f = newSketchFile(
    'sketch1',
    `// ✨ AMIN IDE — به محیط خود خوش آمدید` +
`
// برای شروع، یکی از نمونه‌ها را باز کنید
// یا دکمه بررسی و رفع خطا را بزنید.

void setup() {
  Serial.begin(9600);
  Serial.println("آماده!");
}

void loop() {
  delay(1000);
}
`,
  );
  return {
    id: newId(),
    name: 'Sketch1',
    files: [f],
    createdAt: Date.now(),
    updatedAt: Date.now(),
  };
}
