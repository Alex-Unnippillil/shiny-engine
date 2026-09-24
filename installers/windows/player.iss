; Shiny Player per-user setup. No NVIDIA model/runtime redistributed.
#ifndef PackageDir
  #error PackageDir must point at the built independent application package
#endif
[Setup]
AppId={{EF321531-DF40-48C4-B448-FA047E46B2E1}
AppName=Shiny Player
AppVersion=0.9.0
AppPublisher=Shiny Engine contributors
AppPublisherURL=https://github.com/Alex-Unnippillil/shiny-engine
DefaultDirName={localappdata}\Programs\ShinyPlayer
DefaultGroupName=Shiny Player
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
WizardStyle=modern
DisableProgramGroupPage=yes
OutputBaseFilename=ShinyPlayer-0.9.0-Windows-x64-Setup
OutputDir=..\..\artifacts\vlc
Compression=lzma2
SolidCompression=yes
ArchiveExtraction=full
UninstallDisplayIcon={app}\ShinyVlcPlayer.exe
LicenseFile=..\..\LICENSE
SetupLogging=yes
[Tasks]
Name: vlcruntime; Description: "Download official VLC 3.0.24 x64 runtime from VideoLAN (internet required; optional)"; GroupDescription: "Playback engine:"; Flags: unchecked
Name: desktopicon; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{tmp}\shiny-vlc-runtime\vlc-3.0.24\*"; DestDir: "{app}\runtime\vlc-3.0.24"; Flags: external ignoreversion recursesubdirs createallsubdirs; Tasks: vlcruntime
[Icons]
Name: "{group}\Shiny Player"; Filename: "{app}\ShinyVlcPlayer.exe"
Name: "{group}\Uninstall Shiny Player"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Shiny Player"; Filename: "{app}\ShinyVlcPlayer.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\ShinyVlcPlayer.exe"; Description: "Open Shiny Player"; Flags: nowait postinstall skipifsilent
[Code]
const
  VlcHash = 'fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323';
var
  Prepared: Boolean;
  Progress: TOutputProgressWizardPage;
procedure InitializeWizard;
begin
  Progress := CreateOutputProgressPage('Preparing the playback engine', 'Downloading and verifying the official VideoLAN archive. No NVIDIA models are downloaded.');
  WizardForm.WelcomeLabel2.Caption := 'Install Shiny Player and its native OpenDLSS-NR research worker.' + #13#10#13#10 + 'Includes opt-in DLSS-NR local research for model files you are authorized to use. No model weights are bundled or downloaded. Playback works independently; research output is unverified.' + #13#10#13#10 + 'For playback, select the optional official VLC download or use your existing compatible 64-bit VLC installation. No administrator rights are required.';
end;
function DownloadProgress(const Url, FileName: String; const ProgressValue, ProgressMax: Int64): Boolean;
begin
  Progress.SetProgress(ProgressValue, ProgressMax);
  Result := True;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Archive, Cached: String;
begin
  Result := '';
  if Prepared or not WizardIsTaskSelected('vlcruntime') then exit;
  Progress.Show;
  try
    try
      Archive := ExpandConstant('{tmp}\vlc-3.0.24-win64.zip');
      Cached := ExpandConstant('{param:VLCARCHIVE|}');
      if Cached <> '' then begin
        if not FileExists(Cached) then RaiseException('The specified cached VLC archive is missing.');
        if CompareText(GetSHA256OfFile(Cached), VlcHash) <> 0 then RaiseException('The cached VLC archive checksum does not match the official release.');
        if not FileCopy(Cached, Archive, False) then RaiseException('Could not stage the verified VLC archive.');
      end else
        DownloadTemporaryFile('https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.zip', 'vlc-3.0.24-win64.zip', VlcHash, @DownloadProgress);
      if CompareText(GetSHA256OfFile(Archive), VlcHash) <> 0 then RaiseException('VLC archive checksum failed.');
      Progress.SetText('Extracting the verified VLC archive...', 'Installing official runtime files locally; no system VLC installation is modified.');
      ForceDirectories(ExpandConstant('{tmp}\shiny-vlc-runtime'));
      ExtractArchive(Archive, ExpandConstant('{tmp}\shiny-vlc-runtime'), '', True, nil);
      if not FileExists(ExpandConstant('{tmp}\shiny-vlc-runtime\vlc-3.0.24\libvlc.dll')) then RaiseException('Official VLC archive layout is unexpected.');
      Prepared := True;
    except
      Result := GetExceptionMessage + #13#10 + 'Go back and deselect the VLC download to use an existing official installation, or retry with a valid connection.';
    end;
  finally
    Progress.Hide;
  end;
end;
