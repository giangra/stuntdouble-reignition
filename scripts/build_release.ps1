# Builds the game from a checkout and copies what it needs to run into an
# install folder. The installer runs this; it also works by hand:
#   .\scripts\build_release.ps1 -InstallDir C:\Games\StuntmanIgnition
#
# Needs game\default.xex in the checkout (the installer links it to the
# extracted ISO), Git on PATH and Visual Studio or its Build Tools with the
# C++ tools, Clang and CMake. Lines starting with "##" are progress markers
# the installer reads.

param(
    [Parameter(Mandatory)] [string]$InstallDir
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Preset = "win-amd64-relwithdebinfo"
$BuildDir = Join-Path $Root "out\build\$Preset"

function Step($n, $name) { Write-Host "##STEP $n $name" }

if (-not (Test-Path (Join-Path $Root "game\default.xex"))) {
    throw "game\default.xex not found in $Root"
}

Step 1 "toolchain"
. (Join-Path $PSScriptRoot "devenv.ps1")

Step 2 "sdk"
& (Join-Path $Root "setup.ps1")

Step 3 "configure"
Push-Location $Root
try {
    cmake --preset $Preset -DREXSDK_DIR=thirdparty/rexglue-sdk
    if ($LASTEXITCODE -ne 0) { throw "Configuring the game failed" }

    # The list of generated sources (generated\default\sources.cmake) only
    # exists after codegen, and CMake reads it at configure time. On a fresh
    # checkout: generate first, then configure again so the build sees it.
    cmake --build $BuildDir --target stuntman_codegen
    if ($LASTEXITCODE -ne 0) { throw "Generating the game's code failed" }
    cmake --preset $Preset -DREXSDK_DIR=thirdparty/rexglue-sdk
    if ($LASTEXITCODE -ne 0) { throw "Configuring the game failed" }

    # ~300 generated C++ files compile here.
    Step 4 "build"
    cmake --build $BuildDir --target stuntman stuntman_sync_dlls
    if ($LASTEXITCODE -ne 0) { throw "Building the game failed" }
} finally {
    Pop-Location
}

Step 5 "install"
New-Item -ItemType Directory -Force $InstallDir | Out-Null
Copy-Item (Join-Path $BuildDir "stuntman.exe") $InstallDir -Force
Get-ChildItem $BuildDir -Filter *.dll | Copy-Item -Destination $InstallDir -Force

# The Visual C++ runtime, deployed next to the game (app-local) so it runs
# without the redistributable installed.
$redist = Get-ChildItem (Join-Path $env:STUNTMAN_VS "VC\Redist\MSVC") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } |
    Select-Object -First 1
if ($redist) {
    Get-ChildItem $redist.FullName -Filter *.dll | Copy-Item -Destination $InstallDir -Force
} else {
    Write-Warning "Visual C++ runtime not found in Visual Studio; the game needs the VC++ redistributable installed."
}

Write-Host "##DONE"
