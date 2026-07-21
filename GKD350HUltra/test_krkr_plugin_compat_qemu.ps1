param(
  [string]$QemuDistro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$core = Join-Path $PSScriptRoot "dist_lowglibc\ROCgalgame\cores\krkr\krkrsdl2"
$sysroot = Join-Path $PSScriptRoot "sysroot_device"
$fixture = Join-Path $repoRoot "tests\krkr\plugin_compat"
$runtime = Join-Path $repoRoot "build\test-data\krkr-plugin-compat-qemu"
$data = "$runtime-data"
$font = Join-Path $repoRoot "fonts\ui_font_02.ttf"

foreach ($required in @($core, $font, (Join-Path $fixture "startup.tjs"))) {
  if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
    throw "Missing required file: $required"
  }
}
if (-not (Test-Path -LiteralPath $sysroot -PathType Container)) {
  throw "Missing required directory: $sysroot"
}

wsl -d $QemuDistro -- true
if ($LASTEXITCODE -ne 0) {
  throw "WSL distro '$QemuDistro' is unavailable in the current Windows user context"
}

Remove-Item -LiteralPath $runtime, $data -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $runtime, $data | Out-Null
Copy-Item -LiteralPath (Join-Path $fixture "startup.tjs") -Destination $runtime

function To-WslPath([string]$Path) {
  $resolved = (Resolve-Path -LiteralPath $Path).Path
  if ($resolved -match '^([A-Za-z]):\\(.*)$') {
    $drive = $Matches[1].ToLowerInvariant()
    $rest = $Matches[2] -replace '\\', '/'
    return "/mnt/$drive/$rest"
  }
  return (wsl -d $QemuDistro -- wslpath -a ($resolved -replace '\\', '/')).Trim()
}

$wslCore = To-WslPath $core
$wslSysroot = To-WslPath $sysroot
$wslRuntime = To-WslPath $runtime
$wslData = To-WslPath $data
$wslFont = To-WslPath $font
$stdout = Join-Path $repoRoot "build\test-data\krkr-plugin-compat-qemu.stdout.log"
New-Item -ItemType File -Force $stdout | Out-Null
$wslStdout = To-WslPath $stdout

$command = @"
export QEMU_CPU=cortex-a53 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_NOMOUSE=1 AETHERKIRI_SAVE_TRACE=1
set +e
cd /
probe_data="/tmp/rocgalgame-krkr-plugin-compat-qemu.`$BASHPID"
mkdir -p "`$probe_data"
nice -n 10 taskset -c 0,1 timeout -k 5s 30s qemu-aarch64 -L '$wslSysroot' -E LD_LIBRARY_PATH=/usr/lib:/lib '$wslCore' '$wslRuntime' -datapath="`$probe_data/" -deffont='$wslFont' -nosel >'$wslStdout' 2>&1
qemu_status=`$?
printf '\n[qemu] exit=%s\n' "`$qemu_status" >>'$wslStdout'
sleep 0.2
if [ -s "`$probe_data/plugin_compat_save2.txt" ]; then
  cp "`$probe_data/plugin_compat_save2.txt" '$wslData/plugin_compat_save2.txt'
fi
if [ ! -s "`$probe_data/plugin_compat_ok.txt" ]; then
  cat '$wslStdout'
  if [ -f "`$probe_data/krkr.console.log" ]; then
    iconv -f UTF-16LE -t UTF-8 "`$probe_data/krkr.console.log"
  fi
  exit 86
fi
cp "`$probe_data/plugin_compat_ok.txt" '$wslData/plugin_compat_ok.txt'
iconv -f UTF-16LE -t UTF-8 "`$probe_data/plugin_compat_ok.txt"
rm -rf "`$probe_data"
"@

$commandFile = Join-Path $repoRoot "build\test-data\krkr-plugin-compat-qemu.command.sh"
$command = $command -replace "`r`n", "`n"
[System.IO.File]::WriteAllText(
  $commandFile, $command, [System.Text.UTF8Encoding]::new($false))
$wslCommandFile = To-WslPath $commandFile
$decoded = wsl -d $QemuDistro -- bash $wslCommandFile
if ($LASTEXITCODE -ne 0) {
  Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue
  throw "QEMU plugin compatibility probe did not produce its completion marker"
}
$decodedLog = Join-Path $repoRoot "build\test-data\krkr-plugin-compat-qemu.console.txt"
$decoded | Set-Content -Encoding UTF8 $decodedLog

$success = $decoded | Select-String -SimpleMatch "PLUGIN_COMPAT_OK" | Select-Object -First 1
if (-not $success) {
  Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue
  throw "KRKR plugin compatibility probe failed"
}

$saveProbe = Join-Path $data "plugin_compat_save2.txt"
if (-not (Test-Path -LiteralPath $saveProbe -PathType Leaf) -or
    (Get-Item -LiteralPath $saveProbe).Length -eq 0) {
  throw "saveStruct probe did not create a non-empty file: $saveProbe"
}

$completionProbe = Join-Path $data "plugin_compat_ok.txt"
if (-not (Test-Path -LiteralPath $completionProbe -PathType Leaf) -or
    (Get-Item -LiteralPath $completionProbe).Length -eq 0) {
  throw "completion probe did not create a non-empty file: $completionProbe"
}

$success.Line
Write-Host "saveStruct output: $saveProbe"
