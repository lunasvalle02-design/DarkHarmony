# Castlevania: Harmony of Dissonance DS - ROCKNIX build entry point for Windows (no make required).
# Every step runs inside the Docker image from rocknix/docker.
# Usage:
#   .\rocknix.ps1 image            # once: build the toolchain image
#   .\rocknix.ps1 all              # fetch + build + package
#   .\rocknix.ps1 deploy -DeviceIp 192.168.1.50 [-DevicePass <root password>]
#   .\rocknix.ps1 shell
param(
  [Parameter(Position = 0)]
  [ValidateSet('image', 'fetch', 'build', 'package', 'deploy', 'shell', 'all', 'clean')]
  [string]$Task = 'all',
  [string]$DeviceIp,
  [string]$DeviceUser = 'root',
  [string]$DevicePass,
  [int]$Jobs = 0,
  [string]$GuestOpt = '',
  [switch]$IncludeGameFiles
)
$ErrorActionPreference = 'Stop'
$Image = 'hod-ds-builder:bookworm'
$Root = $PSScriptRoot

function Invoke-InContainer([string]$Script) {
  $envArgs = @()
  if ($DeviceIp)   { $envArgs += @('-e', "DEVICE_IP=$DeviceIp") }
  if ($DeviceUser) { $envArgs += @('-e', "DEVICE_USER=$DeviceUser") }
  if ($DevicePass) { $envArgs += @('-e', "DEVICE_PASS=$DevicePass") }
  if ($Jobs -gt 0) { $envArgs += @('-e', "JOBS=$Jobs") }
  if ($GuestOpt)   { $envArgs += @('-e', "GUEST_OPT=$GuestOpt") }
  if ($IncludeGameFiles) { $envArgs += @('-e', 'INCLUDE_GAME_FILES=1') }
  & docker run --rm -t -v "${Root}:/work" -w /work @envArgs $Image sh $Script
  if ($LASTEXITCODE -ne 0) { throw "Failed: $Script" }
}

switch ($Task) {
  'image'   { & docker build -t $Image (Join-Path $Root 'rocknix/docker'); if ($LASTEXITCODE) { throw 'docker build failed' } }
  'fetch'   { Invoke-InContainer 'rocknix/scripts/fetch-sources.sh' }
  'build'   { Invoke-InContainer 'rocknix/scripts/build.sh' }
  'package' { Invoke-InContainer 'rocknix/scripts/package.sh' }
  'deploy'  { if (-not $DeviceIp) { throw 'Specify -DeviceIp' }; Invoke-InContainer 'rocknix/scripts/deploy.sh' }
  'shell'   { & docker run --rm -it -v "${Root}:/work" -w /work $Image bash }
  'clean'   { Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $Root 'build/rocknix'), (Join-Path $Root 'dist') }
  'all'     { Invoke-InContainer 'rocknix/scripts/fetch-sources.sh'; Invoke-InContainer 'rocknix/scripts/build.sh'; Invoke-InContainer 'rocknix/scripts/package.sh' }
}
