param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$Runtime,[Parameter(Mandatory=$true)][string]$Fixture,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ShinyUiTest {
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,int msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h,uint command);
 [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr h,int index);
 [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect r);
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
}
'@
function Text($h) { $s=[Text.StringBuilder]::new(4096); [void][ShinyUiTest]::GetWindowText($h,$s,4096); return $s.ToString() }
function Command($h,$id) { [void][ShinyUiTest]::SendMessage($h,0x111,[IntPtr]$id,[IntPtr]::Zero) }
function Wait-For([scriptblock]$Condition,$Name,$Timeout=15000) { $end=[Environment]::TickCount64+$Timeout; do { if (& $Condition) { return }; Start-Sleep -Milliseconds 50 } while ([Environment]::TickCount64 -lt $end); throw "UI condition timed out: $Name" }
function Snapshot($h,$name) { $rect=[ShinyUiTest+Rect]::new(); if (-not [ShinyUiTest]::GetClientRect($h,[ref]$rect)) {throw 'No window rectangle'}; $b=[Drawing.Bitmap]::new($rect.Right,$rect.Bottom); $g=[Drawing.Graphics]::FromImage($b); $dc=$g.GetHdc(); try { if (-not [ShinyUiTest]::PrintWindow($h,$dc,3)) { throw 'Window capture failed' } } finally { $g.ReleaseHdc($dc);$g.Dispose() }; try { $b.Save((Join-Path $Output $name),[Drawing.Imaging.ImageFormat]::Png) } finally { $b.Dispose() } }
$proc = Start-Process $Executable -ArgumentList @('--vlc-dir',('"'+$Runtime+'"'),('"'+$Fixture+'"')) -PassThru
$passed = [Collections.Generic.List[string]]::new()
$main=[IntPtr]::Zero
try {
 Wait-For { $proc.Refresh(); $script:main=$proc.MainWindowHandle; $main -ne [IntPtr]::Zero } 'main window'
 Wait-For { (Text ([ShinyUiTest]::GetDlgItem($main,304))) -match '^Playing with libVLC' } 'decoded primary playback'
 Command $main 132
 if ([ShinyUiTest]::IsWindowVisible([ShinyUiTest]::GetDlgItem($main,204))) { throw 'Cinema view retained sidebar' }
 Snapshot $main 'player-cinema.png'
 Command $main 132
 if (-not [ShinyUiTest]::IsWindowVisible([ShinyUiTest]::GetDlgItem($main,204))) { throw 'Workspace did not return' }
 $passed.Add('cinema hides the sidebar and restores the workspace')
 Command $main 135
 $playback=[ShinyUiTest]::GetSubMenu([ShinyUiTest]::GetMenu($main),1)
 $bookmarks=[ShinyUiTest]::GetSubMenu($playback,2)
 [void][ShinyUiTest]::SendMessage($main,0x117,$bookmarks,[IntPtr]::Zero)
 if ([ShinyUiTest]::GetMenuItemCount($bookmarks) -ne 1) { throw 'Source bookmark menu did not populate' }
 $passed.Add('real playback action creates a source bookmark')
 Command $main 137
 $panel=[IntPtr]::Zero
 Wait-For { $script:panel=[ShinyUiTest]::FindWindow('ShinyNativeNeural',$null); $panel -ne [IntPtr]::Zero } 'native neural panel'
 if ([ShinyUiTest]::GetWindow($panel,4) -ne $main) { throw 'Unexpected workbench owner' }
 if ([ShinyUiTest]::IsWindowEnabled([ShinyUiTest]::GetDlgItem($panel,103))) { throw 'Unreviewed native inference was enabled' }
 Start-Sleep -Milliseconds 1500
 Snapshot $panel 'player-neural-workbench.png'
 $passed.Add('native neural panel opens with original preview and disabled model preparation')
 [void][ShinyUiTest]::SendMessage($panel,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-For { -not [ShinyUiTest]::IsWindow($panel) } 'panel closes'
 if (-not [ShinyUiTest]::IsWindow($main)) { throw 'Workbench closure destroyed primary playback' }
 $passed.Add('workbench closure disposes secondary session without closing primary player')
 Command $main 111
 Command $main 110
 Wait-For { (Text ([ShinyUiTest]::GetDlgItem($main,304))) -match '^Playing with libVLC' } 'reopen primary playback'
 $passed.Add('stop and replay remain usable after workbench teardown')
 @{passed=$passed;trainedModelInference=$false;physicalGpuValidated=$false;scope='Actual Windows user-interface actions with original synthetic media. No trained model or sound-device certification.'} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Output 'ui-workbench-report.json')
} finally {
 if ($main -ne [IntPtr]::Zero -and [ShinyUiTest]::IsWindow($main)) { [void][ShinyUiTest]::SendMessage($main,0x10,[IntPtr]::Zero,[IntPtr]::Zero) }
 if (-not $proc.WaitForExit(10000)) { $proc.Kill();throw 'Player failed to close after UI checks' }
}
