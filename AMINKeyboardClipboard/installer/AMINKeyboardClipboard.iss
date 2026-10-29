; ---------------------------------------------------------------------------
;  AMIN Keyboard & Clipboard — Inno Setup 6 script
;
;  Build order:
;    1) dotnet build AMINKeyboardClipboard.csproj -c Release
;       (or build the solution in Visual Studio, Release | Any CPU)
;    2) Compile this script:  ISCC.exe installer\AMINKeyboardClipboard.iss
;
;  Output: installer\Output\AMINKeyboardClipboardSetup.exe
;
;  The installer installs per-user by default (PrivilegesRequired=lowest), so it
;  never triggers a UAC prompt, and it only writes a Run entry when the user
;  ticks the "start with Windows" task.
; ---------------------------------------------------------------------------

#define MyAppName "AMIN Keyboard & Clipboard"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AMIN"
#define MyAppExeName "AMIN Keyboard Clipboard.exe"
#define MyAppFolder "AMINKeyboardClipboard"
#define BuildDir "..\bin\Release\net48"

[Setup]
AppId={{7B3B74B8-FCFA-4CB2-A5C3-5C2C374C7B91}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion=1.0.0.0
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
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\{#MyAppExeName}.config"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\AMIN Keyboard & Clipboard"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\AMIN Keyboard & Clipboard"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Only written when the user selects the optional task above; removed on uninstall.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "AMIN Keyboard Clipboard"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: startup; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; Note: the user's clipboard history lives in
;   %LocalAppData%\AMINKeyboardClipboard
; and is deliberately left in place on uninstall. It can be removed from the
; app's "تنظیمات و حریم خصوصی" tab.

[Code]
const
  Net48Release = 528040;  { .NET Framework 4.8 release key }

function NetFramework48OrLater(): Boolean;
var
  Release: Cardinal;
begin
  Result :=
    RegQueryDWordValue(HKEY_LOCAL_MACHINE,
      'SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full', 'Release', Release) and
    (Release >= Net48Release);
end;

function InitializeSetup(): Boolean;
begin
  Result := True;

  if not NetFramework48OrLater() then
  begin
    if MsgBox('این برنامه به .NET Framework 4.8 یا جدیدتر نیاز دارد.' + #13#10 +
              'نسخه نصب‌شده قدیمی‌تر است یا پیدا نشد.' + #13#10#13#10 +
              'This application requires .NET Framework 4.8 or later.' + #13#10#13#10 +
              'ادامه می‌دهید؟ / Continue anyway?',
              mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
  end;
end;
