# Builds the release asset: StuntmanSetup.exe, the launcher published as a
# single exe. Run on a machine with the .NET SDK:
#   .\scripts\package_release.ps1
# The version comes from launcher\StuntmanLauncher.csproj. The installer
# fetches the source at the tag v<version>, so tag and push it with the
# release:
#   git tag v0.1.0; git push origin v0.1.0
# The asset contains only this project's code: no game files and no
# recompiled game code.

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Out = Join-Path $Root "out\release"

[xml]$proj = Get-Content (Join-Path $Root "launcher\StuntmanLauncher.csproj")
$version = $proj.Project.PropertyGroup.Version | Where-Object { $_ } | Select-Object -First 1

dotnet publish (Join-Path $Root "launcher\StuntmanLauncher.csproj") -c Release -o (Join-Path $Out "publish") --nologo
if ($LASTEXITCODE -ne 0) { throw "Publishing the launcher failed" }

$asset = Join-Path $Out "StuntmanSetup-$version.exe"
Copy-Item (Join-Path $Out "publish\StuntmanLauncher.exe") $asset -Force
$hash = (Get-FileHash $asset -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Release asset: $asset"
Write-Host "SHA-256: $hash"
Write-Host "Tag to push: v$version"
