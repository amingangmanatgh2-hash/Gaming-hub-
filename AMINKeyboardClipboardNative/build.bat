@echo off
REM ---------------------------------------------------------------------------
REM  AMIN Keyboard & Clipboard - native build with the MSVC toolchain.
REM
REM  Run from a "Developer Command Prompt for VS" (or from CI, where the
REM  environment is already set up). No .NET SDK, no NuGet, nothing else.
REM
REM  Output: build\AMINKeyboardClipboard.exe
REM ---------------------------------------------------------------------------
setlocal

if "%~1"=="" (set ARCH=x64) else (set ARCH=%~1)
if not exist build\%ARCH% mkdir build\%ARCH%

echo [1/2] Compiling resources for %ARCH%...
rc /nologo /fo build\%ARCH%\app.res src\app.rc
if errorlevel 1 goto :failed

echo [2/2] Compiling and linking...
cl /nologo /MT /O2 /W3 /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
   /Fo:build\%ARCH%\ /Fe:build\%ARCH%\AMINKeyboardClipboard.exe ^
   src\main.c src\keyboard.c src\clipboard.c src\settings.c ^
   build\%ARCH%\app.res ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:NO ^
   user32.lib gdi32.lib shell32.lib shlwapi.lib advapi32.lib
if errorlevel 1 goto :failed

del /q *.obj >nul 2>&1

echo.
echo OK: build\%ARCH%\AMINKeyboardClipboard.exe
exit /b 0

:failed
echo.
echo BUILD FAILED
exit /b 1
