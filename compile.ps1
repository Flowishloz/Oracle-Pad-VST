#Requires -Version 5.1
param()

$ErrorActionPreference = "Continue"   # vcvarsall emits non-fatal stderr; don't abort on it

$Root     = $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$VcVars   = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
$JucePath = "E:/DEVELOPMENT/AI/JUCE"
$Vst3Dest = "C:\Program Files\Common Files\VST3\OraclePad.vst3\Contents\x86_64-win"

function Fail([string]$msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function ToSlash([string]$p) { $p.Replace('\', '/') }

# ── 0. Admin check ────────────────────────────────────────────────────────────
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warning "Not running as Administrator -- VST3 copy to Program Files may fail."
}

# ── 1. Import MSVC x64 environment ───────────────────────────────────────────
Write-Host ""
Write-Host "[1/4] Importing MSVC x64 environment..." -ForegroundColor Cyan

if (-not (Test-Path $VcVars)) { Fail "vcvarsall.bat not found at: $VcVars" }

$vcOutput = cmd /c "`"$VcVars`" x64 && set"
$imported = 0
foreach ($line in $vcOutput) {
    if ($line -match "^([^=]+)=(.+)$") {
        [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        $imported++
    }
}
if ($imported -lt 10) { Fail "vcvarsall import looks incomplete ($imported vars)." }

[System.Environment]::SetEnvironmentVariable("CMAKE_MAKE_PROGRAM", $null, "Process")
Write-Host "    MSVC environment loaded ($imported vars)." -ForegroundColor Green

# ── Locate tools ──────────────────────────────────────────────────────────────
$clCmd    = Get-Command cl    -ErrorAction SilentlyContinue
$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
$rcCmd    = Get-Command rc    -ErrorAction SilentlyContinue
$mtCmd    = Get-Command mt    -ErrorAction SilentlyContinue

if (-not $clCmd)    { Fail "cl.exe not found in PATH after vcvarsall." }
if (-not $ninjaCmd) { Fail "ninja not found. Install: winget install Ninja-build.Ninja" }

$clExe    = ToSlash $clCmd.Source
$ninjaExe = ToSlash $ninjaCmd.Source
$rcExe    = if ($rcCmd) { ToSlash $rcCmd.Source } else { $null }
$mtExe    = if ($mtCmd) { ToSlash $mtCmd.Source } else { $null }

Write-Host "    cl.exe    : $clExe"
Write-Host "    ninja.exe : $ninjaExe"
if ($rcExe) { Write-Host "    rc.exe    : $rcExe" }
if ($mtExe) { Write-Host "    mt.exe    : $mtExe" }

# ── 2. CMake configure ────────────────────────────────────────────────────────
Write-Host ""
$ninjaFile = Join-Path $BuildDir "build.ninja"
if (-not (Test-Path $ninjaFile)) {
    Write-Host "[2/4] Configuring CMake..." -ForegroundColor Cyan

    if (Test-Path $BuildDir) {
        Write-Host "    Removing stale build directory..." -ForegroundColor DarkGray
        Remove-Item -Path $BuildDir -Recurse -Force
    }

    $cmakeArgs = @(
        "-B", (ToSlash $BuildDir),
        "-S", (ToSlash $Root),
        "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=$ninjaExe",
        "-DCMAKE_C_COMPILER=$clExe",
        "-DCMAKE_CXX_COMPILER=$clExe",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DJUCE_PATH=$JucePath"
    )
    if ($rcExe) { $cmakeArgs += "-DCMAKE_RC_COMPILER=$rcExe" }
    if ($mtExe) { $cmakeArgs += "-DCMAKE_MT=$mtExe" }

    cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed (exit $LASTEXITCODE)." }
    Write-Host "    Configure complete." -ForegroundColor Green
} else {
    Write-Host "[2/4] Already configured -- skipping." -ForegroundColor DarkGray
}

# ── 3. Build ──────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "[3/4] Building Release..." -ForegroundColor Cyan

cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { Fail "Build failed (exit $LASTEXITCODE)." }
Write-Host "    Build succeeded." -ForegroundColor Green

# ── 4. Deploy VST3 ───────────────────────────────────────────────────────────
Write-Host ""
Write-Host "[4/4] Deploying VST3..." -ForegroundColor Cyan

$candidates = @(
    (Join-Path $BuildDir "OraclePad_artefacts\VST3\OraclePad.vst3\Contents\x86_64-win\OraclePad.vst3"),
    (Join-Path $BuildDir "OraclePad_artefacts\Release\VST3\OraclePad.vst3\Contents\x86_64-win\OraclePad.vst3"),
    (Join-Path $BuildDir "OraclePad_artefacts\Debug\VST3\OraclePad.vst3\Contents\x86_64-win\OraclePad.vst3")
)

$Vst3Binary = $null
foreach ($c in $candidates) { if (Test-Path $c) { $Vst3Binary = $c; break } }
if (-not $Vst3Binary) {
    Fail ("Built VST3 binary not found. Searched:`n  " + ($candidates -join "`n  "))
}

Write-Host "    Source : $Vst3Binary"
Write-Host "    Dest   : $Vst3Dest"

if (-not (Test-Path $Vst3Dest)) {
    New-Item -ItemType Directory -Path $Vst3Dest -Force | Out-Null
}

Copy-Item -Path $Vst3Binary -Destination $Vst3Dest -Force
Write-Host "    Deployed successfully." -ForegroundColor Green
Write-Host ""
Write-Host "Done. Rescan plugins in Ableton to pick up the update." -ForegroundColor White
