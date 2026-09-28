#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef OutputDir
  #define OutputDir "..\..\artifacts\desktop"
#endif
[Setup]
AppId=ShinyPlayer.Desktop.Qt
AppName=Shiny Desktop
AppVersion=0.1.0
AppPublisher=Shiny Player contributors
DefaultDirName={localappdata}\Programs\ShinyDesktop
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=ShinyDesktop-0.1.0-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\ShinyDesktop.exe
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes
LicenseFile={#PackageDir}\share\shiny-desktop\LICENSE
[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{userprograms}\Shiny Desktop"; Filename: "{app}\bin\ShinyDesktop.exe"
[Run]
Filename: "{app}\bin\ShinyDesktop.exe"; Description: "Open Shiny Desktop"; Flags: nowait postinstall skipifsilent
; User preferences are deliberately retained; never delete playlists or media.
