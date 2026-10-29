# ============================================================
#  Gaming Hub Optimizer - DISABLE ALL (back to Windows defaults)
#  Run: right-click > Run with PowerShell   (or:  powershell -ExecutionPolicy Bypass -File .\Disable-All.ps1)
#  No backup needed - writes stock Windows values directly.
# ============================================================

# --- Self-elevate to Administrator ---
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell "-ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

function Set-Reg($Path, $Name, $Value, $Type = 'DWord') {
    if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }
    New-ItemProperty -Path $Path -Name $Name -Value $Value -PropertyType $Type -Force | Out-Null
    Write-Host "  [OK] $Name = $Value" -ForegroundColor Green
}
function Del-Reg($Path, $Name) {
    Remove-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue
    Write-Host "  [OK] $Name removed (default)" -ForegroundColor Green
}

Write-Host "`n=== DISABLING ALL OPTIMIZATIONS ===`n" -ForegroundColor Red

Write-Host "[1] Game Mode -> OFF"
Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 0
Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 0

Write-Host "[2] Game DVR / background recording -> ON (default)"
Set-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 1
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 1
Del-Reg 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\GameDVR' 'AllowGameDVR'

Write-Host "[3] HAGS -> Windows default"
Del-Reg 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode'

Write-Host "[4] Power plan -> Balanced"
powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e
Write-Host "  [OK] Balanced active" -ForegroundColor Green

Write-Host "[5] Game GPU/CPU priority -> default"
Del-Reg 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games' 'GPU Priority'
Del-Reg 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games' 'Priority'

Write-Host "[6] Visual effects -> Let Windows choose + transparency ON"
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 0
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' 'EnableTransparency' 1

Write-Host "[7] Mouse acceleration -> ON (default)"
Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '1' 'String'
Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '6' 'String'
Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '10' 'String'

Write-Host "[8] Fullscreen Optimizations -> default"
Del-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_FSEBehaviorMode'

Write-Host "[9] Notifications -> ON"
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Notifications\Settings' 'NOC_GLOBAL_SETTING_TOASTS_ENABLED' 1

Write-Host "`nDONE. Everything is back to Windows defaults. RESTART your PC once.`n" -ForegroundColor Cyan
Read-Host 'Press Enter to close'
