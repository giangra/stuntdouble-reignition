param(
    [string]$SdkDir = "thirdparty\rexglue-sdk"
)

# Applies every patches\sdk\*.patch to the SDK, in name order. Applied patches
# are recorded (with their hash) in the SDK's .stuntman-patches file and
# skipped on later runs, so the script can be rerun. Later patches may touch
# the same lines as earlier ones, so a reverse check alone can't tell.

$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false

$projectRoot = Split-Path -Parent $PSScriptRoot
$sdkPath = Join-Path $projectRoot $SdkDir
$patchFiles = Get-ChildItem (Join-Path $PSScriptRoot "sdk") -Filter *.patch | Sort-Object Name

if (-not (Test-Path $sdkPath)) {
    Write-Error "SDK directory not found: $sdkPath"
    Write-Error "Run setup.ps1 first to clone the SDK."
    exit 1
}

$marker = Join-Path $sdkPath ".stuntman-patches"
$applied = @()
if (Test-Path $marker) { $applied = Get-Content $marker }

Push-Location $sdkPath
try {
    foreach ($patch in $patchFiles) {
        $id = "$($patch.Name) $((Get-FileHash $patch.FullName -Algorithm SHA256).Hash)"
        if ($applied -contains $id) {
            Write-Host "  $($patch.Name): already applied."
            continue
        }
        if ($applied | Where-Object { $_ -like "$($patch.Name) *" }) {
            Write-Error "$($patch.Name) changed since it was applied. Reset the SDK (git -C $SdkDir checkout . ; delete .stuntman-patches) and rerun."
            exit 1
        }

        & git apply --check $patch.FullName 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $applyResult = & git apply $patch.FullName 2>&1
        } else {
            Write-Host "  $($patch.Name): attempting --3way ..."
            $applyResult = & git apply --3way $patch.FullName 2>&1
        }
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to apply $($patch.Name): $applyResult"
            exit 1
        }
        Add-Content $marker $id
        Write-Host "  $($patch.Name): applied."
    }
}
finally {
    Pop-Location
}
exit 0
