# Verification report — what was actually tested

This document exists so nobody has to guess which claims are verified and which are not.
It reflects the build that produced `dist/AminKeyboardClipboard-x64.exe` (v1.0.0).

**Build environment:** Linux (Debian 12, x86_64), 2 vCPU, Zig 0.17.0 (which bundles
Clang 20, MinGW-w64 and `lld`), GCC 12.2 for the host test build.
**No Windows machine, no MSVC, no Wine, and no Inno Setup were available**, and the sandbox
has no network access to `nuget.org`, `dot.net`, `winehq.org` or `jrsoftware.org`.

---

## 1. Verified here (with the commands used)

| # | Check | Command | Result |
| --- | --- | --- | --- |
| 1 | Portable logic suite (text/UTF-8, bidi detection, JSON, clipboard store, settings, hotkey parser, keyboard layouts, geometry) | `./tests/run_tests.sh` | **PASS — 648 checks, 0 failures** |
| 2 | Same suite compiled into the Windows binary (`--selftest`) | compiled | builds and links |
| 3 | Warning-free compile of all 16 translation units for x86_64 | `./build/build_cross.sh` | **0 warnings, 0 errors** (`-Wall -Wextra`) |
| 4 | Same for x86 | `./build/build_cross.sh` | **0 warnings, 0 errors** |
| 5 | PE header: GUI subsystem (no console window) | `build/verify_pe.py` | **PASS** (Subsystem = 2) |
| 6 | PE header: minimum OS 6.0 / subsystem 6.0 | `build/verify_pe.py` | **PASS** |
| 7 | Resources embedded: icon (7 sizes), group icon, version info, manifest | `build/verify_pe.py` | **PASS** |
| 8 | No API newer than Windows 7 imported | `build/verify_pe.py` (curated deny-list) | **PASS** — 240 (x64) / 238 (x86) imported functions, none on the deny-list |
| 9 | No build-machine paths inside the binary | `build/verify_pe.py` | **PASS** |
| 10 | Static runtime: no `libstdc++`/`libgcc`/`vcruntime` DLL needed | `build/verify_pe.py` (import table) | **PASS** — only system DLLs + in-box UCRT |
| 11 | Layout invariants: every key row sums to exactly 15 units, no overlapping keys, rows span the full width for widths 320–1600 px | test #1 | **PASS** |
| 12 | All 32 Persian letters + آ/ژ, Persian digits, ZWNJ, Persian punctuation reachable from the Persian layout | test #1 | **PASS** |
| 13 | Clipboard store: cap 100, newest-first, dedupe, delete, clear, search (Persian variants), JSON round-trip, corrupt-file tolerance | test #1 | **PASS** |
| 14 | Hotkey parser/formatter, Win+V detection, auto fallback chain validity | test #1 | **PASS** |
| 15 | JSON reader: escapes, surrogate pairs, BOM, depth limit, malformed input rejection | test #1 | **PASS** |
| 16 | Full core suite under **AddressSanitizer + UndefinedBehaviorSanitizer** (memory errors, leaks, UB) | `g++ -fsanitize=address,undefined` + `tests/test_logic.cpp` | **PASS — 648/648, no sanitizer report** |
| 17 | UI geometry + Persian shaping reviewed via `tools/render_preview.py` (two layout bugs found and fixed) | `python3 tools/render_preview.py` | **PASS** after fixes |
| 18 | Random-input fuzzing: 20 000 random byte strings, 20 000 *mutated valid* JSON documents (3 492 accepted and re-serialized), 20 000 random UTF-16 strings including lone surrogates and NULs | `./tests/run_fuzz.sh` | **PASS — no crash, no leak, no UB** |

The imported DLL list of the x64 build is exactly:
`ADVAPI32, COMCTL32, GDI32, KERNEL32, SHELL32, USER32, USP10, ole32` + `api-ms-win-crt-*` (UCRT,
part of Windows 10 since RTM). No networking DLL is imported at all.

## 2. Not verified here — please run `--selftest` on your PC

These require an actual Windows session and were **not** executed by me:

1. **Visual rendering.** No screen was ever produced. Colours, spacing, the Persian shape of the
   letters, hover/press animations and DPI scaling are implemented but unseen by me.
   This is the single biggest uncertainty.
2. **`SendInput` injection into a foreign application** (typing into Notepad, Word, the browser).
   The code path is standard, and `--selftest` re-tests it against a real EDIT control on your
   machine, but I could not press a key myself.
3. **Global shortcuts** (`RegisterHotKey`, Win+V conflict on 1809+, the Ctrl+Alt+V fallback).
   `--selftest` reports which combination this machine actually accepts.
4. **Clipboard listener** (`AddClipboardFormatListener` + `WM_CLIPBOARDUPDATE`). `--selftest`
   attaches and detaches a real listener.
5. **Tray icon** (`Shell_NotifyIcon`), including the Explorer-restart path.
6. **`IShellLink` desktop shortcut** and the HKCU Run value write.
7. **The installer scripts** (`install.bat`, `install.ps1`, `uninstall.ps1`) and the
   **Inno Setup script** (`amin-kbc.iss`) — none of them could be executed in a Linux sandbox.
   The app itself can create the shortcut and the startup entry from its own UI, and that path
   *is* covered by `--selftest`.
8. **The MSVC build script** (`build_msvc.bat`) — no MSVC available here.
9. **Windows 10 1803 specifically.** The binary avoids newer APIs by construction (pinned
   `_WIN32_WINNT=0x0601` plus the import-table audit), but 1803 hardware was not available to
   run it on.

## 3. What to expect from `--selftest` on your machine

```
AminKeyboardClipboard.exe --selftest
```

Writes `%APPDATA%\AminKeyboardClipboard\selftest.log` and prints the same text to the console it
was started from. Every line is `PASS`, `FAIL` or `SKIP`; the last line is the count.

Things the test *touches* (and restores):

* the clipboard (previous text is written back; if it was empty the clipboard is cleared),
* the HKCU Run value (previous state restored),
* the tray icon (added then removed),
* a temporary file `selftest-probe.json` in the data folder (deleted).

It never touches HKLM, never asks for elevation and never deletes user data.

## 4. Known limitations (by design, stated honestly)

* **Text only.** Images and files on the clipboard are not captured. The UI never claims otherwise.
* **No password detection.** Copied passwords are stored like any other text. The UI offers
  *Pause capture* and *Clear all* instead of pretending to be a password manager.
* **Elevated windows.** Windows blocks `SendInput` from a normal process into a window running
  elevated (UIPI). The app does not request elevation, so it cannot type into e.g. an
  administrator console. This is by design (the brief forbids requiring admin rights).
* **Win+V on 1809+** belongs to Windows; the app reports this and uses Ctrl+Alt+V.
* **Single global shortcut per action.** No custom key capture dialog; a fixed list is cycled in
  the wizard/UI (all combinations are validated by the tests).
* **DPI:** system DPI aware. On a multi-monitor setup with different scaling, the window will be
  bitmap-scaled by Windows on the secondary monitor.

## 4b. UI geometry and Persian shaping, checked without Windows

`tools/render_preview.py` parses `src/core/layout.cpp` and `src/win/amin.h`, re-implements
`core::compute_row()` and the dashboard's `compute_layout()` one-to-one, and renders PNGs into
`docs/preview/`. Running it found and fixed two real defects:

1. a row whose key count did not match its `RowDef` (caught while cross-checking the tables),
2. the privacy note in the clipboard tab overlapping the button row (layout arithmetic error).

The renderer also proved that every one of the 20 rows has the declared number of keys and sums to
exactly 15.00 key-units, and that the Persian presentation forms are correct.

It is still an **approximation** of the real GDI/Uniscribe output, so it does not replace the
visual check on Windows.

## 5. How to reproduce everything that *was* verified

```bash
# 1. Portable tests (needs g++ or clang++)
./tests/run_tests.sh

# 2. Cross-compile both architectures (needs python3 -m pip install ziglang)
./build/build_cross.sh

# 3. PE / compatibility audit (needs python3 -m pip install pefile)
python3 build/verify_pe.py dist/AminKeyboardClipboard-x64.exe dist/AminKeyboardClipboard-x86.exe
```
