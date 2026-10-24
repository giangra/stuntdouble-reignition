# Runs the game's benchmark: boots into the first career scene, drives it
# (restarting on failure) and records frame times, optionally a profile.
# See src/bench.h.
#   .\tools\bench.ps1 -Name base
#   .\tools\bench.ps1 -Name scale3 -Args "--resolution_scale=3" -Profile
# Results in bench\<Name>\; tools\frames_report.py summarises frames.txt.

param(
    [Parameter(Mandatory)] [string]$Name,
    [string[]]$Args = @(),
    [switch]$Profile,
    [int]$Width = 1280,
    [int]$Height = 720,
    [hashtable]$Env = @{}
)

$Root = Split-Path -Parent $PSScriptRoot
$Exe = Join-Path $Root "out\build\win-amd64-relwithdebinfo\stuntman.exe"
$Out = Join-Path $Root "bench\$Name"
Remove-Item -Recurse -Force $Out -ErrorAction SilentlyContinue

# Menus up to the first scene, then hold the throttle; A every few seconds
# gets through the retry menu when the scene fails.
$script = "10:start 14:a 18:a 22:a 26:a 30:a 33:a 36:a 40:a 43:a 46:a 48:+rt"
for ($t = 52; $t -le 96; $t += 4) { $script += " ${t}:a" }
$script += " 60:shot 80:shot 97:quit"

$env:STUNTMAN_BENCH = $script
$env:STUNTMAN_BENCH_OUT = $Out
$env:STUNTMAN_PROFILE = $null
$env:STUNTMAN_FRAMES = $null
if ($Profile) { $env:STUNTMAN_PROFILE = "50,45" } else { $env:STUNTMAN_FRAMES = "50,45" }
foreach ($k in $Env.Keys) { Set-Item "env:$k" $Env[$k] }

$argList = @("--fullscreen=false", "--window_width=$Width", "--window_height=$Height",
             "--video_mode_width=$Width", "--video_mode_height=$Height",
             "--log_file=bench\$Name\game.log", "--resolution_scale=1") + $Args
$p = Start-Process $Exe -WorkingDirectory $Root -ArgumentList $argList -PassThru
$p.WaitForExit(180000) | Out-Null
if (-not $p.HasExited) { $p.Kill() }
foreach ($k in $Env.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue }
python (Join-Path $PSScriptRoot "frames_report.py") (Join-Path $Out "frames.txt")
