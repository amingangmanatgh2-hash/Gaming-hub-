; Gaming Hub Optimizer — Inno Setup 6 installer
[Setup]
AppId={{8F4D2A61-3C7E-4B9A-A5D1-6E2F8C4B7019}
AppName=Gaming Hub Optimizer
AppVersion=1.0.0
AppPublisher=Gaming Hub
AppPublisherURL=https://github.com/amingangmanatgh2-hash/Gaming-hub-
DefaultDirName={pf}\Gaming Hub Optimizer
DefaultGroupName=Gaming Hub Optimizer
UninstallDisplayName=Gaming Hub Optimizer
SetupIconFile=..\GamingOptimizer\Resources\optimizer.ico
OutputDir=Output
OutputBaseFilename=GamingHubOptimizer_Setup_1.0.0
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\GamingOptimizer\bin\Release\GamingHubOptimizer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\GamingOptimizer\Resources\optimizer.ico"; DestDir: "{app}\Resources"; Flags: ignoreversion
Source: "license.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Gaming Hub Optimizer"; Filename: "{app}\GamingHubOptimizer.exe"
Name: "{autodesktop}\Gaming Hub Optimizer"; Filename: "{app}\GamingHubOptimizer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\GamingHubOptimizer.exe"; Description: "{cm:LaunchProgram,Gaming Hub Optimizer}"; Flags: nowait postinstall skipifsilent
