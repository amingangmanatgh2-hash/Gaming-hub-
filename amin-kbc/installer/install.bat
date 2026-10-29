@echo off
REM AMIN Keyboard & Clipboard - portable installer (no administrator rights).
REM Double-click this file, or run:  install.bat /autostart
setlocal
cd /d "%~dp0"
set "PSARGS=-ExecutionPolicy Bypass -NoProfile -File "%~dp0install.ps1""
if /I "%~1"=="/autostart" set "PSARGS=%PSARGS% -Autostart"
if /I "%~1"=="-autostart" set "PSARGS=%PSARGS% -Autostart"
powershell.exe %PSARGS%
if errorlevel 1 (
  echo.
  echo Installation failed. See the messages above.
  pause
  exit /b 1
)
echo.
echo Press any key to close this window.
pause >nul
endlocal
