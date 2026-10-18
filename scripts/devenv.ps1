# Puts the Visual Studio toolchain (MSVC env, Windows SDK, bundled Clang,
# CMake and Ninja) on PATH for the current PowerShell session.
# Usage: . .\scripts\devenv.ps1

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vsWhere -latest -prerelease -products * -property installationPath
if (-not $vs) { throw "Visual Studio not found" }

& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

$extra = @(
    "$vs\VC\Tools\Llvm\x64\bin",
    "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
    "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
)
$env:PATH = ($extra -join ';') + ';' + $env:PATH
