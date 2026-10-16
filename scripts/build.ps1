# One-click native build for CastlevaniaHarmonyOfDissonanceRecomp
param(
    [string]$BuildType = "Release",
    [string]$GbarecompRoot = "",
    [string]$SdlRoot = "",
    [string]$Toolchain = "C:/MYAPPLY/mingw64",
    [string]$Generator = "Ninja",
    [int]$Jobs = 2,
    [string]$Target = "",
    [switch]$BuildContractTest
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if ($Jobs -lt 1) { throw "Jobs must be positive." }

if (-not $GbarecompRoot) {
    if ($env:GBARECOMP_ROOT) { $GbarecompRoot = $env:GBARECOMP_ROOT }
    else { $GbarecompRoot = Join-Path (Split-Path -Parent $root) "gbarecomp-main" }
}
if (-not (Test-Path (Join-Path $GbarecompRoot "CMakeLists.txt"))) {
    throw "GBARecomp not found at $GbarecompRoot. Clone https://github.com/mstan/gbarecomp (with submodules) as a sibling folder."
}

if (-not $SdlRoot) {
    if ($env:SDL2_ROOT) { $SdlRoot = $env:SDL2_ROOT }
    elseif (Test-Path (Join-Path (Split-Path -Parent $root) "_sdl2\SDL2-2.32.8\x86_64-w64-mingw32\include\SDL2\SDL.h")) {
        $SdlRoot = Join-Path (Split-Path -Parent $root) "_sdl2\SDL2-2.32.8\x86_64-w64-mingw32"
    } elseif (Test-Path (Join-Path $root "third_party\SDL2-2.30.11\x86_64-w64-mingw32\include\SDL2\SDL.h")) {
        $SdlRoot = Join-Path $root "third_party\SDL2-2.30.11\x86_64-w64-mingw32"
    }
}

Write-Host "=== Castlevania: Harmony of Dissonance (GBARecomp) Builder ===" -ForegroundColor Cyan
Write-Host "GBARECOMP_ROOT = $GbarecompRoot"
Write-Host "Jobs           = $Jobs"

# Prefer MinGW toolchain
$gcc = $null
$gxx = $null
if (Test-Path "$Toolchain/bin/gcc.exe") {
    $gcc = "$Toolchain/bin/gcc.exe"
    $gxx = "$Toolchain/bin/g++.exe"
} else {
    $cmdGcc = Get-Command gcc.exe -ErrorAction SilentlyContinue
    $cmdGxx = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($cmdGcc) { $gcc = $cmdGcc.Source }
    if ($cmdGxx) { $gxx = $cmdGxx.Source }
}

$ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue
if (-not $ninja -and $Generator -eq "Ninja") {
    $Generator = "MinGW Makefiles"
}

$cmakeArgs = @(
    "-S", $root,
    "-B", (Join-Path $root "build"),
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DGBARECOMP_ROOT=$GbarecompRoot"
)

if ($gcc -and $gxx) {
    $cmakeArgs += @("-DCMAKE_C_COMPILER=$gcc", "-DCMAKE_CXX_COMPILER=$gxx")
}
if ($SdlRoot) {
    $cmakeArgs += @("-DSDL2_ROOT=$SdlRoot")
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

$buildTarget = if ($Target) { $Target } else { "CastlevaniaHarmonyOfDissonanceRecomp" }
& cmake --build (Join-Path $root "build") --target $buildTarget --config $BuildType --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

if ($BuildContractTest) {
    & cmake --build (Join-Path $root "build") --target launcher_contract --config $BuildType --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Building launcher_contract failed" }
}

Write-Host "Build successful: build\CastlevaniaHarmonyOfDissonanceRecomp.exe (and hod.exe)" -ForegroundColor Green
