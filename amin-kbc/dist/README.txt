AMIN Keyboard & Clipboard 1.0.0
===============================

AminKeyboardClipboard-x64.exe   64-bit build (recommended on any 64-bit Windows 10)
AminKeyboardClipboard-x86.exe   32-bit build (32-bit Windows, or if the x64 build misbehaves)

Quick start
-----------
1. Copy the .exe anywhere (Desktop, USB stick, C:\Apps\...). No installation needed.
2. Double-click it. If SmartScreen appears, choose "More info" -> "Run anyway"
   (the program has no code-signing certificate; that is the only reason for the warning).
3. Click "Show keyboard" / "نمايش صفحه کلید" and start typing into any application.
   Copy text with Ctrl+C and it appears in the clipboard history.

Install (optional, still no administrator rights)
-------------------------------------------------
Run install.bat from the ZIP. It copies the program to
%LOCALAPPDATA%\Programs\AminKeyboardClipboard, adds Start Menu and Desktop shortcuts and
registers an uninstall entry. Uninstall with uninstall.bat.

Data and privacy
----------------
Settings, clipboard history and logs are stored in %APPDATA%\AminKeyboardClipboard.
Text only, local only, no network code, no administrator rights.

Verify it on your own PC
------------------------
    AminKeyboardClipboard.exe --selftest

Writes %APPDATA%\AminKeyboardClipboard\selftest.log with PASS/FAIL/SKIP for every feature
(clipboard listener, SendInput into another window, global shortcut, startup entry, tray,
shortcut creation) and prints the same report.

Full Persian guide: docs/INSTALL-FA.md
Testing report:     docs/TESTING.md
Source code:        src/  (build scripts in build/)
