# Puts the Visual Studio toolchain (MSVC env, Windows SDK, bundled Clang,
# CMake and Ninja) on PATH for the current PowerShell session.
# Usage: . .\scripts\devenv.ps1

# Only installs that have the C++ tools and Clang qualify, so an older VS
# without them doesn't win over a newer Build Tools install.
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) { throw "Visual Studio not found" }
$vs = & $vsWhere -latest -prerelease -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.Llvm.Clang `
    -property installationPath | Select-Object -First 1
if (-not $vs) { throw "Visual Studio with the C++ tools and Clang not found" }

& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

$extra = @(
    "$vs\VC\Tools\Llvm\x64\bin",
    "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
    "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
)
$env:PATH = ($extra -join ';') + ';' + $env:PATH
$env:STUNTMAN_VS = $vs
