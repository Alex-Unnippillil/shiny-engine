# Capture the real, released Windows application. Do not paint media over its UI.
param([Parameter(Mandatory=$true)][string]$Work,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $Work,$Output | Out-Null
$Work = (Resolve-Path $Work).Path
$Output = (Resolve-Path $Output).Path
$release = 'https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.10.0'
$portable = Join-Path $Work 'portable.zip'
Invoke-WebRequest "$release/ShinyPlayer-0.10.0-Windows-x64-Portable.zip" -OutFile $portable -TimeoutSec 120
if ((Get-FileHash $portable -Algorithm SHA256).Hash.ToLower() -ne '3d164076c8b8cc1b442e1dae5e4e2abf95ef48c6417ccc319fdb9d3a6b24bb3f') { throw 'Released player package changed' }
Expand-Archive $portable (Join-Path $Work 'player')
$vlczip = Join-Path $Work 'vlc.zip'
Invoke-WebRequest 'https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.zip' -OutFile $vlczip -TimeoutSec 180
if ((Get-FileHash $vlczip -Algorithm SHA256).Hash.ToLower() -ne 'fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323') { throw 'VLC runtime hash mismatch' }
Expand-Archive $vlczip (Join-Path $Work 'runtime')
$sourceUrl = 'https://assets.science.nasa.gov/dynamicimage/assets/science/missions/webb/science/2022/07/STScI-01GA6KKWG229B16K4Q38CH3BXS.png?crop=faces%2Cfocalpoint&fit=clip&h=1158&w=2000'
$source = Join-Path $Work 'cosmic-cliffs.png'
Invoke-WebRequest $sourceUrl -OutFile $source -TimeoutSec 180
if ((Get-Item $source).Length -gt 16MB) { throw 'Unexpected source image size' }
$image = [Drawing.Image]::FromFile($source)
if ($image.Width -ne 2000 -or $image.Height -ne 1158) { $image.Dispose();throw 'Unexpected source dimensions' }
$frame = [Drawing.Bitmap]::new(1280,742)
$graphics = [Drawing.Graphics]::FromImage($frame)
try {
 $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
 $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
 $graphics.DrawImage($image,0,0,1280,742)
 $encoder = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
 $quality = [Drawing.Imaging.EncoderParameters]::new(1)
 $quality.Param[0] = [Drawing.Imaging.EncoderParameter]::new([Drawing.Imaging.Encoder]::Quality,[long]96)
 try { $frame.Save((Join-Path $Work 'frame.jpg'),$encoder,$quality) } finally { $quality.Dispose() }
} finally { $graphics.Dispose();$frame.Dispose();$image.Dispose() }
$clip = Join-Path $Work 'Cosmic Cliffs - Webb.avi'
python "$PSScriptRoot/make-still-clip.py" (Join-Path $Work 'frame.jpg') $clip
if ($LASTEXITCODE) { throw 'Still-clip generation failed' }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ReadmeCapture {
 [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int height,bool repaint);
}
'@
$exe = Join-Path $Work 'player/ShinyVlcPlayer.exe'
$runtime = Join-Path $Work 'runtime/vlc-3.0.24'
$screenshot = Join-Path $Output 'readme-cosmic-cliffs.png'
$args = @('--vlc-dir',('"'+$runtime+'"'),'--ui-smoke',('"'+$clip+'"'),('"'+$screenshot+'"'))
$proc = Start-Process $exe -ArgumentList $args -PassThru
try {
 $until = [Environment]::TickCount64+4000
 do { $proc.Refresh();if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { break };Start-Sleep -Milliseconds 50 } while ([Environment]::TickCount64 -lt $until)
 if ($proc.HasExited -or $proc.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Player window unavailable' }
 if (-not [ReadmeCapture]::MoveWindow($proc.MainWindowHandle,0,0,1440,900,$true)) { throw 'Cannot size documentation capture' }
 if (-not $proc.WaitForExit(30000)) { throw 'Player capture timed out' }
 if ($proc.ExitCode -ne 0) { throw 'Native capture failed' }
} finally { if (-not $proc.HasExited) { $proc.Kill();$proc.WaitForExit() };$proc.Dispose() }
$diagnostics = Get-Content (Join-Path $Output 'ui-playback.json') -Raw | ConvertFrom-Json
if ($diagnostics.app -ne '0.10.0' -or $diagnostics.decodedVideoFrames -lt 1 -or $diagnostics.width -ne 1280 -or $diagnostics.height -ne 742 -or $diagnostics.dlss5Inference -ne $false) { throw 'Native decoding evidence missing' }
$shot = [Drawing.Image]::FromFile($screenshot)
# The runner's desktop constrains the requested outer size. Record actual client pixels.
try {
 $shotWidth=$shot.Width; $shotHeight=$shot.Height
 if ($shotWidth -lt 1000 -or $shotHeight -lt 680) { throw 'Screenshot too small to show the readable workspace' }
} finally { $shot.Dispose() }
@{
 schema=1; purpose='README presentation, not enhancement evidence'; appVersion='0.10.0'
 playerSourceSha='1957df8453bc26d341a1e47976d1e034f8f46d9c'
 playerPackageSha256=(Get-FileHash $portable -Algorithm SHA256).Hash.ToLower()
 sourcePage='https://science.nasa.gov/asset/webb/cosmic-cliffs-in-the-carina-nebula-nircam-image/'
 sourceUrl=$sourceUrl; sourceSha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLower()
 credit='NASA, ESA, CSA, STScI'; media='10-second silent still-image clip; bicubic resize to 1280x742 and JPEG quality 96'
 capture='Unmodified player --ui-smoke / PrintWindow client-area capture; no post-capture editing or compositing'
 screenshotWidth=$shotWidth; screenshotHeight=$shotHeight
 screenshotSha256=(Get-FileHash $screenshot -Algorithm SHA256).Hash.ToLower()
 captureRun=$env:GITHUB_RUN_ID; diagnostics=$diagnostics
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Output 'readme-cosmic-cliffs.json') -Encoding utf8
Remove-Item (Join-Path $Output 'ui-playback.json')
