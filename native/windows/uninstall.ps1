$ErrorActionPreference = 'Stop'
foreach ($browser in @('Google\Chrome', 'Microsoft\Edge')) {
  $key = "HKCU:\Software\$browser\NativeMessagingHosts\com.shinyengine.companion"
  if (Test-Path -LiteralPath $key) { Remove-Item -LiteralPath $key -Recurse -Force }
}
$destination = Join-Path $env:LOCALAPPDATA 'ShinyEngine'
if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
Write-Host 'Shiny Engine native companion removed. Remove the browser extension separately.'
