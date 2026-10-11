; Build: ISCC /DAppVersion=0.7.1 /DPayloadDir=<absolute dist/package> PAFPhotos.iss
#ifndef AppVersion
  #define AppVersion "0.7.1"
#endif
#ifndef PayloadDir
  #define PayloadDir "..\dist\package"
#endif
[Setup]
AppId={{B0A6C7F8-7276-4514-BC49-38A23B08A628}
AppName=PAF-Photos
AppVersion={#AppVersion}
AppPublisher=Lyvt-Dev
AppPublisherURL=https://github.com/Lyvt-Dev/PAF-Photos
AppSupportURL=https://github.com/Lyvt-Dev/PAF-Photos/issues
AppUpdatesURL=https://github.com/Lyvt-Dev/PAF-Photos/releases
DefaultDirName={localappdata}\Programs\PAF-Photos
DefaultGroupName=PAF-Photos
UsePreviousGroup=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\PAFPhotos.exe
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
Name: "{group}\PAF-Photos"; Filename: "{app}\PAFPhotos.exe"
Name: "{autodesktop}\PAF-Photos"; Filename: "{app}\PAFPhotos.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\PAFPhotos"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
; Register an Open With candidate without changing anyone's default viewer.
Root: HKCU; Subkey: "Software\Classes\Applications\PAFPhotos.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "PAF-Photos"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\PAFPhotos.exe\shell\open\command"; ValueType: string; ValueData: """{app}\PAFPhotos.exe"" ""%1"""

[Run]
Filename: "{app}\PAFPhotos.exe"; Description: "{cm:LaunchProgram,PAF-Photos}"; Flags: nowait postinstall skipifsilent

[InstallDelete]
Type: files; Name: "{app}\PhotoVLC.exe"
Type: files; Name: "{userprograms}\Photo VLC\Photo VLC.lnk"
Type: files; Name: "{userdesktop}\Photo VLC.lnk"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var OldCommand: String;
begin
  if CurStep = ssPostInstall then
    if RegQueryStringValue(HKCU, 'Software\Classes\Applications\PhotoVLC.exe\shell\open\command', '', OldCommand) then
      if CompareText(OldCommand, '"' + ExpandConstant('{app}') + '\PhotoVLC.exe" "%1"') = 0 then
        RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Classes\Applications\PhotoVLC.exe');
end;
