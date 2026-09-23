# Build the native player, research worker and installer from this checkout.
# Requires Windows x64, Visual Studio C++/SDK, CMake, Git, Python3 and Inno Setup6.
[CmdletBinding()]
param([switch]$SkipInstaller)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:OS -ne 'Windows_NT') { throw 'This script requires Windows; portable policy tests can run separately on Linux.' }
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
  foreach ($tool in @('git','cmake','ctest','python')) { if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing required tool: $tool" } }
  function Run([string]$Program,[string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
  }
  function Dependency([string]$Name,[string]$Repository,[string]$Commit) {
    $folder = Join-Path $root ".deps/$Name"
    if (-not (Test-Path $folder)) {
      Run git @('init',$folder)
      Run git @('-C',$folder,'remote','add','origin',"https://github.com/$Repository.git")
    }
    $origin = (& git -C $folder remote get-url origin).Trim()
    if ($LASTEXITCODE -ne 0 -or $origin -ne "https://github.com/$Repository.git") { throw "Unexpected dependency origin: $Name" }
    if ((& git -C $folder status --porcelain --untracked-files=no)) { throw "Dependency $Name contains modified tracked files; do not overwrite them." }
    Run git @('-C',$folder,'fetch','--depth=1','origin',$Commit)
    Run git @('-C',$folder,'checkout','--detach',$Commit)
    if ((& git -C $folder rev-parse HEAD).Trim() -ne $Commit) { throw "Dependency commit mismatch: $Name" }
  }
  function Archive([string]$Url,[string]$Destination,[string]$Digest) {
    if (-not (Test-Path $Destination)) { Invoke-WebRequest $Url -OutFile $Destination }
    if ((Get-FileHash $Destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Digest) { throw "Archive hash mismatch: $Destination" }
  }
  New-Item -ItemType Directory -Force .deps,artifacts/vlc | Out-Null
  Dependency vlc-source videolan/vlc 6de05adcbaf2e8b85fe86aad4169393098628119
  Dependency nr maanHimself/OpenDLSS-NR 9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1
  Dependency vulkan KhronosGroup/Vulkan-Headers 6802bb4733b63ed5efd3adb308a6c885ef180ea1
  Dependency volk zeux/volk 776893306c5d3b22b6185b5d4a258b81d94572bf
  Archive 'https://github.com/KhronosGroup/glslang/releases/download/16.6.0/glslang-16.6.0-windows-x86_64-release.zip' .deps/glslang.zip 82bf434e69b9bb4829de7e2b4bc2c5e7a7861e53d66cf75e5cc70f5f694a8d9b
  Expand-Archive .deps/glslang.zip .deps/nr/tools/glslang -Force
  & .deps/nr/scripts/build_shaders.ps1
  if ($LASTEXITCODE -ne 0) { throw 'Shader compilation failed' }
  Run cmake @('-S','native/nr-worker','-B','build/nr','-A','x64',"-DNR_SOURCE=$root/.deps/nr","-DVULKAN_HEADERS=$root/.deps/vulkan","-DVOLK_SOURCE=$root/.deps/volk")
  Run cmake @('--build','build/nr','--config','Release','--parallel')
  Run ctest @('--test-dir','build/nr','-C','Release','--output-on-failure')
  New-Item -ItemType Directory -Force build/vlc/Release/nr | Out-Null
  Copy-Item build/nr/Release/ShinyNrWorker.exe build/vlc/Release/nr/ -Force
  Copy-Item .deps/nr/build/shaders build/vlc/Release/nr/ -Recurse -Force
  Copy-Item .deps/nr/build/ptx build/vlc/Release/nr/ -Recurse -Force
  Archive 'https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.zip' .deps/vlc.zip fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323
  Expand-Archive .deps/vlc.zip .deps/runtime -Force
  Run python @('tests/vlc-player/make_fixture.py','.deps/fixtures')
  $fixture = Join-Path $root '.deps/fixtures/moving-original.avi'
  $runtime = Join-Path $root '.deps/runtime/vlc-3.0.24'
  Run cmake @('-S','native/vlc-player','-B','build/vlc','-A','x64',"-DVLC_INCLUDE_DIR=$root/.deps/vlc-source/include","-DVLC_TEST_RUNTIME=$runtime","-DVLC_TEST_FIXTURE=$fixture")
  Run cmake @('--build','build/vlc','--config','Release','--parallel')
  Run ctest @('--test-dir','build/vlc','-C','Release','--output-on-failure')
  & tests/vlc-player/ui.ps1 -Executable "$root/build/vlc/Release/ShinyVlcPlayer.exe" -Runtime $runtime -Fixture $fixture -Output "$root/artifacts/vlc"
  $package = Join-Path $root 'artifacts/package'
  if (Test-Path $package) { Remove-Item $package -Recurse -Force }
  New-Item -ItemType Directory -Force "$package/nr" | Out-Null
  Copy-Item build/vlc/Release/ShinyVlcPlayer.exe $package/
  Copy-Item build/vlc/Release/nr/* $package/nr/ -Recurse
  Copy-Item native/vlc-player/README.md,native/vlc-player/THIRD_PARTY_NOTICES.md,docs/native-neural.md,docs/research-mode.md,LICENSE $package/
  Copy-Item .deps/vlc-source/COPYING.LIB $package/LGPL-2.1.txt
  Copy-Item .deps/nr/LICENSE $package/nr/OpenDLSS-LICENSE.txt
  Copy-Item .deps/nr/NOTICE $package/nr/OpenDLSS-NOTICE.txt
  Copy-Item .deps/volk/LICENSE.md $package/nr/volk-LICENSE.txt
  Copy-Item .deps/vulkan/LICENSE.md $package/nr/Vulkan-Headers-LICENSE.txt
  Get-ChildItem $package -File -Recurse | Sort-Object FullName | ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $_.FullName.Substring($package.Length + 1).Replace('\','/') } | Set-Content "$package/SHA256SUMS.txt"
  Compress-Archive $package/* artifacts/vlc/ShinyPlayer-0.7.0-Windows-x64-Portable.zip -Force
  if (-not $SkipInstaller) {
    $iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
    if (-not (Test-Path $iscc)) { throw 'Install Inno Setup 6, or pass -SkipInstaller to produce only the portable package.' }
    Run $iscc @('/Qp',"/DPackageDir=$package",'installers/windows/player.iss')
    & installers/windows/smoke.ps1
  }
  Copy-Item build/vlc/playback-report.json artifacts/vlc/ -Force
  Get-ChildItem artifacts/vlc -File | Where-Object { $_.Extension -in '.zip','.exe','.png','.json' } | Sort-Object Name | ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $_.Name } | Set-Content artifacts/vlc/SHA256SUMS.txt
  Write-Host 'Native builds and test artifacts are in artifacts/vlc. This does not establish trained-model quality or GPU performance.'
} finally { Pop-Location }
