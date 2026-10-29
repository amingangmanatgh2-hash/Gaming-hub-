<#
.SYNOPSIS
    AMIN Keyboard & Clipboard - per-user installer (no administrator rights).

.DESCRIPTION
    Installs the application into %LOCALAPPDATA%\Programs\AminKeyboardClipboard,
    creates Start Menu and Desktop shortcuts and registers an uninstall entry in
    HKEY_CURRENT_USER only.

    NOTHING is written to HKLM, no UAC prompt appears, no Windows security
    setting is touched, and the user's files are never deleted or moved.
    The optional "start with Windows" entry uses the same HKCU Run value the
    application itself uses, so the checkbox inside the app stays in sync.

.PARAMETER NoDesktopShortcut
    Do not create a shortcut on the desktop.

.PARAMETER NoStartMenu
    Do not create a Start Menu shortcut.

.PARAMETER Autostart
    Also start the application with Windows (HKCU Run entry, per user).

.PARAMETER Uninstall
    Removes the application (shortcuts, Start Menu entry, Run entry, uninstall
    registry key and the installed program files). User data in
    %APPDATA%\AminKeyboardClipboard is kept unless -RemoveUserData is given.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File install.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File install.ps1 -Autostart
#>
[CmdletBinding()]
param(
    [switch]$NoDesktopShortcut,
    [switch]$NoStartMenu,
    [switch]$Autostart,
    [switch]$Uninstall,
    [switch]$RemoveUserData
)

$ErrorActionPreference = 'Stop'

$AppName        = 'AMIN Keyboard & Clipboard'
$AppNameFa      = 'صفحه کلید و کلیپ‌بورد امین'
$ExeName        = 'AminKeyboardClipboard.exe'
$InstallDir     = Join-Path $env:LOCALAPPDATA 'Programs\AminKeyboardClipboard'
$DataDir        = Join-Path $env:APPDATA 'AminKeyboardClipboard'
$RunKey         = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$RunValueName   = 'AminKeyboardClipboard'
$UninstallKey   = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\AminKeyboardClipboard'
$StartMenuDir   = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'
$StartMenuLink  = Join-Path $StartMenuDir "$AppName.lnk"
$DesktopLink    = Join-Path ([Environment]::GetFolderPath('Desktop')) "$AppName.lnk"
$ScriptDir      = Split-Path -Parent $MyInvocation.MyCommand.Path

function Write-Step($text) { Write-Host "  $text" }
function Write-Ok($text)   { Write-Host "  [OK] $text" -ForegroundColor Green }
function Write-Warn2($text){ Write-Host "  [!]  $text" -ForegroundColor Yellow }
function Write-Err($text)  { Write-Host "  [X]  $text" -ForegroundColor Red }

function New-Shortcut {
    param([string]$LinkPath, [string]$TargetPath, [string]$Arguments = '', [string]$WorkDir = '')

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($LinkPath)
    $shortcut.TargetPath = $TargetPath
    if ($Arguments) { $shortcut.Arguments = $Arguments }
    if ($WorkDir)   { $shortcut.WorkingDirectory = $WorkDir }
    $shortcut.Description = $AppName
    $shortcut.IconLocation = "$TargetPath,0"
    $shortcut.Save()
}

function Stop-RunningApp {
    $running = Get-Process -Name 'AminKeyboardClipboard' -ErrorAction SilentlyContinue
    if ($running) {
        Write-Step 'Stopping the running instance...'
        $running | ForEach-Object { $_.CloseMainWindow() | Out-Null }
        Start-Sleep -Milliseconds 700
        $still = Get-Process -Name 'AminKeyboardClipboard' -ErrorAction SilentlyContinue
        if ($still) { $still | Stop-Process -Force }
    }
}

# ---------------------------------------------------------------------------
#  Uninstall
# ---------------------------------------------------------------------------
if ($Uninstall) {
    Write-Host ''
    Write-Host "  $AppName - uninstall" -ForegroundColor Cyan
    Write-Host ''
    Stop-RunningApp

    foreach ($link in @($DesktopLink, $StartMenuLink,
                        (Join-Path $InstallDir "$AppName.lnk"))) {
        if (Test-Path -LiteralPath $link) {
            Remove-Item -LiteralPath $link -Force
            Write-Ok "removed $link"
        }
    }
    if (Test-Path $RunKey) {
        $item = Get-ItemProperty -Path $RunKey -Name $RunValueName -ErrorAction SilentlyContinue
        if ($item) {
            Remove-ItemProperty -Path $RunKey -Name $RunValueName -Force
            Write-Ok 'removed the start-with-Windows entry'
        }
    }
    if (Test-Path $UninstallKey) {
        Remove-Item -Path $UninstallKey -Recurse -Force
        Write-Ok 'removed the uninstall registry entry'
    }
    if (Test-Path -LiteralPath $InstallDir) {
        Remove-Item -LiteralPath $InstallDir -Recurse -Force
        Write-Ok "removed $InstallDir"
    }
    if ($RemoveUserData) {
        if (Test-Path -LiteralPath $DataDir) {
            Remove-Item -LiteralPath $DataDir -Recurse -Force
            Write-Ok "removed the clipboard history and settings in $DataDir"
        }
    } elseif (Test-Path -LiteralPath $DataDir) {
        Write-Warn2 "your clipboard history and settings were kept in: $DataDir"
        Write-Warn2 'delete that folder manually if you want them gone.'
    }
    Write-Host ''
    Write-Host '  Uninstall finished.' -ForegroundColor Green
    Write-Host ''
    exit 0
}

# ---------------------------------------------------------------------------
#  Install
# ---------------------------------------------------------------------------
Write-Host ''
Write-Host "  $AppName" -ForegroundColor Cyan
Write-Host "  $AppNameFa" -ForegroundColor Cyan
Write-Host ''

# 1. Find the executable next to this script.
$is64 = [Environment]::Is64BitOperatingSystem
$candidates = @()
if ($is64) {
    $candidates += Join-Path $ScriptDir 'AminKeyboardClipboard-x64.exe'
    $candidates += Join-Path $ScriptDir '..\dist\AminKeyboardClipboard-x64.exe'
}
$candidates += Join-Path $ScriptDir 'AminKeyboardClipboard-x86.exe'
$candidates += Join-Path $ScriptDir '..\dist\AminKeyboardClipboard-x86.exe'
$candidates += Join-Path $ScriptDir 'AminKeyboardClipboard.exe'

$source = $null
foreach ($c in $candidates) {
    if (Test-Path -LiteralPath $c) { $source = (Resolve-Path -LiteralPath $c).Path; break }
}
if (-not $source) {
    Write-Err 'AminKeyboardClipboard-x64.exe was not found next to this script.'
    Write-Err 'Extract the whole ZIP archive and run install.bat again.'
    exit 1
}
Write-Step "source: $source"
Write-Step ("processor: {0} ({1})" -f $env:PROCESSOR_ARCHITECTURE, $(if ($is64) { '64-bit Windows' } else { '32-bit Windows' }))

# 2. Stop a running copy.
Stop-RunningApp

# 3. Copy the files.
if (-not (Test-Path -LiteralPath $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}
$targetExe = Join-Path $InstallDir $ExeName
if ((Test-Path -LiteralPath $targetExe) -and ($source -ne $targetExe) -and
    ((Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $targetExe).Hash)) {
    Write-Ok 'the installed version is already up to date'
} else {
    Copy-Item -LiteralPath $source -Destination $targetExe -Force
    Write-Ok "installed to $targetExe"
}

# Ship the uninstaller next to the program so it can be found later.
foreach ($extra in @('uninstall.ps1', 'uninstall.bat', 'README.md', 'INSTALL-FA.md')) {
    $p = Join-Path $ScriptDir $extra
    if (Test-Path -LiteralPath $p) {
        Copy-Item -LiteralPath $p -Destination (Join-Path $InstallDir $extra) -Force
    }
}

# 4. Shortcuts.
if (-not $NoStartMenu) {
    if (-not (Test-Path -LiteralPath $StartMenuDir)) {
        New-Item -ItemType Directory -Path $StartMenuDir -Force | Out-Null
    }
    New-Shortcut -LinkPath $StartMenuLink -TargetPath $targetExe -WorkDir $InstallDir
    Write-Ok 'Start Menu shortcut created'
    $uninstLink = Join-Path $StartMenuDir "$AppName (uninstall).lnk"
    $uninstScript = Join-Path $InstallDir 'uninstall.ps1'
    if (Test-Path -LiteralPath $uninstScript) {
        New-Shortcut -LinkPath $uninstLink `
            -TargetPath 'powershell.exe' `
            -Arguments ('-ExecutionPolicy Bypass -NoProfile -File "{0}"' -f $uninstScript) `
            -WorkDir $InstallDir
        Write-Ok 'uninstall shortcut created in the Start Menu'
    }
}
if (-not $NoDesktopShortcut) {
    New-Shortcut -LinkPath $DesktopLink -TargetPath $targetExe -WorkDir $InstallDir
    Write-Ok 'desktop shortcut created'
}

# 5. Optional autostart (same HKCU value the app itself toggles).
if ($Autostart) {
    if (-not (Test-Path $RunKey)) { New-Item -Path $RunKey -Force | Out-Null }
    Set-ItemProperty -Path $RunKey -Name $RunValueName -Value ('"{0}" --minimized' -f $targetExe)
    Write-Ok 'start with Windows enabled (HKCU, current user only)'
}

# 6. Uninstall entry so the app shows up in "Apps & features".
if (-not (Test-Path $UninstallKey)) { New-Item -Path $UninstallKey -Force | Out-Null }
$sizeKb = [math]::Round((Get-Item -LiteralPath $targetExe).Length / 1KB)
$uninstScriptPath = Join-Path $InstallDir 'uninstall.ps1'
$uninstallString = 'powershell.exe -ExecutionPolicy Bypass -NoProfile -File "{0}"' -f $uninstScriptPath
$version = '1.0.0'
Set-ItemProperty -Path $UninstallKey -Name 'DisplayName'     -Value $AppName
Set-ItemProperty -Path $UninstallKey -Name 'DisplayVersion'  -Value $version
Set-ItemProperty -Path $UninstallKey -Name 'Publisher'       -Value 'AMIN'
Set-ItemProperty -Path $UninstallKey -Name 'DisplayIcon'     -Value $targetExe
Set-ItemProperty -Path $UninstallKey -Name 'InstallLocation' -Value $InstallDir
Set-ItemProperty -Path $UninstallKey -Name 'UninstallString' -Value $uninstallString
Set-ItemProperty -Path $UninstallKey -Name 'QuietUninstallString' -Value ($uninstallString + ' -Uninstall')
Set-ItemProperty -Path $UninstallKey -Name 'NoModify'        -Value 1 -Type DWord
Set-ItemProperty -Path $UninstallKey -Name 'NoRepair'        -Value 1 -Type DWord
Set-ItemProperty -Path $UninstallKey -Name 'EstimatedSize'   -Value ([int]$sizeKb) -Type DWord
Write-Ok 'registered in "Apps & features" (per user)'

# 7. Verify the installation.
Write-Host ''
$ok = $true
if (-not (Test-Path -LiteralPath $targetExe)) { Write-Err 'the executable is missing'; $ok = $false }
if (-not $NoStartMenu -and -not (Test-Path -LiteralPath $StartMenuLink)) {
    Write-Err 'the Start Menu shortcut is missing'; $ok = $false
}
if ($ok) { Write-Ok 'verification passed' }

Write-Host ''
if ($ok) {
    Write-Host '  Done. Start the app from the Start Menu: ' -NoNewline
    Write-Host $AppName -ForegroundColor White
    Write-Host ''
    Write-Host '  Persian:' -ForegroundColor DarkGray
    Write-Host '  نصب کامل شد. برنامه را از منوی استارت اجرا کنید.' -ForegroundColor Gray
    Write-Host '  برای بررسی سلامت روی سیستم خودتان این فرمان را اجرا کنید:' -ForegroundColor Gray
    Write-Host ("  `"{0}`" --selftest" -f $targetExe) -ForegroundColor Gray
    Write-Host ''
    Write-Host '  Nothing was written outside HKCU and your user folders.' -ForegroundColor DarkGray
    Write-Host ''
    Start-Sleep -Seconds 1
    exit 0
} else {
    Write-Err 'installation finished with errors'
    exit 1
}
