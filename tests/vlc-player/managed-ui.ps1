param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$Runtime,[Parameter(Mandatory=$true)][string]$Fixture,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
# This test owns only a newly created default store on a disposable Windows runner.
# Never replace or delete an existing user's library store when run manually.
$store = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'ShinyPlayer/LibraryStore'
if (Test-Path $store) { throw 'Managed UI test requires an absent default library store; existing data was left untouched.' }
$cli = Join-Path (Split-Path $Executable -Parent) 'ShinyLibraryManagerCli.exe'
$ownedStore = $false
$proc = $null
$main = [IntPtr]::Zero
$preview = [IntPtr]::Zero
$passed = [Collections.Generic.List[string]]::new()
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ShinyManagedUiTest {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,int m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h,uint command);
 [DllImport("user32.dll",EntryPoint="FindWindowW",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,IntPtr title);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
}
'@
function Managed-Text($h) { $s=[Text.StringBuilder]::new(8192); [void][ShinyManagedUiTest]::GetWindowText($h,$s,8192); return $s.ToString() }
function Managed-Command($h,$id) { if (-not [ShinyManagedUiTest]::PostMessage($h,0x111,[IntPtr]$id,[IntPtr]::Zero)) { throw "Cannot post action $id" } }
function Managed-Wait([scriptblock]$Condition,$Name,$Timeout=15000) { $end=[Environment]::TickCount64+$Timeout; do { if (& $Condition) {return}; Start-Sleep -Milliseconds 50 } while ([Environment]::TickCount64 -lt $end); throw "Managed UI condition timed out: $Name" }
function Managed-Cli([string[]]$Command) { $json = & $cli @Command; if ($LASTEXITCODE) { throw 'Installed manager command failed' }; return ($json | ConvertFrom-Json) }
function Managed-Status { return (Managed-Text ([ShinyManagedUiTest]::GetDlgItem($preview,106))) }
function Managed-Snapshot($name) {
 $r=[ShinyManagedUiTest+Rect]::new(); if (-not [ShinyManagedUiTest]::GetClientRect($preview,[ref]$r)) {throw 'No preview rectangle'}
 $b=[Drawing.Bitmap]::new($r.Right,$r.Bottom); $g=[Drawing.Graphics]::FromImage($b); $dc=$g.GetHdc()
 try { if (-not [ShinyManagedUiTest]::PrintWindow($preview,$dc,3)) {throw 'Preview screenshot failed'} } finally {$g.ReleaseHdc($dc);$g.Dispose()}
 try {$b.Save((Join-Path $Output $name),[Drawing.Imaging.ImageFormat]::Png)} finally {$b.Dispose()}
}
function Open-ManagedPreview {
 Managed-Command $main 141
 Managed-Wait { $script:preview=[ShinyManagedUiTest]::FindWindow('ShinyManagedPreview',[IntPtr]::Zero); $preview -ne [IntPtr]::Zero } 'preview window'
 if ([ShinyManagedUiTest]::GetWindow($preview,4) -ne $main) {throw 'Preview owner mismatch'}
 Managed-Command $preview 101
}
try {
 $ownedStore = $true
 $catalog = Managed-Cli @('--import-bundled')
 $first = ($catalog.packages | Where-Object version -eq '1.0.0').digest
 $second = ($catalog.packages | Where-Object version -eq '1.1.0').digest
 if (-not $first -or -not $second) {throw 'Two reference bundles were not imported'}
 $null = Managed-Cli @('--stage',$first)
 $null = Managed-Cli @('--activate',$first)
 $proc = Start-Process $Executable -ArgumentList @('--vlc-dir',('"'+$Runtime+'"'),('"'+$Fixture+'"')) -PassThru
 Managed-Wait { $proc.Refresh(); $script:main=$proc.MainWindowHandle; $main -ne [IntPtr]::Zero } 'primary window'
 Managed-Wait { (Managed-Text ([ShinyManagedUiTest]::GetDlgItem($main,304))) -match '^Playing with libVLC' } 'original playback'
 Open-ManagedPreview
 Managed-Wait { (Managed-Status) -match 'spatial reference version 1' -and (Managed-Status) -match 'Frames: [1-9]' } 'version one actual frame output'
 Managed-Snapshot 'player-managed-reference-v1.png'
 $passed.Add('real GUI starts the selected worker and displays matched decoded source/output')
 Managed-Command $preview 103
 Managed-Wait { (Managed-Text ([ShinyManagedUiTest]::GetDlgItem($preview,103))) -eq 'Resume preview' } 'pause'
 Managed-Command $preview 104
 Managed-Command $preview 103
 Managed-Wait { (Managed-Text ([ShinyManagedUiTest]::GetDlgItem($preview,103))) -eq 'Pause preview' } 'resume after reset'
 Managed-Wait { (Managed-Status) -match 'Frames: [1-9]' } 'resumed frames'
 $passed.Add('pause, reset and resume remain usable')
 $null = Managed-Cli @('--stage',$second)
 $null = Managed-Cli @('--activate',$second)
 Managed-Wait { (Managed-Status) -match 'spatial reference version 1' } 'running version remains leased'
 [void][ShinyManagedUiTest]::PostMessage($preview,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Managed-Wait { -not [ShinyManagedUiTest]::IsWindow($preview) } 'first preview closes'
 Open-ManagedPreview
 Managed-Wait { (Managed-Status) -match 'spatial reference version 2' -and (Managed-Status) -match 'Frames: [1-9]' } 'new preview uses version two'
 Managed-Snapshot 'player-managed-reference-v2.png'
 $passed.Add('selection leaves a running worker unchanged and a new preview loads the selected version')
 Managed-Command $preview 102
 Managed-Wait { (Managed-Status) -match '^Preview stopped' } 'preview stop'
 Managed-Command $preview 101
 Managed-Wait { (Managed-Status) -match 'spatial reference version 2' -and (Managed-Status) -match 'Frames: [1-9]' } 'preview restart'
 Managed-Command $main 111
 Managed-Wait { -not [ShinyManagedUiTest]::IsWindow($preview) } 'primary Stop closes enhancement worker'
 Managed-Command $main 110
 Managed-Wait { (Managed-Text ([ShinyManagedUiTest]::GetDlgItem($main,304))) -match '^Playing with libVLC' } 'primary replay'
 $passed.Add('preview stop/restart works and primary Stop tears down the preview without breaking replay')
 $null = Managed-Cli @('--original')
 Open-ManagedPreview
 Managed-Wait { (Managed-Status) -match '^No package selected' } 'original fallback without false active state'
 if (-not [ShinyManagedUiTest]::IsWindow($main)) {throw 'Original selection destroyed the primary player'}
 $passed.Add('original selection leaves primary playback available and reports no package instead of fake enhancement')
 @{ passed=$passed; versionsProcessed=2; source='actual libVLC-decoded synthetic fixture'; dlss=$false; physicalGpuValidated=$false; primaryAudioSynchronized=$false } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Output 'ui-managed-report.json')
} catch {
 $failure = $_
 @{ passed=$passed; previewStatus=(Managed-Status); error=$failure.Exception.Message } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Output 'ui-managed-failure.json')
 if ($preview -ne [IntPtr]::Zero -and [ShinyManagedUiTest]::IsWindow($preview)) { Managed-Snapshot 'ui-managed-failure.png' }
 throw $failure
} finally {
 if ($proc) {
  if ($main -ne [IntPtr]::Zero -and [ShinyManagedUiTest]::IsWindow($main)) { [void][ShinyManagedUiTest]::PostMessage($main,0x10,[IntPtr]::Zero,[IntPtr]::Zero) }
  if (-not $proc.WaitForExit(10000)) {$proc.Kill();$proc.WaitForExit();throw 'Managed UI primary failed to exit'}
 }
 if ($ownedStore -and (Test-Path $store)) {
  if ((Get-Content (Join-Path $store 'store.marker') -Raw) -ne "Shiny library store v1`n") {throw 'Test store identity mismatch; not removing it'}
  Remove-Item $store -Recurse -Force
 }
}
