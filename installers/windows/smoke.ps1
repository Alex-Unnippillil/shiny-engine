$ErrorActionPreference = 'Stop'
$installer = Join-Path $PWD 'artifacts/vlc/ShinyPlayer-0.9.0-Windows-x64-Setup.exe'
$temporaryRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
$target = Join-Path $temporaryRoot ('ShinyPlayer-Installer-Test-' + [guid]::NewGuid().ToString('N'))
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
foreach ($path in @('ShinyVlcPlayer.exe','ShinyLibraryManager.exe','ShinyLibraryManagerCli.exe','ShinyEnhancementWorker.exe','library-bundles\1.0.0\shiny_spatial.dll','library-bundles\1.1.0\shiny_spatial.dll','library-bundles\1.2.0\shiny_spatial.dll','nr\ShinyNrWorker.exe','runtime\vlc-3.0.24\libvlc.dll','unins000.exe')) { if (-not (Test-Path (Join-Path $target $path))) { throw "Installer omitted $path" } }
$fixture = Join-Path $PWD '.deps/fixtures/moving-original.avi'
Run-Checked (Join-Path $target 'ShinyVlcPlayer.exe') @('--ui-smoke',('"'+$fixture+'"'),('"'+$proof+'\installed-player.png"')) 30000
$ui = Get-Content (Join-Path $proof 'ui-playback.json') -Raw | ConvertFrom-Json
if ($ui.app -ne '0.9.0' -or $ui.decodedVideoFrames -lt 1) { throw 'Installed player did not decode video' }
$store = Join-Path $temporaryRoot ('ShinyManager-Installed-Test-' + [guid]::NewGuid().ToString('N'))
try {
  $cli = Join-Path $target 'ShinyLibraryManagerCli.exe'
  $catalog = & $cli --store-dir $store --import-bundled | ConvertFrom-Json
  if ($LASTEXITCODE -ne 0 -or $catalog.packages.Count -ne 3 -or (($catalog.packages.version | Sort-Object) -join ',') -ne '1.0.0,1.1.0,1.2.0') { throw 'Installed manager did not import its exact bundled versions' }
  foreach ($entry in $catalog.packages) {
    & $cli --store-dir $store --stage $entry.digest | Out-Null
    if ($LASTEXITCODE) { throw 'Installed manager staging failed' }
    & $cli --store-dir $store --activate $entry.digest | Out-Null
    if ($LASTEXITCODE) { throw 'Installed worker/DLL handshake or known-frame check failed' }
  }
  & $cli --store-dir $store --rollback | Out-Null
  if ($LASTEXITCODE) { throw 'Installed manager rollback failed' }
} finally { if (Test-Path $store) { Remove-Item $store -Recurse -Force } }
Run-Checked (Join-Path $target 'unins000.exe') @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART')
Start-Sleep -Seconds 2
foreach ($path in @('ShinyVlcPlayer.exe','ShinyLibraryManager.exe','ShinyLibraryManagerCli.exe','ShinyEnhancementWorker.exe','library-bundles\1.0.0\shiny_spatial.dll','library-bundles\1.1.0\shiny_spatial.dll','library-bundles\1.2.0\shiny_spatial.dll','nr\ShinyNrWorker.exe','runtime\vlc-3.0.24\libvlc.dll')) { if (Test-Path (Join-Path $target $path)) { throw "Uninstaller retained managed file $path" } }
@{ installed=$true; managedLibrarySelection=$true; managedLibraryVersions=3; managedWorkerProbe=$true; verifiedCachedVlc=$true; runtimeAutoDetected=$true; decodedVideoFrames=$ui.decodedVideoFrames; uninstalled=$true; trainedModelInference=$false; signed=$false; scope='Disposable Windows CI user. No physical GPU, neural model, online prerequisite download or sound device validation.' } | ConvertTo-Json | Set-Content (Join-Path $proof 'installer-report.json')
