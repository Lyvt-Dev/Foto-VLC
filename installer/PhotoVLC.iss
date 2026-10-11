; Build: ISCC /DAppVersion=0.4.0 /DPayloadDir=<absolute dist/package> PhotoVLC.iss
#ifndef AppVersion
  #define AppVersion "0.4.0"
#endif
#ifndef PayloadDir
  #define PayloadDir "..\dist\package"
#endif
[Setup]
AppId={{B0A6C7F8-7276-4514-BC49-38A23B08A628}
AppName=Photo VLC
AppVersion={#AppVersion}
AppPublisher=Lyvt-Dev
AppPublisherURL=https://github.com/Lyvt-Dev/Foto-VLC
AppSupportURL=https://github.com/Lyvt-Dev/Foto-VLC/issues
AppUpdatesURL=https://github.com/Lyvt-Dev/Foto-VLC/releases
DefaultDirName={localappdata}\Programs\Photo VLC
DefaultGroupName=Photo VLC
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\PhotoVLC.exe
OutputDir=..\dist
OutputBaseFilename=setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Photo VLC"; Filename: "{app}\PhotoVLC.exe"
Name: "{autodesktop}\Photo VLC"; Filename: "{app}\PhotoVLC.exe"; Tasks: desktopicon

[Registry]
; Register an Open With candidate without changing anyone's default viewer.
Root: HKCU; Subkey: "Software\Classes\Applications\PhotoVLC.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "Photo VLC"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\PhotoVLC.exe\shell\open\command"; ValueType: string; ValueData: """{app}\PhotoVLC.exe"" ""%1"""

[Run]
Filename: "{app}\PhotoVLC.exe"; Description: "{cm:LaunchProgram,Photo VLC}"; Flags: nowait postinstall skipifsilent
