$ErrorActionPreference = 'Stop'
$installer = Join-Path $PWD 'artifacts/vlc/ShinyPlayer-0.6.0-Windows-x64-Setup.exe'
$target = Join-Path $env:RUNNER_TEMP 'ShinyPlayer-Installer-Test'
$proof = Join-Path $PWD 'artifacts/vlc'
function Run-Checked($exe, $arguments, $timeout = 90000) {
  $p = Start-Process $exe -ArgumentList $arguments -PassThru
  if (-not $p.WaitForExit($timeout)) { $p.Kill(); throw "Process timed out: $([IO.Path]::GetFileName($exe))" }
  if ($p.ExitCode -ne 0) { throw "Process failed ($($p.ExitCode)): $([IO.Path]::GetFileName($exe))" }
}
if (Test-Path $target) { throw 'Installer test target must be fresh' }
# Exercise verified cached prerequisite handling without redistributing VLC.
$archive = Join-Path $PWD '.deps/vlc.zip'
Run-Checked $installer @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/DIR="'+$target+'"'),'/TASKS=vlcruntime',('/VLCARCHIVE="'+$archive+'"'),('/LOG="'+$proof+'\installer-test.log"'))
foreach ($path in @('ShinyVlcPlayer.exe','nr\ShinyNrWorker.exe','runtime\vlc-3.0.24\libvlc.dll','unins000.exe')) { if (-not (Test-Path (Join-Path $target $path))) { throw "Installer omitted $path" } }
$fixture = Join-Path $PWD '.deps/fixtures/moving-original.avi'
Run-Checked (Join-Path $target 'ShinyVlcPlayer.exe') @('--ui-smoke',('"'+$fixture+'"'),('"'+$proof+'\installed-player.png"')) 30000
$ui = Get-Content (Join-Path $proof 'ui-playback.json') -Raw | ConvertFrom-Json
if ($ui.decodedVideoFrames -lt 1) { throw 'Installed player did not decode video' }
Run-Checked (Join-Path $target 'unins000.exe') @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART')
Start-Sleep -Seconds 2
foreach ($path in @('ShinyVlcPlayer.exe','nr\ShinyNrWorker.exe','runtime\vlc-3.0.24\libvlc.dll')) { if (Test-Path (Join-Path $target $path)) { throw "Uninstaller retained managed file $path" } }
@{ installed=$true; verifiedCachedVlc=$true; runtimeAutoDetected=$true; decodedVideoFrames=$ui.decodedVideoFrames; uninstalled=$true; trainedModelInference=$false; signed=$false; scope='Disposable Windows CI user. No physical GPU, neural model, online prerequisite download or sound device validation.' } | ConvertTo-Json | Set-Content (Join-Path $proof 'installer-report.json')
