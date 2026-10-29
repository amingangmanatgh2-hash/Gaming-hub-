// Small UI helpers — toast, modal, dom.
import type { Lang } from './i18n';
import { tr } from './i18n';

export function $<T extends HTMLElement = HTMLElement>(sel: string): T {
  const el = document.querySelector(sel);
  if (!el) throw new Error(`missing element ${sel}`);
  return el as unknown as T;
}

export function toast(text: string, kind: 'ok' | 'bad' | '' = '') {
  const root = $('#toastRoot');
  const el = document.createElement('div');
  el.className = `toast ${kind}`.trim();
  el.textContent = text;
  root.appendChild(el);
  setTimeout(() => {
    el.style.transition = 'opacity .3s, transform .3s';
    el.style.opacity = '0';
    el.style.transform = 'translateY(10px)';
    setTimeout(() => el.remove(), 320);
  }, 2600);
}

export function modal(opts: {
  title: string;
  body: string;      // HTML
  actions: { label: string; kind?: string; onClick: () => void }[];
}): { close: () => void } {
  const root = $('#modalRoot');
  root.innerHTML = `
    <div class="modal">
      <h3>${opts.title}</h3>
      <div class="modal-body">${opts.body}</div>
      <div class="modal-actions"></div>
    </div>`;
  const actionsEl = root.querySelector('.modal-actions') as HTMLElement;
  const close = () => { root.classList.add('hidden'); root.innerHTML = ''; };
  for (const a of opts.actions) {
    const btn = document.createElement('button');
    btn.className = `btn ${a.kind || ''}`.trim();
    btn.textContent = a.label;
    btn.addEventListener('click', () => a.onClick());
    actionsEl.appendChild(btn);
  }
  root.addEventListener('click', (e) => { if (e.target === root) close(); }, { once: true });
  root.classList.remove('hidden');
  return { close };
}

export function applyI18n(lang: Lang) {
  document.querySelectorAll<HTMLElement>('[data-i18n]').forEach((el) => {
    const key = el.getAttribute('data-i18n')!;
    el.textContent = tr(lang, key);
  });
  document.documentElement.lang = lang;
  document.documentElement.dir = lang === 'fa' ? 'rtl' : 'ltr';
}
