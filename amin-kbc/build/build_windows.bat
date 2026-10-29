@echo off
REM ===========================================================================
REM  Build AMIN Keyboard & Clipboard on Windows with Zig (recommended).
REM
REM  1. Download Zig from https://ziglang.org/download/ and add zig.exe to PATH
REM  2. Open cmd.exe in the repository root and run:  build\build_windows.bat
REM
REM  Output: dist\AminKeyboardClipboard-x64.exe
REM ===========================================================================
setlocal
cd /d "%~dp0.."

where zig >nul 2>nul
if errorlevel 1 (
  echo ERROR: zig.exe was not found in PATH.
  echo Download it from https://ziglang.org/download/ and add it to PATH.
  exit /b 1
)

if not exist build mkdir build
if not exist dist mkdir dist

echo ==^> compiling resources
zig rc src\res\app.rc build\app.res || exit /b 1

echo ==^> compiling application
zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -municode -static ^
  -Wall -Wextra -Wno-unused-parameter -finput-charset=UTF-8 ^
  -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
  -Wl,--subsystem,windows ^
  -lusp10 -lcomctl32 -lshell32 -lole32 -luser32 -lgdi32 -ladvapi32 ^
  -o dist\AminKeyboardClipboard-x64.exe ^
  src\core\text.cpp src\core\json.cpp src\core\store.cpp src\core\settings.cpp ^
  src\core\hotkey.cpp src\core\layout.cpp src\core\selftest_core.cpp ^
  src\win\gfx.cpp src\win\util_win.cpp src\win\platform.cpp src\win\app.cpp ^
  src\win\ui_keyboard.cpp src\win\ui_clipboard.cpp src\win\ui_dashboard.cpp ^
  src\win\selftest_win.cpp src\win\main.cpp ^
  build\app.res || exit /b 1

echo ==^> done: dist\AminKeyboardClipboard-x64.exe
endlocal
