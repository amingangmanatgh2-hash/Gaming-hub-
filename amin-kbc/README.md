# AMIN Keyboard & Clipboard

A lightweight Windows desktop application with a **Persian/English on-screen keyboard** and a
**clipboard history manager**. Built for **Windows 10 (works from 7 SP1)** — including older
builds such as **1803 (build 17134)** — and for modest hardware such as a Dell Latitude E5570
with 8 GB RAM.

* **One executable, ~0.6 MB, no runtime to install.** Native Win32 + static C++17 runtime,
  no .NET, no Electron, no web view.
* **Persian first:** RTL layout, Persian digits, ZWNJ (نیم‌فاصله), آ/ژ/پ/چ/گ, Persian punctuation.
* **Clipboard history:** the last 100 text entries, newest first, duplicates merged, searchable,
  saved to disk and restored at startup. **Text only** — images and files are not captured, and
  nothing is ever uploaded.
* **Typing goes to the right window:** keys are injected with `SendInput(KEYEVENTF_UNICODE)` into
  the window that was focused *before* the keyboard opened.

> **Honest status:** this repository contains complete source code and **compiled, verified
> binaries**. The code was compiled here for x64 and x86 and its portable logic is covered by 648
> automated checks. However, the build environment is Linux: the GUI itself has **not** been run
> on a real Windows desktop. `docs/TESTING.md` lists exactly what was verified and what still has
> to be confirmed on your PC (and `AminKeyboardClipboard.exe --selftest` does that automatically).

---

## 1. What you get

| Path | Content |
| --- | --- |
| `dist/AminKeyboardClipboard-x64.exe` | 64-bit build (recommended) |
| `dist/AminKeyboardClipboard-x86.exe` | 32-bit build (any Windows 7–11) |
| `dist/README.txt` | quick start for the binaries |
| `installer/install.bat` / `install.ps1` | per-user installer, **no administrator rights** |
| `installer/uninstall.bat` | removes the app, keeps your clipboard history |
| `installer/amin-kbc.iss` | optional Inno Setup script for a classic `setup.exe` |
| `src/` | complete source code (portable core + Win32 UI) |
| `build/` | build scripts (Zig cross-compile, MSVC, PE verifier) |
| `tests/` | host-side test runner for the core logic |
| `docs/INSTALL-FA.md` | **راهنمای نصب و استفاده به فارسی** |
| `docs/TESTING.md` | verification report (what was and was not tested) |
| `docs/DEVELOPER.md` | architecture and how to add a keyboard layout |
| `docs/preview/` | renders produced from the real key tables (dev aid, not screenshots) |
| `dist/AMIN-Keyboard-Clipboard-1.0.0.zip` | both builds + installer + manuals in one archive |

---

## 2. Features

### Virtual keyboard
* Full English QWERTY layout and full Persian (ISIRI/Windows) layout, plus a symbols page with
  currency, math and Arabic diacritics.
* Letters, digits (ASCII and ۰–۹), punctuation (`. ، ؟ ؛ « » ٪ ﷼`), Space, Backspace, Enter, Tab,
  Esc, Shift, CapsLock, language switch and a hide button.
* **Floating, always-on-top** window that can be dragged by its title bar.
* **No-focus mode** (`WS_EX_NOACTIVATE`): the keyboard never takes focus, so typed characters
  always land in the application you were working in.
* Remembers the target window (title shown in the keyboard's title bar) and returns focus to it
  before injecting a keystroke.
* A live preview of the text being sent, so you can see the result even if the target app is
  covered.

### Clipboard manager
* Real `WM_CLIPBOARDUPDATE` listener (no polling) — captures text copied with **Ctrl+C**.
* Up to **100 entries**, newest first, exact duplicates are merged and moved to the top.
* Click an entry → it is copied to the clipboard (paste with **Ctrl+V**).
  Double-click / **Enter** → copy, hide the window and give focus back to your application.
* **Search** (case-insensitive, Persian letter variants folded: ي≈ی, ك≈ک, ة≈ه), **delete**,
  **clear all**, **pause capture**.
* Saved to `%APPDATA%\AminKeyboardClipboard\history.json` and restored at startup.
* Text only. Images/files are **not** captured. Nothing leaves the PC.

### Shortcuts, startup, installer
* A single global shortcut opens/hides the clipboard window. The app tries **Win+V** first and
  automatically falls back to **Ctrl+Alt+V** when Windows owns Win+V (Windows 10 1809+).
  The dashboard always shows **what is actually registered**, plus a button that opens the
  clipboard window regardless of shortcuts.
* A second shortcut (**Ctrl+Alt+K**) toggles the keyboard.
* *Start with Windows* writes exactly one value under
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` — **no administrator rights**.
* Desktop shortcut is created by the app itself (`IShellLink`), no installer needed.
* Per-user installer scripts, and an optional Inno Setup script for a normal `setup.exe`.

---

## 3. Quick start (portable, no installation)

The easiest route: download **`dist/AMIN-Keyboard-Clipboard-1.0.0.zip`**, extract it and run
`AminKeyboardClipboard-x64.exe`.

1. Copy `AminKeyboardClipboard-x64.exe` (or the `-x86.exe` build) anywhere you like — the Desktop,
   a USB stick, `C:\Apps\...`.
2. Double-click it. Windows SmartScreen may warn about an unsigned program (normal for any
   software without a code-signing certificate): *More info → Run anyway*.
3. The setup wizard opens on first launch. Choose a shortcut for the clipboard window, optionally
   enable *Start with Windows*, and you are done.
4. Click **Show keyboard** / **نمایش صفحه کلید**: click into any text field of any application
   first, then press the keys.

Nothing is installed and nothing is registered unless you press the toggle for it. To remove the
program, delete the .exe; to remove your data, delete `%APPDATA%\AminKeyboardClipboard`.

## 4. Installer (still no administrator rights)

```
installer\install.bat                 (double-click; per-user install into %LOCALAPPDATA%)
installer\install.bat /autostart      (also start with Windows)
installer\uninstall.bat               (removes the app, keeps the clipboard history)
```

The script copies the program to `%LOCALAPPDATA%\Programs\AminKeyboardClipboard`, creates Start Menu
and Desktop shortcuts, registers an entry in *Apps & features* (**HKCU only**), and finishes by
verifying that everything exists.

A classic `setup.exe` can be produced on Windows from `installer/amin-kbc.iss`:

```
build\build_windows.bat
build\build_installer_windows.bat     (needs Inno Setup 6, https://jrsoftware.org/isdl.php)
```

---

## 5. Build from source

### Option A — Zig (verified in this repository, works from Linux/Windows/macOS)

Zig ships Clang, MinGW-w64 and a resource compiler, so a complete Windows toolchain is one
download:

```bash
python3 -m pip install ziglang          # or download zig from https://ziglang.org/download/
./build/build_cross.sh                  # -> dist/AminKeyboardClipboard-x64.exe and -x86.exe
```

On Windows:

```bat
build\build_windows.bat
```

### Option B — Microsoft Visual C++ (script provided, **not executed here**)

Open an *x64 Native Tools Command Prompt for VS 2019/2022* and run:

```bat
build\build_msvc.bat
```

The sources are plain C++17 and use only the Win32 API, so MSVC should build them; the script has
not been run in this environment because no MSVC is available there.

### Verify the binaries

```bash
python3 -m pip install pefile
python3 build/verify_pe.py dist/AminKeyboardClipboard-x64.exe dist/AminKeyboardClipboard-x86.exe
```

It checks the GUI subsystem, minimum OS/subsystem versions, embedded icon + version + manifest
resources, that **no API newer than Windows 7 is imported**, and that no build-machine paths are
embedded.

### Run the portable tests

```bash
./tests/run_tests.sh          # 648 checks: text, JSON, clipboard store, settings, hotkeys, layouts
```

### Run the on-machine self test (on your Windows PC)

```
AminKeyboardClipboard.exe --selftest
```

It runs the same 648 core checks plus live checks for the clipboard listener, real `SendInput`
injection into an edit control, global shortcut availability, the HKCU startup value, the tray
icon and `IShellLink`, and writes a PASS/FAIL/SKIP report to
`%APPDATA%\AminKeyboardClipboard\selftest.log`.

---

## 6. Compatibility

| Item | Value |
| --- | --- |
| Target | Windows 10 (any build, including **1803 / 17134**), also runs on 7 SP1, 8.1, 11 |
| `_WIN32_WINNT` | `0x0601` — the compiler rejects anything newer |
| Architecture | x64 and x86 builds |
| Runtime | none (static C++ runtime; UCRT is part of Windows 10) |
| RAM footprint | a few MB; the UI is a single 32-bit DIB per window |
| Disk | ~0.8 MB program + a few KB of settings/history |
| DPI | system DPI aware (`dpiAware=true`), scales from 100 % to 200 % |
| UAC / admin | **never required**; manifest is `asInvoker` |

Deliberately **not** used, because it would break older Windows 10 builds: Direct2D/DirectWrite
with `d2d1_3` features, `SetProcessDpiAwarenessContext`, `GetDpiForWindow`,
`DwmSetWindowAttribute`, WinUI/XAML islands, system-backdrop APIs, Windows App SDK.
Persian shaping uses **Uniscribe** (`usp10.dll`), which exists on every Windows since 2000.

---

## 7. Privacy

* The clipboard listener only reads `CF_UNICODETEXT`/`CF_TEXT`, and only into a local JSON file.
* No network code at all: the executable imports no networking DLL.
* History is capped at 100 entries and 100 000 characters per entry.
* Passwords are *not* filtered specially — if you copy a password it will be stored like any other
  text, so use **Pause capture** (⏸) while working with sensitive data, and **Clear all** afterwards.
  This is stated plainly instead of pretending to detect passwords.
* Settings and history live in `%APPDATA%\AminKeyboardClipboard\` (`settings.json`, `history.json`,
  `app.log`, `selftest.log`).

---

## 8. Repository layout

```
amin-kbc/
├── src/core/        portable logic (UTF-8/16, JSON, store, settings, hotkeys, layouts, tests)
├── src/win/         Win32 UI (renderer, keyboard, clipboard, dashboard, platform, self test)
├── src/res/         icon, manifest, version resource
├── tests/           host test runner (same suite as --selftest)
├── build/           Zig / MSVC / Inno Setup build scripts + PE verifier
├── installer/       per-user installer, uninstaller, Inno Setup script
├── docs/            Persian installation guide, testing report, developer notes
└── dist/            compiled executables
```

## 9. License

Free to use, copy and modify for personal and commercial use.
