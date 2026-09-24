param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$Runtime,[Parameter(Mandatory=$true)][string]$Fixture,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class WorkspaceUi {
 public delegate bool EnumProc(IntPtr h,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb,IntPtr p);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h,uint cmd);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,int msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,int msg,IntPtr w,IntPtr l);
 [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)] static extern IntPtr SendText(IntPtr h,int msg,IntPtr w,string text);
 [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)] static extern IntPtr ReadText(IntPtr h,int msg,IntPtr w,StringBuilder text);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h,ref Point p);
 [StructLayout(LayoutKind.Sequential)] public struct Point {public int X,Y;}
 [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int height,bool repaint);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint id,ref ThreadInfo info);
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)] struct ThreadInfo {public int size,flags;public IntPtr active,focus,capture,menu,move,caret;public Rect rect;}
 public static IntPtr Focus(IntPtr h){uint pid;uint thread=GetWindowThreadProcessId(h,out pid);ThreadInfo info=new ThreadInfo();info.size=Marshal.SizeOf(info);return GetGUIThreadInfo(thread,ref info)?info.focus:IntPtr.Zero;}
 public static string Text(IntPtr h){var s=new StringBuilder(2048);ReadText(h,0xD,(IntPtr)2048,s);return s.ToString();}
 // System WM_SETTEXT/WM_GETTEXT messages marshal text across the test/app process boundary.
 public static void SetText(IntPtr h,string text){if(h==IntPtr.Zero||SendText(h,0xC,IntPtr.Zero,text)==IntPtr.Zero||Text(h)!=text)throw new InvalidOperationException("Control text injection failed");}
 public static IntPtr Find(string cls,uint pid,IntPtr owner){IntPtr found=IntPtr.Zero;EnumWindows((h,p)=>{uint actual;GetWindowThreadProcessId(h,out actual);var s=new StringBuilder(80);GetClassNameW(h,s,80);if(actual==pid&&s.ToString()==cls&&GetWindow(h,4)==owner&&IsWindowVisible(h)){found=h;return false;}return true;},IntPtr.Zero);return found;}
}
'@
function Wait-For([scriptblock]$Condition,[string]$Name){$until=[Environment]::TickCount64+15000;do {if(&$Condition){return};Start-Sleep -Milliseconds 50}while([Environment]::TickCount64 -lt $until);throw "Workspace condition timed out: $Name"}
function Command($h,$id){[void][WorkspaceUi]::SendMessage($h,0x111,[IntPtr]$id,[IntPtr]::Zero)}
function Snapshot($h,$name){$r=[WorkspaceUi+Rect]::new();if( -not [WorkspaceUi]::GetClientRect($h,[ref]$r)){throw 'No client rectangle'};$b=[Drawing.Bitmap]::new($r.Right,$r.Bottom);$g=[Drawing.Graphics]::FromImage($b);$dc=$g.GetHdc();try{if( -not [WorkspaceUi]::PrintWindow($h,$dc,3)){throw 'Capture failed'}}finally{$g.ReleaseHdc($dc);$g.Dispose()};try{$b.Save((Join-Path $Output $name),[Drawing.Imaging.ImageFormat]::Png)}finally{$b.Dispose()}}
$passed=[Collections.Generic.List[string]]::new();$main=[IntPtr]::Zero;$proc=$null
$second=Join-Path (Split-Path $Fixture) 'second-review.avi';Copy-Item $Fixture $second
try{
 $proc=Start-Process $Executable -ArgumentList @('--vlc-dir',('"'+$Runtime+'"')) -PassThru
 Wait-For {$script:main=[WorkspaceUi]::Find('ShinyVlcPlayer',$proc.Id,[IntPtr]::Zero);$main-ne[IntPtr]::Zero} 'idle window'
 Wait-For {[WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,304)) -match  '^Ready'} 'runtime ready'
 if([WorkspaceUi]::IsWindowEnabled([WorkspaceUi]::GetDlgItem($main,110))){throw 'Empty workspace enabled play'}
 if( -not [WorkspaceUi]::IsWindowVisible([WorkspaceUi]::GetDlgItem($main,225))){throw 'Welcome action missing'}
 Snapshot $main 'player-welcome.png';$passed.Add('welcome screen exposes local onboarding and disables media-dependent playback')
 [void][WorkspaceUi]::PostMessage($main,0x111,[IntPtr]142,[IntPtr]::Zero);$palette=[IntPtr]::Zero
 Wait-For {$script:palette=[WorkspaceUi]::Find('ShinyQuickActions',$proc.Id,$main);$palette-ne[IntPtr]::Zero} 'quick actions'
 if([WorkspaceUi]::IsWindowEnabled($main)){throw 'Quick actions did not own its modal interaction'}
 [void][WorkspaceUi]::SetText([WorkspaceUi]::GetDlgItem($palette,10),'play or pause')
 Wait-For {[WorkspaceUi]::SendMessage([WorkspaceUi]::GetDlgItem($palette,11),0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -eq 1} 'filtered command'
 if([WorkspaceUi]::IsWindowEnabled([WorkspaceUi]::GetDlgItem($palette,1))){throw 'Unavailable command enabled'}
 [void][WorkspaceUi]::PostMessage([WorkspaceUi]::GetDlgItem($palette,10),0x100,[IntPtr]13,[IntPtr]::Zero)
 Start-Sleep -Milliseconds 100
 if( -not [WorkspaceUi]::IsWindow($palette)){throw 'Disabled action ran'}
 # Deliberately shrink the modal to exercise constrained work-area layout.
 [void][WorkspaceUi]::MoveWindow($palette,40,40,560,440,$true)
 $client=[WorkspaceUi+Rect]::new();$origin=[WorkspaceUi+Point]::new()
 [void][WorkspaceUi]::GetClientRect($palette,[ref]$client);[void][WorkspaceUi]::ClientToScreen($palette,[ref]$origin)
 foreach($id in @(10,11,12,1,2)){
  $r=[WorkspaceUi+Rect]::new();$c=[WorkspaceUi]::GetDlgItem($palette,$id);[void][WorkspaceUi]::GetWindowRect($c,[ref]$r)
  if($r.Left -lt $origin.X -or $r.Top -lt $origin.Y -or $r.Right -gt ($origin.X+$client.Right) -or $r.Bottom -gt ($origin.Y+$client.Bottom)){throw 'Constrained Quick actions clipped an interactive control'}
 }
 [void][WorkspaceUi]::PostMessage([WorkspaceUi]::GetDlgItem($palette,10),0x100,[IntPtr]27,[IntPtr]::Zero)
 Wait-For { -not [WorkspaceUi]::IsWindow($palette) -and [WorkspaceUi]::IsWindowEnabled($main)} 'palette cancellation'
 $passed.Add('quick actions filter is bounded; unavailable commands do not run; compact modal keeps controls visible; Escape restores the owner')
 [void][WorkspaceUi]::PostMessage($main,0x10,[IntPtr]::Zero,[IntPtr]::Zero);if( -not $proc.WaitForExit(10000)){throw 'Idle player did not close'}
 $proc=Start-Process $Executable -ArgumentList @('--vlc-dir',('"'+$Runtime+'"'),('"'+$Fixture+'"'),('"'+$second+'"')) -PassThru
 Wait-For {$script:main=[WorkspaceUi]::Find('ShinyVlcPlayer',$proc.Id,[IntPtr]::Zero);$main-ne[IntPtr]::Zero} 'playback window'
 Wait-For {[WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,304)) -match  '^Playing with libVLC'} 'decoded playback'
 $list=[WorkspaceUi]::GetDlgItem($main,204);$filter=[WorkspaceUi]::GetDlgItem($main,220)
 if([WorkspaceUi]::SendMessage($list,0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -ne 2){throw 'Two inputs not queued'}
 Snapshot $main 'player-workspace.png'
 [void][WorkspaceUi]::SetText($filter,'second')
 Wait-For {[WorkspaceUi]::SendMessage($list,0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -eq 1} 'queue filtering'
 [void][WorkspaceUi]::SendMessage($list,0x186,[IntPtr]::Zero,[IntPtr]::Zero)
 [void][WorkspaceUi]::SendMessage($main,0x111,[IntPtr](204 -bor (2 -shl 16)),$list)
 Wait-For {[WorkspaceUi]::Text($main) -match  'second-review.avi'} 'filtered row maps to second source'
 Wait-For {[WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,222)) -eq  'PLAYING'} 'second playback'
 $passed.Add('filtered queue activation resolves the real source index, not the visible row number')
 [void][WorkspaceUi]::SetText($filter,'no matching title')
 if([WorkspaceUi]::IsWindowEnabled([WorkspaceUi]::GetDlgItem($main,127))){throw 'Empty filter enabled removal'}
 if([WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,221)) -notmatch  '^No matching'){throw 'Empty search guidance missing'}
 [void][WorkspaceUi]::SetText($filter,'moving original')
 if([WorkspaceUi]::IsWindowEnabled([WorkspaceUi]::GetDlgItem($main,138))){throw 'Filtered reorder enabled'}
 [void][WorkspaceUi]::PostMessage($list,0x100,[IntPtr]83,[IntPtr]::Zero);Start-Sleep -Milliseconds 100
 if([WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,222)) -ne 'PLAYING'){throw 'List typing stopped playback'}
 [void][WorkspaceUi]::PostMessage($list,0x100,[IntPtr]46,[IntPtr]::Zero)
 Wait-For {[WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,221)) -match  '^No matching'} 'filtered delete'
 [void][WorkspaceUi]::SetText($filter,'')
 if([WorkspaceUi]::SendMessage($list,0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -ne 1 -or [WorkspaceUi]::Text($main) -notmatch  'second-review.avi'){throw 'Filtered deletion removed current source'}
 $passed.Add('empty search, filtered removal and native list typing preserve current playback')
 Command $main 144
 if([WorkspaceUi]::IsWindowVisible($list) -or -not[WorkspaceUi]::IsWindowVisible([WorkspaceUi]::GetDlgItem($main,205))){throw 'Adjustment view failed'}
 Snapshot $main 'player-adjustments.png';Command $main 143
 if( -not [WorkspaceUi]::IsWindowVisible($list)){throw 'Queue view did not return'}
 $passed.Add('Queue and Adjustments views preserve playback and expose distinct controls')
 [void][WorkspaceUi]::PostMessage($main,0x111,[IntPtr]142,[IntPtr]::Zero)
 Wait-For {$script:palette=[WorkspaceUi]::Find('ShinyQuickActions',$proc.Id,$main);$palette-ne[IntPtr]::Zero} 'loaded quick actions'
 Snapshot $palette 'player-quick-actions.png'
 [void][WorkspaceUi]::SetText([WorkspaceUi]::GetDlgItem($palette,10),'toggle cinema')
 [void][WorkspaceUi]::PostMessage([WorkspaceUi]::GetDlgItem($palette,10),0x100,[IntPtr]13,[IntPtr]::Zero)
 Wait-For { -not [WorkspaceUi]::IsWindow($palette) -and -not[WorkspaceUi]::IsWindowVisible($list)} 'palette executes selected command'
 Command $main 143
 $passed.Add('Quick actions executes an existing command only after explicit Enter')
 [void][WorkspaceUi]::MoveWindow($main,40,40,740,640,$true)
 Wait-For { -not [WorkspaceUi]::IsWindowVisible($filter)} 'compact sidebar hides'
 if([WorkspaceUi]::Focus($main) -ne [WorkspaceUi]::GetDlgItem($main,142)){throw 'Focus stranded on a hidden sidebar'}
 Snapshot $main 'player-workspace-compact.png'
 $passed.Add('compact resize moves keyboard focus away from hidden controls')
 Command $main 126
 Wait-For {[WorkspaceUi]::IsWindowVisible([WorkspaceUi]::GetDlgItem($main,225))} 'clear returns welcome'
 if([WorkspaceUi]::IsWindowEnabled([WorkspaceUi]::GetDlgItem($main,110))){throw 'Cleared player retained play availability'}
 $passed.Add('Clear stops playback, discards the in-memory queue and returns to onboarding')
 @{schema=1;passed=$passed;scope='Actual Windows application interactions and synthetic decoding. No trained-model, sound-device or physical-GPU certification.'} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Output 'ui-workspace-report.json')
} catch {
 if($main-ne[IntPtr]::Zero-and[WorkspaceUi]::IsWindow($main)){
  $p=[WorkspaceUi]::Find('ShinyQuickActions',$proc.Id,$main);if($p -ne [IntPtr]::Zero){Snapshot $p 'workspace-failure-palette.png'}
  Snapshot $main 'workspace-failure.png';@{passed=$passed;error=$_.Exception.Message;status=[WorkspaceUi]::Text([WorkspaceUi]::GetDlgItem($main,304))}|ConvertTo-Json -Depth 4|Set-Content(Join-Path $Output 'workspace-failure.json')}
 throw
} finally {
 if($proc-and-not$proc.HasExited){if($main-ne[IntPtr]::Zero){[void][WorkspaceUi]::PostMessage($main,0x10,[IntPtr]::Zero,[IntPtr]::Zero)};if( -not $proc.WaitForExit(10000)){$proc.Kill();throw 'Workspace player failed to close'}}
 Remove-Item $second -ErrorAction SilentlyContinue
}
