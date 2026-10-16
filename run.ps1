# Launch the player from the project root (GUI, no console).
#   .\run.ps1
#   .\run.ps1 -Rom "path\to\your.gba" -Bios "path\to\gba_bios.bin"
param(
    [string]$Rom = "",
    [string]$Bios = "",
    [string]$Save = "",
    [switch]$Launcher
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$candidates = @(
    (Join-Path $root "build\CastlevaniaHarmonyOfDissonanceRecomp.exe"),
    (Join-Path $root "build\hod.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    throw "Executable not found. Build first: .\scripts\build.ps1"
}

$argList = @()
if ($Launcher) { $argList += "--launcher" }
if ($Rom)      { $argList += @("--rom", $Rom) }
if ($Bios)     { $argList += @("--bios", $Bios) }
if ($Save)     { $argList += @("--save", $Save) }

Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $root
