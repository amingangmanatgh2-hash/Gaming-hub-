@echo off
REM ===========================================================================
REM  Builds the optional setup.exe with Inno Setup 6 (must be installed).
REM  Run build\build_windows.bat first so dist\*.exe exist.
REM ===========================================================================
setlocal
cd /d "%~dp0.."

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo Inno Setup 6 was not found.
  echo Download it from https://jrsoftware.org/isdl.php and install it, or use
  echo installer\install.bat for a per-user installation without an installer.
  exit /b 1
)

if not exist dist\AminKeyboardClipboard-x64.exe (
  echo dist\AminKeyboardClipboard-x64.exe is missing - run build\build_windows.bat first.
  exit /b 1
)

"%ISCC%" installer\amin-kbc.iss || exit /b 1
echo.
echo Setup file written to dist\
dir /b dist\AMIN-Keyboard-Clipboard-Setup-*.exe
endlocal
