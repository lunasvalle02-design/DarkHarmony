$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$zip = Join-Path $root "release-stage\CastlevaniaHarmonyOfDissonanceRecomp-windows-x64-v0.1.0-preview.zip"
$testDir = Join-Path $env:TEMP "hod_release_isolated_test"

Write-Host "=== Testing Release Package ===" -ForegroundColor Cyan
Write-Host "Archive: $zip"
if (-not (Test-Path $zip)) { throw "Release archive not found: $zip" }

if (Test-Path $testDir) { Remove-Item -Recurse -Force $testDir }
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

Write-Host "Extracting to: $testDir"
Expand-Archive -LiteralPath $zip -DestinationPath $testDir

$manifestPath = Join-Path $testDir "manifest.json"
if (-not (Test-Path $manifestPath)) { throw "manifest.json not found in release package" }

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
Write-Host "Manifest file count: $($manifest.Count)"

$failed = 0
foreach ($entry in $manifest) {
    $filePath = Join-Path $testDir $entry.file
    if (-not (Test-Path $filePath)) {
        Write-Host "MISSING: $($entry.file)" -ForegroundColor Red
        $failed++
        continue
    }
    $actualHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $entry.sha256) {
        Write-Host "HASH MISMATCH: $($entry.file)" -ForegroundColor Red
        $failed++
    }
}
if ($failed -gt 0) { throw "Manifest verification failed with $failed errors" }
Write-Host "All manifest files and SHA-256 hashes verified OK." -ForegroundColor Green

# Test execution with clean PATH (no developer toolchain in PATH)
$exe = Join-Path $testDir "CastlevaniaHarmonyOfDissonanceRecomp.exe"
$rom = (Get-ChildItem -LiteralPath $root -Filter *.gba | Select-Object -First 1).FullName
$bios = "D:\Project\GameRecomp\gbarecomp-cli-windows-x86_64\gbabios\gba_bios.bin"

$oldPath = $env:PATH
$env:PATH = "C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem"

try {
    Write-Host "Running release binary with isolated PATH (no toolchain)..."
    $p = Start-Process -FilePath $exe -ArgumentList @("--rom", "`"$rom`"", "--bios", "`"$bios`"", "--frames", "60", "--no-window") -WorkingDirectory $testDir -Wait -PassThru -NoNewWindow
    if ($p.ExitCode -ne 0) {
        throw "Release binary exited with code $($p.ExitCode)"
    }
    Write-Host "Release binary ran successfully! ExitCode: $($p.ExitCode)" -ForegroundColor Green

    # Verify Unicode and space path support
    $unicodeTestDir = Join-Path $env:TEMP "白夜协奏曲 测试 目录"
    if (Test-Path $unicodeTestDir) { Remove-Item -Recurse -Force $unicodeTestDir }
    New-Item -ItemType Directory -Path $unicodeTestDir -Force | Out-Null
    Write-Host "Testing Unicode / space path: $unicodeTestDir"
    Expand-Archive -LiteralPath $zip -DestinationPath $unicodeTestDir
    $unicodeExe = Join-Path $unicodeTestDir "CastlevaniaHarmonyOfDissonanceRecomp.exe"
    $up = Start-Process -FilePath $unicodeExe -ArgumentList @("--rom", "`"$rom`"", "--bios", "`"$bios`"", "--frames", "60", "--no-window") -WorkingDirectory $unicodeTestDir -Wait -PassThru -NoNewWindow
    if ($up.ExitCode -ne 0) {
        throw "Release binary in Unicode path exited with code $($up.ExitCode)"
    }
    Write-Host "Unicode / space path run succeeded! ExitCode: $($up.ExitCode)" -ForegroundColor Green
    Remove-Item -Recurse -Force $unicodeTestDir
} finally {
    $env:PATH = $oldPath
}

Write-Host "=== RELEASE VERIFICATION PASSED ===" -ForegroundColor Green
