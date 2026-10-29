; ============================================================================
;  AMIN Keyboard & Clipboard - Inno Setup 6 script
;
;  This produces a real Windows installer (setup.exe). It is OPTIONAL: the app
;  is portable and works without any installation, and installer\install.bat
;  installs it per-user without administrator rights.
;
;  IMPORTANT - honesty note: this Inno Setup script could NOT be compiled in the
;  environment where the app was written (Inno Setup is a Windows program).
;  Build it on Windows with:
;      "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\amin-kbc.iss
;  or run build\build_installer_windows.bat which detects ISCC automatically.
;
;  The installer is per-user (PrivilegesRequired=lowest): no UAC prompt, nothing
;  is written to HKLM, and the user's own files are never touched.
; ============================================================================
#define AppName        "AMIN Keyboard & Clipboard"
#define AppVersion     "1.0.0"
#define AppPublisher   "AMIN"
#define AppExeName     "AminKeyboardClipboard.exe"

[Setup]
AppId={{8E3C0F5A-2C74-4C0F-9E2B-6F6F2F1A7C31}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\AminKeyboardClipboard
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\dist
OutputBaseFilename=AMIN-Keyboard-Clipboard-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
ArchitecturesAllowed=x86 x64
ArchitecturesInstallIn64BitMode=
MinVersion=6.1sp1
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes

[Languages]
; Persian is not a built-in Inno Setup language; the wizard stays in English.
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce
Name: "autostart"; Description: "Start with Windows (current user only)"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
; 32-bit build works on both x86 and x64 Windows; the 64-bit build is installed
; when available and the machine is 64-bit.
Source: "..\dist\AminKeyboardClipboard-x64.exe"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion; Check: IsWin64 and FileExists(ExpandConstant('{src}\..\dist\AminKeyboardClipboard-x64.exe'))
Source: "..\dist\AminKeyboardClipboard-x86.exe"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion; Check: not IsWin64
Source: "..\docs\INSTALL-FA.md"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
; Same value name the application itself uses for its checkbox.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "AminKeyboardClipboard"; ValueData: """{app}\{#AppExeName}"" --minimized"; \
    Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Run {#AppName}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\{#AppExeName}"; Parameters: "--selftest"; Description: "Run the self test"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
; Only files this installer created; user data in %APPDATA% is preserved.
Type: filesandordirs; Name: "{app}"
