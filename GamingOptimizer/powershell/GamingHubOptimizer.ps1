# ============================================================
#  Gaming Hub Optimizer - PowerShell Edition v1.0.1
#  Windows 10/11 Gaming Optimizer: FPS + Quality + Display
#
#  Run:
#    powershell -ExecutionPolicy Bypass -File .\GamingHubOptimizer.ps1
#  (auto-elevates to Administrator; every Apply first backs up
#   registry values to %APPDATA%\GamingHubOptimizer\backup-*.txt)
# ============================================================

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell "-ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

$BackupDir = Join-Path $env:APPDATA 'GamingHubOptimizer'
New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
$script:Backup = @()

function Backup-Reg($Path, $Name) {
    $v = (Get-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue).$Name
    $script:Backup += "$Path|$Name|$v"
}
function Set-Reg($Path, $Name, $Value, $Type = 'DWord') {
    Backup-Reg $Path $Name
    if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }
    New-ItemProperty -Path $Path -Name $Name -Value $Value -PropertyType $Type -Force | Out-Null
    Write-Host "  [OK] $Name = $Value" -ForegroundColor Green
}
function Del-Reg($Path, $Name) {
    Backup-Reg $Path $Name
    Remove-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue
    Write-Host "  [OK] $Name removed" -ForegroundColor Green
}
function Save-Backup {
    $f = Join-Path $BackupDir ("backup-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".txt")
    $script:Backup | Set-Content $f
    $script:Backup | Set-Content (Join-Path $BackupDir 'last-backup.txt')
    Write-Host "Backup saved: $f" -ForegroundColor Yellow
}

# ---------------- Profiles ----------------
function Apply-FPS {
    Write-Host "`n--- MAX FPS ---" -ForegroundColor Magenta
    Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 1
    Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1
    Set-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 0
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 0
    Set-Reg 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\GameDVR' 'AllowGameDVR' 0
    Set-Reg 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode' 2
    powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61 2>$null
    powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61
    Write-Host "  [OK] Power plan = Ultimate Performance" -ForegroundColor Green
    Set-Reg 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games' 'GPU Priority' 8
    Set-Reg 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games' 'Priority' 6
    Set-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_FSEBehaviorMode' 2
    Save-Backup
    Write-Host "Done. RESTART once (HAGS + power plan)." -ForegroundColor Cyan
}

function Apply-Quality {
    Write-Host "`n--- ULTRA QUALITY + DISPLAY ---" -ForegroundColor Magenta
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 1
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' 'EnableTransparency' 1
    Set-Reg 'HKCU:\Control Panel\Desktop' 'FontSmoothing' '2' 'String'
    Set-Reg 'HKCU:\Control Panel\Desktop' 'FontSmoothingType' 2
    Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '0' 'String'
    Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '0' 'String'
    Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '0' 'String'
    Save-Backup
    Show-Display
    Write-Host "Done." -ForegroundColor Cyan
}

function Disable-All {
    Write-Host "`n--- DISABLE ALL -> WINDOWS DEFAULTS ---" -ForegroundColor Red
    Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 0
    Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 0
    Set-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 1
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 1
    Del-Reg 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\GameDVR' 'AllowGameDVR'
    Del-Reg 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode'
    powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e
    Write-Host "  [OK] Power plan = Balanced" -ForegroundColor Green
    Del-Reg 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games' 'GPU Priority'
    Del-Reg 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks\Games' 'Priority'
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 0
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' 'EnableTransparency' 1
    Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '1' 'String'
    Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '6' 'String'
    Set-Reg 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '10' 'String'
    Del-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_FSEBehaviorMode'
    Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Notifications\Settings' 'NOC_GLOBAL_SETTING_TOASTS_ENABLED' 1
    Save-Backup
    Write-Host "All reverted to Windows defaults. RESTART once." -ForegroundColor Cyan
}

function Restore-Backup {
    $f = Join-Path $BackupDir 'last-backup.txt'
    if (-not (Test-Path $f)) { Write-Host "No backup found." -ForegroundColor Red; return }
    foreach ($line in Get-Content $f) {
        $p = $line -split '\|', 3
        if ($p.Count -lt 3) { continue }
        if ($p[2] -eq '') { Remove-ItemProperty -Path $p[0] -Name $p[1] -ErrorAction SilentlyContinue }
        else { New-ItemProperty -Path $p[0] -Name $p[1] -Value $p[2] -PropertyType DWord -Force -ErrorAction SilentlyContinue | Out-Null }
    }
    Write-Host "Backup restored. RESTART once." -ForegroundColor Cyan
}

function Clean-Cache {
    $targets = @($env:TEMP, "$env:LOCALAPPDATA\Temp", "$env:LOCALAPPDATA\D3DSCache",
                 "$env:LOCALAPPDATA\NVIDIA\DXCache", "$env:LOCALAPPDATA\AMD\DxCache")
    $freed = 0
    foreach ($t in $targets) {
        if (Test-Path $t) {
            Get-ChildItem $t -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
                $len = $_.Length
                Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
                if (-not (Test-Path $_.FullName)) { $freed += $len }
            }
            Write-Host "  [OK] Cleaned $t" -ForegroundColor Green
        }
    }
    Clear-RecycleBin -Force -ErrorAction SilentlyContinue
    Write-Host ("Freed: {0} MB" -f [math]::Round($freed / 1MB, 1)) -ForegroundColor Cyan
}

function Show-Info {
    $os  = (Get-CimInstance Win32_OperatingSystem).Caption
    $cpu = (Get-CimInstance Win32_Processor).Name
    $ram = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
    $gpu = Get-CimInstance Win32_VideoController | Select-Object -First 1
    Write-Host "`nOS:  $os`nCPU: $cpu`nRAM: $ram GB"
    Write-Host "GPU: $($gpu.Name)"
    Write-Host "Display: $($gpu.CurrentHorizontalResolution) x $($gpu.CurrentVerticalResolution) @ $($gpu.CurrentRefreshRate) Hz"
    if ($gpu.CurrentRefreshRate -lt 75) { Write-Host "WARNING: refresh rate under 75 Hz - raise it in Settings > Display > Advanced!" -ForegroundColor Yellow }
}

function Show-Display {
    Show-Info
    Write-Host "`nChecklist: Native resolution | Max refresh rate | In-game V-Sync OFF + FPS cap = Hz | G-Sync/FreeSync ON | Monitor Game Mode ON" -ForegroundColor Yellow
}

# ---------------- Menu ----------------
do {
    Write-Host "`n========== Gaming Hub Optimizer (PowerShell) ==========" -ForegroundColor Cyan
    Write-Host " 1) System info + Display report"
    Write-Host " 2) Apply MAX FPS profile"
    Write-Host " 3) Apply ULTRA QUALITY + Display profile"
    Write-Host " 4) Apply BOTH (FPS + Quality)"
    Write-Host " 5) Clean Temp + Shader cache + Recycle Bin"
    Write-Host " 6) Restore last backup"
    Write-Host " 7) DISABLE ALL -> Windows defaults" -ForegroundColor Red
    Write-Host " 0) Exit"
    $c = Read-Host "`nChoose"
    switch ($c) {
        '1' { Show-Info }
        '2' { Apply-FPS }
        '3' { Apply-Quality }
        '4' { Apply-FPS; Apply-Quality }
        '5' { Clean-Cache }
        '6' { Restore-Backup }
        '7' { if ((Read-Host 'Revert EVERYTHING to Windows defaults? (y/n)') -eq 'y') { Disable-All } }
    }
} while ($c -ne '0')
