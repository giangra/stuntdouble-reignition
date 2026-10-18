# Sets up the ReXGlue SDK for this project:
#   1. clones ReXGlue at the pinned tag into thirdparty\rexglue-sdk
#   2. applies patches\sdk\*.patch
#   3. builds the rexglue CLI
# Then configure and build the game with CMake (see README.md).

$ErrorActionPreference = "Stop"

$Root   = $PSScriptRoot
$SdkDir = Join-Path $Root "thirdparty\rexglue-sdk"
$SdkTag = "v0.10.0"

Write-Host "Stunt Double: Reignition - SDK setup" -ForegroundColor Cyan

# Clang, CMake and Ninja ship with Visual Studio
if (-not (Get-Command clang -ErrorAction SilentlyContinue)) {
    . (Join-Path $Root "scripts\devenv.ps1") 2>$null
}
$missing = @("git", "cmake", "ninja", "clang") | Where-Object { -not (Get-Command $_ -ErrorAction SilentlyContinue) }
if ($missing) {
    throw "Missing tools: $($missing -join ', '). Install Visual Studio with the C++ desktop workload, Clang and CMake tools."
}

if (-not (Test-Path (Join-Path $SdkDir ".git"))) {
    git clone --branch $SdkTag --depth 1 https://github.com/rexglue/rexglue-sdk.git $SdkDir
    if ($LASTEXITCODE -ne 0) { throw "Cloning ReXGlue failed" }
}
git -C $SdkDir submodule update --init --recursive --depth 1
if ($LASTEXITCODE -ne 0) { throw "Fetching ReXGlue submodules failed" }

# Git without symlink support writes symlinks as small text files holding the
# target path. libmspack's cabextract tree is made of them; copy the targets in.
$mspack = Join-Path $SdkDir "thirdparty\libmspack"
foreach ($entry in git -C $mspack ls-files -s) {
    if ($entry -notmatch '^120000 \S+ \d+\t(.+)$') { continue }
    $link = Join-Path $mspack $Matches[1]
    if ((Get-Item $link).Length -gt 256) { continue }
    $target = Join-Path (Split-Path $link) (Get-Content $link -Raw).Trim()
    if (Test-Path $target) { Copy-Item $target $link -Force }
}

& (Join-Path $Root "patches\apply_sdk_patches.ps1")
if ($LASTEXITCODE -ne 0) { throw "Applying SDK patches failed" }

Push-Location $SdkDir
try {
    cmake --preset win-amd64
    if ($LASTEXITCODE -ne 0) { throw "Configuring ReXGlue failed" }
    cmake --build out\build\win-amd64 --config Release --target rexglue
    if ($LASTEXITCODE -ne 0) { throw "Building the rexglue CLI failed" }
} finally {
    Pop-Location
}

Write-Host "SDK ready. Next:" -ForegroundColor Green
Write-Host "  cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR=thirdparty/rexglue-sdk"
Write-Host "  cmake --build out\build\win-amd64-relwithdebinfo"
