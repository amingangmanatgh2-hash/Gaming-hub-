# Developer notes

## Layers

```
src/core/    portable C++17, zero Win32 includes. Compiled for the host AND for Windows.
src/win/     Win32 layer: renderer, three windows, platform glue, self test, entry point.
```

`src/core` is deliberately host-compilable: it is where every piece of business logic lives, so it
can be unit-tested and fuzzed on the build machine (`tests/`). The Win32 layer only draws and wires
things up.

| File | Responsibility |
| --- | --- |
| `core/text.*` | UTF-8 ⇄ UTF-16, RTL/bidi detection, previews, Persian digits, relative time |
| `core/json.*` | minimal JSON reader/writer (settings + history), depth limited, BOM tolerant |
| `core/store.*` | clipboard history: newest-first, cap 100, dedupe, search, persistence |
| `core/settings.*` | user settings with forward/backward compatible JSON |
| `core/hotkey.*` | shortcut parsing/formatting, Win+V detection, fallback candidate list |
| `core/layout.*` | Persian/English key tables + row geometry (15 key-units per row) |
| `core/selftest_core.*` | the 648-check suite, used by `tests/` and by `--selftest` |
| `win/gfx.*` | 32-bit DIB surface, analytic anti-aliasing, Uniscribe text, vector icons |
| `win/ui_keyboard.*` | floating keyboard: layout, hover/press animation, injection, drag |
| `win/ui_clipboard.*` | history window: search box, list, copy/delete/clear/pause, scrollbar |
| `win/ui_dashboard.*` | main window: tabs, cards, toggles, status, setup wizard |
| `win/platform.cpp` | focus tracking, SendInput, clipboard listener, tray, HKCU Run, IShellLink |
| `win/app.cpp` | state, persistence, window orchestration, hidden message window |
| `win/selftest_win.cpp` | on-machine PASS/FAIL/SKIP report |
| `win/main.cpp` | single instance, DPI, fonts, message loop |

## Rendering

Everything is drawn into an off-screen `Surface` (a top-down 32-bit `CreateDIBSection`) and
blitted in one `BitBlt`, which is why there is no flicker and no dependency on DWM/GDI+/Direct2D.
Rounded rectangles and circles use a signed distance function with analytic coverage, so edges are
anti-aliased at any DPI without shaders.

Text: Latin runs go through `DrawTextW`; anything containing Arabic-script characters is shaped by
`ScriptStringAnalyse`/`ScriptStringOut` (Uniscribe), which handles the Persian joining forms,
bidi reordering and the U+200C zero-width non-joiner. `font_for_text()` walks a fallback chain
(`Segoe UI → Tahoma → Arial → Segoe UI Symbol → Times New Roman`) using
`GetGlyphIndicesW(..., GGI_MARK_NONEXISTING_GLYPHS)` so rare symbols such as U+FDFC (﷼) never
render as boxes.

## Key injection

1. A 500 ms timer records the foreground window whenever it is *not* ours and not the shell.
2. `app_show_keyboard()` records the target once more before our window takes focus.
3. `inject_char()` calls `ensure_target_receives_input()`: if our own window holds the focus it
   re-activates the target (`AttachThreadInput` workaround for the foreground lock, verified with
   `GetForegroundWindow()`), then sends `INPUT_KEYBOARD` + `KEYEVENTF_UNICODE`.
4. In no-focus mode (`WS_EX_NOACTIVATE`) our window never takes focus, so step 3 is a no-op.

## Adding a keyboard layout

1. Add `KeyDef` tables in `src/core/layout.cpp` (`K()` for a normal key, `KU()` for a key with a
   non-1.0 width, `KVK()` for a virtual-key key).
2. **Every row must total exactly `kRowUnits` (15)** — the test suite asserts this.
3. Add a `PageDef[]`, a `LayoutDef`, extend `layout_by_index()`/`layout_count()`.
4. Run `./tests/run_tests.sh`: the geometry, completeness and special-key tests will validate the
   new table automatically.

## Compatibility rules

* `_WIN32_WINNT` is pinned to `0x0601` in `src/win/amin.h`. Do not raise it; do not call APIs
  newer than Windows 7 without a runtime `GetProcAddress` check + fallback.
* `build/verify_pe.py` fails the build if a newer-Windows API appears in the import table.
* No `#include <winsock2.h>`/`<wininet.h>`/`<ws2tcpip.h>` — the app must stay offline-only.

## Coding conventions

* `snake_case` functions, `PascalCase` types, 4 spaces, 100 column soft limit.
* No exceptions across the Win32 boundary; the app is built with the static runtime and never
  throws out of a window procedure.
* Every user-visible string exists in Persian (primary) and English (`l10n_init()` in
  `win/util_win.cpp`).
