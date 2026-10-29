# AMIN Keyboard & Clipboard 🖥️

Persian/English virtual keyboard + clipboard history manager for **Windows 10 1803+**.
Built with **WPF / .NET Framework 4.8** — no admin rights, no network, no telemetry.

> **真实性说明 (Honest note):** This is complete, typed source code. The exe must be
> compiled on Windows (Visual Studio 2022 or the included GitHub Actions workflow).
> A pre-built binary is not included in the repo.

## Features

| Feature | Status |
|---|---|
| Persian (ISIRI 9147) + English (QWERTY) on-screen keyboard | ✅ |
| Type into the focused application via `SendInput` (Unicode) | ✅ |
| Floating always-on-top keyboard window | ✅ |
| Clipboard history (up to 100 items, JSON-persisted) | ✅ |
| Search / delete / clear / copy history | ✅ |
| Clipboard popup window (click item to copy) | ✅ |
| Global hotkeys `Ctrl+Alt+K` / `Ctrl+Alt+V` | ✅ |
| Start with Windows (real `.lnk` in Startup folder) | ✅ |
| Dark theme with purple/blue accents | ✅ |
| Privacy-first: all data local (`%APPDATA%\AMIN_Keyboard_Clipboard\history.json`) | ✅ |

## Requirements

- Windows 10 1803 or newer
- Visual Studio 2022 (Community) with the **.NET desktop development** workload (includes .NET Framework 4.8)
- Optional: Inno Setup 6 for the installer

## Build

```
1. Open AMIN_Keyboard_Clipboard.sln
2. Configuration = Release
3. Build → Build Solution (F6)
4. Run: bin\Release\AMIN_Keyboard_Clipboard.exe
```

## GitHub Actions

`.github/workflows/build-windows.yml` (repository root) builds the exe on a
Windows runner, zips it, and (best-effort) compiles the Inno Setup installer.
Trigger it via the **Actions** tab → "Build AMIN Keyboard & Clipboard (Windows)" → Run workflow.

## Hotkeys

- `Ctrl+Alt+K` — show/hide floating keyboard
- `Ctrl+Alt+V` — show/hide clipboard popup

## Layout

```
AMIN_Keyboard_Clipboard/
├── AMIN_Keyboard_Clipboard.sln
├── AMIN_Keyboard_Clipboard/          project folder
│   ├── App.xaml(.cs)                 application + global error handler
│   ├── MainWindow.xaml(.cs)          dashboard: keyboard / clipboard / settings
│   ├── KeyboardWindow.xaml(.cs)      floating always-on-top keyboard
│   ├── ClipboardWindow.xaml(.cs)     floating clipboard popup
│   ├── Controls/KeyboardPanel.*      Persian + English key layout (shared)
│   ├── Util/                         SendInput, typing target, clipboard store,
│   │                                 startup shortcut, watermark
│   └── Resources/                    DarkTheme.xaml + amin.ico
├── installer/                        Inno Setup script + MIT license
└── docs/راهنمای_نصب.md              Persian installation guide
```

## Limitations

- Text only — images/files are deliberately not captured.
- Persian shaping depends on the target app's text engine (fine in Notepad, Word, browsers).
- Multi-monitor focus edge cases may vary.

## License

MIT — see `installer/license.txt`.
