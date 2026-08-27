; The CI job fills dist/ with executable, SDL runtime and Project Eon's own
; cards. Original Millennium/Deuteros archives are intentionally excluded.
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif

[Setup]
AppId={{0B67A8E0-3C32-4694-9866-AD5A7C9A7562}
AppName=Project Eon
AppVersion={#MyAppVersion}
AppPublisher=Project Eon contributors
AppPublisherURL=https://github.com/yeager/project-eon
DefaultDirName={autopf}\Project Eon
DefaultGroupName=Project Eon
OutputDir=installer
OutputBaseFilename=Project-Eon-{#MyAppVersion}-windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes

[Files]
Source: "dist\project-eon.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\SDL3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\SDL3_image.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\zlib*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\assets\*"; DestDir: "{app}\assets"; Flags: recursesubdirs ignoreversion
Source: "dist\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; The program scans this directory in place. The installer never places game
; data here and never unpacks a user archive.
Name: "{app}\data"

[Icons]
Name: "{group}\Project Eon"; Filename: "{app}\project-eon.exe"
Name: "{autodesktop}\Project Eon"; Filename: "{app}\project-eon.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"

[Run]
Filename: "{app}\project-eon.exe"; Description: "Launch Project Eon"; Flags: nowait postinstall skipifsilent
