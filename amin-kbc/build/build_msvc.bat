@echo off
REM ===========================================================================
REM  Alternative build with Microsoft Visual C++ (Visual Studio 2019/2022).
REM  The Zig build (build_windows.bat) is the path that was actually verified.
REM  Open a "x64 Native Tools Command Prompt" and run build\build_msvc.bat
REM ===========================================================================
setlocal
cd /d "%~dp0.."

if not exist build mkdir build
if not exist dist mkdir dist

echo ==^> resources
rc /nologo /fo build\app.res src\res\app.rc || exit /b 1

echo ==^> compile + link
cl /nologo /std:c++17 /W4 /O2 /EHsc /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
  /Fe:dist\AminKeyboardClipboard.exe ^
  src\core\text.cpp src\core\json.cpp src\core\store.cpp src\core\settings.cpp ^
  src\core\hotkey.cpp src\core\layout.cpp src\core\selftest_core.cpp ^
  src\win\gfx.cpp src\win\util_win.cpp src\win\platform.cpp src\win\app.cpp ^
  src\win\ui_keyboard.cpp src\win\ui_clipboard.cpp src\win\ui_dashboard.cpp ^
  src\win\selftest_win.cpp src\win\main.cpp ^
  build\app.res ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
  user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib comctl32.lib usp10.lib || exit /b 1

echo ==^> done: dist\AminKeyboardClipboard.exe
endlocal
