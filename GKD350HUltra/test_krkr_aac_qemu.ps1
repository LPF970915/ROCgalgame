param(
  [string]$AudioFile = "",
  [string]$QemuDistro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$core = Join-Path $PSScriptRoot "dist_lowglibc\ROCgalgame\cores\krkr\krkrsdl2"
$sysroot = Join-Path $PSScriptRoot "sysroot_device"
$fixture = Join-Path $repoRoot "tests\krkr\aac_probe"
$runtime = Join-Path $repoRoot "build\test-data\krkr-aac-qemu"
$data = "$runtime-data"
$font = Join-Path $repoRoot "fonts\ui_font_02.ttf"
if (-not $AudioFile) {
  $AudioFile = Join-Path $repoRoot "build\test-data\murasame_attention1.m4a"
}

foreach ($required in @($AudioFile, $core, $font, (Join-Path $fixture "startup.tjs"))) {
  if (-not (Test-Path -LiteralPath $required)) {
    throw "Missing required file: $required"
  }
}

Remove-Item -LiteralPath $runtime, $data -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $runtime, $data | Out-Null
Copy-Item -LiteralPath (Join-Path $fixture "startup.tjs") -Destination $runtime
Copy-Item -LiteralPath $AudioFile -Destination (Join-Path $runtime "probe_audio.ogg")

function To-WslPath([string]$Path) {
  $resolved = (Resolve-Path -LiteralPath $Path).Path
  return (wsl -d $QemuDistro -- wslpath -a ($resolved -replace '\\', '/')).Trim()
}

$wslCore = To-WslPath $core
$wslSysroot = To-WslPath $sysroot
$wslRuntime = To-WslPath $runtime
$wslData = To-WslPath $data
$wslFont = To-WslPath $font
$stdout = Join-Path $repoRoot "build\test-data\krkr-aac-qemu.stdout.log"
New-Item -ItemType File -Force $stdout | Out-Null
$wslStdout = To-WslPath $stdout

$command = @"
export QEMU_CPU=cortex-a53 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_NOMOUSE=1
nice -n 10 taskset -c 0,1 timeout 12 qemu-aarch64 -L '$wslSysroot' -E LD_LIBRARY_PATH=/usr/lib:/lib \
  '$wslCore' '$wslRuntime' -datapath='$wslData' -deffont='$wslFont' -nosel \
  >'$wslStdout' 2>&1 || true
iconv -f UTF-16LE -t UTF-8 '$wslData/krkr.console.log'
"@

$decoded = wsl -d $QemuDistro -- bash -lc $command
if ($LASTEXITCODE -ne 0) { throw "QEMU AAC probe could not read the KRKR log" }
$decoded | Set-Content -Encoding UTF8 (Join-Path $repoRoot "build\test-data\krkr-aac-qemu.console.txt")
$success = $decoded | Select-String -SimpleMatch "AAC_PLAY_OK" | Select-Object -First 1
if (-not $success) {
  Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue
  throw "KRKR AAC playback probe failed"
}

$success.Line
