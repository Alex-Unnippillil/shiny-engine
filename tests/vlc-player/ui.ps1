param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$Runtime,[Parameter(Mandatory=$true)][string]$Fixture,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class ShinyUiTest {
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,int msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,int msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll",EntryPoint="FindWindowW",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,IntPtr title);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h,uint command);
 private delegate bool EnumWindowsCallback(IntPtr h,IntPtr parameter);
 [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsCallback callback,IntPtr parameter);
 [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h,out uint processId);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassNameW(IntPtr h,StringBuilder name,int count);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr GetModuleHandleW(string name);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr CreateWindowExW(uint extendedStyle,string cls,string title,uint style,int x,int y,int width,int height,IntPtr owner,IntPtr menu,IntPtr instance,IntPtr parameter);
 [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr h);
 public static IntPtr FindOwnedWindow(string cls,IntPtr owner,uint processId) {
  IntPtr result=IntPtr.Zero;
  EnumWindows((h,p)=>{
   uint actual; GetWindowThreadProcessId(h,out actual);
   if(actual!=processId || GetWindow(h,4)!=owner || !IsWindowVisible(h)) return true;
   var name=new StringBuilder(256); GetClassNameW(h,name,name.Capacity);
   if(name.ToString()!=cls) return true;
   result=h; return false;
  },IntPtr.Zero);
  return result;
 }
 public static IntPtr CreateCompetingDialog() {
  // A visible topmost dialog in the TEST process must not mask the player's picker.
  // No executable/library input, no user interaction and no filesystem side effects.
  return CreateWindowExW(0x08000088,"#32770","Shiny test-only competing dialog",0x90000000,
   0,0,32,32,IntPtr.Zero,IntPtr.Zero,GetModuleHandleW(null),IntPtr.Zero);
 }
 public static string[] DialogInventory(uint processId) {
  var result=new List<string>();
  EnumWindows((h,p)=>{
   uint actual; GetWindowThreadProcessId(h,out actual);
   var cls=new StringBuilder(256); GetClassNameW(h,cls,cls.Capacity);
   if(actual==processId || cls.ToString()=="#32770")
    result.Add("class="+cls+"; playerProcess="+(actual==processId)+"; visible="+IsWindowVisible(h)+"; enabled="+IsWindowEnabled(h)+"; owner="+GetWindow(h,4).ToInt64());
   return true;
  },IntPtr.Zero);
  return result.ToArray();
 }

 [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr h,int index);
 [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int height,bool repaint);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h,ref Point p);
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
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
$panel=[IntPtr]::Zero
$decoy=[IntPtr]::Zero
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
 Wait-For { $script:panel=[ShinyUiTest]::FindOwnedWindow('ShinyNativeNeural',$main,[uint32]$proc.Id); $panel -ne [IntPtr]::Zero } 'native neural panel'
 if ([ShinyUiTest]::GetWindow($panel,4) -ne $main) { throw 'Unexpected workbench owner' }
 if ([ShinyUiTest]::IsWindowEnabled([ShinyUiTest]::GetDlgItem($panel,103))) { throw 'Unreviewed native inference was enabled' }
 Start-Sleep -Milliseconds 1500
 Snapshot $panel 'player-neural-workbench.png'
 $passed.Add('native neural panel opens with original preview and disabled preparation until valid intake')
 [void][ShinyUiTest]::SendMessage([ShinyUiTest]::GetDlgItem($panel,109),0xF1,[IntPtr]1,[IntPtr]::Zero)
 Command $panel 109
 Command $panel 110
 Start-Sleep -Milliseconds 200
 if ([ShinyUiTest]::IsWindowEnabled([ShinyUiTest]::GetDlgItem($panel,103))) { throw 'Research checkbox enabled preparation without any inspected model' }
 if ([ShinyUiTest]::IsWindowEnabled([ShinyUiTest]::GetDlgItem($panel,113))) { throw 'Output export available before any inference result' }
 Snapshot $panel 'player-research-mode.png'
 $passed.Add('research selection alone does not bypass intake or fabricate an exportable result')
 # Open the actual modal folder picker asynchronously. The primary owner must
 # remain disabled until cancellation, so its auto-next timer cannot destroy
 # the research panel on a nested message loop.
 $decoy=[ShinyUiTest]::CreateCompetingDialog()
 if ($decoy -eq [IntPtr]::Zero) { throw 'Could not create the test-only competing dialog' }
 if ([ShinyUiTest]::FindOwnedWindow('#32770',[IntPtr]::Zero,[uint32]$PID) -ne $decoy) { throw 'Competing-dialog fixture is not discoverable in the test process' }
 if (-not [ShinyUiTest]::PostMessage($panel,0x111,[IntPtr]102,[IntPtr]::Zero)) { throw 'Could not request the model folder picker' }
 $picker=[IntPtr]::Zero
 Wait-For { $script:picker=[ShinyUiTest]::FindOwnedWindow('#32770',$panel,[uint32]$proc.Id); $picker -ne [IntPtr]::Zero } 'model folder picker'
 if ($picker -eq $decoy) { throw 'A foreign dialog was mistaken for the model picker' }
 if ([ShinyUiTest]::IsWindowEnabled($main)) { throw 'Primary owner remained enabled during the research picker' }
 [void][ShinyUiTest]::PostMessage($picker,0x111,[IntPtr]2,[IntPtr]::Zero)
 Wait-For { -not [ShinyUiTest]::IsWindow($picker) -and [ShinyUiTest]::IsWindowEnabled($main) -and [ShinyUiTest]::IsWindowEnabled($panel) } 'picker cancellation and owner recovery'
 if ([ShinyUiTest]::IsWindowEnabled([ShinyUiTest]::GetDlgItem($panel,103))) { throw 'Cancelled picker granted model authorization' }
 if (-not [ShinyUiTest]::IsWindow($decoy)) { throw 'Picker cancellation closed the unrelated test dialog' }
 if (-not [ShinyUiTest]::DestroyWindow($decoy)) { throw 'Could not dispose the competing-dialog fixture' }
 $decoy=[IntPtr]::Zero
 $passed.Add('process-and-owner-scoped lookup ignores a competing dialog; cancellation affects only the actual model picker')
 $passed.Add('real folder-picker cancellation restores owner controls without authorizing a model')
 Command $panel 2
 Wait-For { -not [ShinyUiTest]::IsWindow($panel) } 'panel closes'
 if (-not [ShinyUiTest]::IsWindow($main)) { throw 'Workbench closure destroyed primary playback' }
 $passed.Add('dialog cancellation disposes secondary session without closing primary player')
 # Exercise the actual responsive Win32 layout, not just its portable geometry.
 $before=[ShinyUiTest+Rect]::new()
 if (-not [ShinyUiTest]::GetWindowRect($main,[ref]$before)) { throw 'Cannot read player rectangle' }
 if (-not [ShinyUiTest]::MoveWindow($main,$before.Left,$before.Top,740,640,$true)) { throw 'Cannot resize player' }
 Wait-For { -not [ShinyUiTest]::IsWindowVisible([ShinyUiTest]::GetDlgItem($main,204)) } 'compact layout sidebar hides'
 $client=[ShinyUiTest+Rect]::new();$origin=[ShinyUiTest+Point]::new()
 [void][ShinyUiTest]::GetClientRect($main,[ref]$client)
 [void][ShinyUiTest]::ClientToScreen($main,[ref]$origin)
 foreach ($id in @(101,137,110,111,201,202,203)) {
  $control=[ShinyUiTest]::GetDlgItem($main,$id)
  if (-not [ShinyUiTest]::IsWindowVisible($control)) { throw "Compact layout hid essential control $id" }
  $rect=[ShinyUiTest+Rect]::new();[void][ShinyUiTest]::GetWindowRect($control,[ref]$rect)
  # Combo box's closed window rectangle is the visible transport control.
  if ($rect.Left -lt $origin.X -or $rect.Top -lt $origin.Y -or $rect.Right -gt ($origin.X+$client.Right) -or $rect.Bottom -gt ($origin.Y+$client.Bottom)) { throw "Compact control $id extends outside the client area" }
 }
 Snapshot $main 'player-compact.png'
 [void][ShinyUiTest]::MoveWindow($main,$before.Left,$before.Top,($before.Right-$before.Left),($before.Bottom-$before.Top),$true)
 Wait-For { [ShinyUiTest]::IsWindowVisible([ShinyUiTest]::GetDlgItem($main,204)) } 'expanded layout restores queue'
 $passed.Add('compact native window keeps transport, seek, volume and research controls inside the viewport and restores the queue')
 Command $main 111
 Command $main 110
 Wait-For { (Text ([ShinyUiTest]::GetDlgItem($main,304))) -match '^Playing with libVLC' } 'reopen primary playback'
 $passed.Add('stop and replay remain usable after workbench teardown')
 @{passed=$passed;trainedModelInference=$false;physicalGpuValidated=$false;scope='Actual Windows UI and folder dialog with original synthetic media. No trained model, neural export or sound-device certification.'} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Output 'ui-workbench-report.json')
} catch {
 $failure = $_
 if ($main -ne [IntPtr]::Zero -and [ShinyUiTest]::IsWindow($main)) {
  @{status=(Text ([ShinyUiTest]::GetDlgItem($main,304)));passed=$passed;error=$failure.Exception.Message;panelExists=[ShinyUiTest]::IsWindow($panel);panelEnabled=[ShinyUiTest]::IsWindowEnabled($panel);mainEnabled=[ShinyUiTest]::IsWindowEnabled($main);windows=[ShinyUiTest]::DialogInventory([uint32]$proc.Id)} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Output 'ui-failure.json')
  Snapshot $main 'ui-failure-main.png'
 }
 throw $failure
} finally {
 if ($decoy -ne [IntPtr]::Zero -and [ShinyUiTest]::IsWindow($decoy)) { [void][ShinyUiTest]::DestroyWindow($decoy) }
 if ($main -ne [IntPtr]::Zero -and [ShinyUiTest]::IsWindow($main)) { [void][ShinyUiTest]::PostMessage($main,0x10,[IntPtr]::Zero,[IntPtr]::Zero) }
 if (-not $proc.WaitForExit(10000)) { $proc.Kill();throw 'Player failed to close after UI checks' }
}
