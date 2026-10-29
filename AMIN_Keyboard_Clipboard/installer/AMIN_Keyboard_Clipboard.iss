; AMIN Keyboard & Clipboard — Inno Setup 6 installer script
; Requires: Inno Setup 6 (https://jrsoftware.org/isinfo.php)
; Build the app first (Release), then compile this script with ISCC.exe.

[Setup]
AppName=AMIN Keyboard & Clipboard
AppVersion=1.0.0
AppPublisher=AMIN
AppPublisherURL=https://github.com/amingangmanatgh2-hash/Gaming-hub-
DefaultDirName={pf}\AMIN Keyboard & Clipboard
DefaultGroupName=AMIN Keyboard & Clipboard
UninstallDisplayName=AMIN Keyboard & Clipboard
UninstallIcon={app}\Resources\amin.ico
SetupIconFile=..\AMIN_Keyboard_Clipboard\Resources\amin.ico
OutputDir=Output
OutputBaseFilename=AMIN_Keyboard_Clipboard_Setup_1.0.0
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x6c
ArchitecturesInstallIn64BitMode=x6c

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startup"; Description: "Start AMIN with Windows"; GroupDescription: "Installation Options:"

[Files]
Source: "..\AMIN_Keyboard_Clipboard\bin\Release\AMIN_Keyboard_Clipboard.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\AMIN_Keyboard_Clipboard\Resources\amin.ico"; DestDir: "{app}\Resources"; Flags: ignoreversion
Source: "license.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\AMIN Keyboard & Clipboard"; Filename: "{app}\AMIN_Keyboard_Clipboard.exe"
Name: "{autodesktop}\AMIN Keyboard & Clipboard"; Filename: "{app}\AMIN_Keyboard_Clipboard.exe"; Tasks: desktopicon
Name: "{commonstartup}\AMIN Keyboard & Clipboard"; Filename: "{app}\AMIN_Keyboard_Clipboard.exe"; Tasks: startup

[Run]
Filename: "{app}\AMIN_Keyboard_Clipboard.exe"; Description: "{cm:LaunchProgram,AMIN Keyboard & Clipboard}"; Flags: nowait postinstall skipifsilent
