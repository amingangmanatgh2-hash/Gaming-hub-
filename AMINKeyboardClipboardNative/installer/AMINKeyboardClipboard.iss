; ---------------------------------------------------------------------------
;  AMIN Keyboard & Clipboard (native Win32) - Inno Setup 6 script
;
;  This installer has NO .NET Framework requirement: the application is a plain
;  native Win32 executable, statically linked against the C runtime. Windows
;  needs nothing installed to run it.
;
;  Build order:
;    1) build.bat x64      (and build.bat x86 with the 32-bit toolchain)
;    2) ISCC.exe AMINKeyboardClipboard.iss
;
;  Output: installer\Output\AMINKeyboardClipboardSetup.exe
; ---------------------------------------------------------------------------

#define MyAppName "AMIN Keyboard & Clipboard"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "AMIN"
#define MyAppExeName "AMINKeyboardClipboard.exe"

[Setup]
AppId={{7B3B74B8-FCFA-4CB2-A5C3-5C2C374C7B91}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion=1.1.0.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
DefaultDirName={autopf}\AMIN Keyboard Clipboard
DefaultGroupName=AMIN Keyboard Clipboard
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputDir=Output
OutputBaseFilename=AMINKeyboardClipboardSetup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x86 x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=6.1sp1
AppMutex=AMINKeyboardClipboard.SingleInstance
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupLogging=yes

[Languages]
Name: "farsi"; MessagesFile: "Farsi.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "اجرای برنامه هنگام ورود به ویندوز"; GroupDescription: "تنظیمات اختیاری:"

[Files]
; Exactly one of these two is installed, depending on the OS bitness, and both
; land under the same file name so shortcuts stay identical.
Source: "..\build\x64\{#MyAppExeName}"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion; Check: IsWin64
Source: "..\build\x86\{#MyAppExeName}"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion; Check: not IsWin64
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\AMIN Keyboard & Clipboard"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\AMIN Keyboard & Clipboard"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Written only when the user ticks the optional task; removed on uninstall.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "AMIN Keyboard Clipboard"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: startup; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; The clipboard history lives in %LocalAppData%\AMINKeyboardClipboard and is
; intentionally left in place on uninstall. It can be cleared from the app's
; Settings page ("حذف کامل داده‌های محلی").
