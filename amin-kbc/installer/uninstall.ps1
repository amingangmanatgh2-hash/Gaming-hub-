<#
.SYNOPSIS
    Uninstalls AMIN Keyboard & Clipboard (per-user, no administrator rights).

.DESCRIPTION
    Removes the program files, shortcuts, the start-with-Windows entry and the
    uninstall registry key. The clipboard history and settings in
    %APPDATA%\AminKeyboardClipboard are KEPT unless -RemoveUserData is given.
#>
[CmdletBinding()]
param([switch]$RemoveUserData)

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$installer = Join-Path $here 'install.ps1'

if (Test-Path -LiteralPath $installer) {
    if ($RemoveUserData) {
        & powershell.exe -ExecutionPolicy Bypass -NoProfile -File $installer -Uninstall -RemoveUserData
    } else {
        & powershell.exe -ExecutionPolicy Bypass -NoProfile -File $installer -Uninstall
    }
    exit $LASTEXITCODE
}

# Fallback when this file was copied without install.ps1.
$AppName      = 'AMIN Keyboard & Clipboard'
$InstallDir   = Join-Path $env:LOCALAPPDATA 'Programs\AminKeyboardClipboard'
$DataDir      = Join-Path $env:APPDATA 'AminKeyboardClipboard'
$RunKey       = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$UninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\AminKeyboardClipboard'
$StartMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'

Get-Process -Name 'AminKeyboardClipboard' -ErrorAction SilentlyContinue |
    ForEach-Object { $_.CloseMainWindow() | Out-Null }
Start-Sleep -Milliseconds 700
Get-Process -Name 'AminKeyboardClipboard' -ErrorAction SilentlyContinue | Stop-Process -Force

Remove-Item -LiteralPath (Join-Path $StartMenuDir "$AppName.lnk") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $StartMenuDir "$AppName (uninstall).lnk") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath('Desktop')) "$AppName.lnk") -Force -ErrorAction SilentlyContinue
Remove-ItemProperty -Path $RunKey -Name 'AminKeyboardClipboard' -Force -ErrorAction SilentlyContinue
Remove-Item -Path $UninstallKey -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
if ($RemoveUserData) {
    Remove-Item -LiteralPath $DataDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host '  Removed the app and its clipboard history.'
} else {
    Write-Host "  Removed the app. History and settings kept in: $DataDir"
}
