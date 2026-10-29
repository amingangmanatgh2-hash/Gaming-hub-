@echo off
REM AMIN Keyboard & Clipboard - uninstaller (keeps your clipboard history).
setlocal
cd /d "%~dp0"
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%~dp0uninstall.ps1"
echo.
echo Press any key to close this window.
pause >nul
endlocal
