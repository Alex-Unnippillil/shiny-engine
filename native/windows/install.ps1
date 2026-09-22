param(
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-p]{32}$')][string]$ExtensionId,
  [string]$Executable = "$PSScriptRoot\shiny-native.exe"
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Executable).Path
if ([IO.Path]::GetExtension($source) -ne '.exe') { throw 'Select the compiled shiny-native.exe.' }
$destination = Join-Path $env:LOCALAPPDATA 'ShinyEngine'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$target = Join-Path $destination 'shiny-native.exe'
if ($source -ne $target) { Copy-Item -LiteralPath $source -Destination $target -Force }
$origin = "chrome-extension://$ExtensionId/"
$utf8 = New-Object System.Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $destination 'allowed-origin.txt'), $origin, $utf8)
$manifest = @{
  name='com.shinyengine.companion'
  description='Shiny Engine local Windows SDR spatial preview'
  path=$target
  type='stdio'
  allowed_origins=@($origin)
}
$manifestPath = Join-Path $destination 'com.shinyengine.companion.json'
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 3), $utf8)
foreach ($browser in @('Google\Chrome', 'Microsoft\Edge')) {
  $key = "HKCU:\Software\$browser\NativeMessagingHosts\com.shinyengine.companion"
  New-Item -Path $key -Force | Out-Null
  Set-Item -LiteralPath $key -Value $manifestPath
}
Write-Host "Installed for this Windows user only: $destination"
Write-Host 'No service, scheduled task, firewall rule, model download or automatic capture was created.'
Write-Host 'The unsigned alpha build requires independent hardware validation before production use.'
