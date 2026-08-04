param(
  [string]$DeviceHost = "root@192.168.31.13",
  [string]$AppDir = "/storage/games-external/app/ROCgalgame",
  [string]$CorePath = "$PSScriptRoot\dist_glibc234\ROCgalgame\cores\krkr\krkr2"
)

$ErrorActionPreference = "Stop"
$resolvedCore = (Resolve-Path -LiteralPath $CorePath).Path
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedCore).Hash.ToLowerInvariant()
$remoteTemp = "/tmp/krkr2-$hash.new"
$remoteCore = "$AppDir/cores/krkr/krkr2"

& ssh -o BatchMode=yes -o ConnectTimeout=8 $DeviceHost "test -d '$AppDir' && test -x '$AppDir/cores/krkr/krkrsdl2'"
if ($LASTEXITCODE -ne 0) { throw "Device or installed ROCgalgame tree is unavailable" }

& scp -p $resolvedCore "${DeviceHost}:$remoteTemp"
if ($LASTEXITCODE -ne 0) { throw "SCP upload failed" }

$remoteCommand = @"
set -eu
app='$AppDir'
incoming='$remoteTemp'
target='$remoteCore'
expected='$hash'
set -- `$(sha256sum "`$incoming")
test "`$1" = "`$expected"
if pidof krkr2 >/dev/null 2>&1; then
  echo 'KRKR2 core is running; refusing replacement.' >&2
  exit 20
fi
stamp=`$(date +%Y%m%d-%H%M%S)
backup_dir="/storage/games-external/ROCgalgame_refactor_backups/krkr2_`$stamp"
mkdir -p "`$backup_dir"
if [ -f "`$target" ]; then cp -p "`$target" "`$backup_dir/krkr2"; fi
cp "`$incoming" "`$app/cores/krkr/.krkr2.new"
chmod 755 "`$app/cores/krkr/.krkr2.new"
sync
mv "`$app/cores/krkr/.krkr2.new" "`$target"
sync
set -- `$(sha256sum "`$target")
test "`$1" = "`$expected"
rm -f "`$incoming"
echo "deployed=`$expected"
echo "backup=`$backup_dir/krkr2"
file "`$target"
if command -v readelf >/dev/null 2>&1; then
  readelf -l "`$target" | grep 'Requesting program interpreter' || true
fi
runtime_ld="`$app/cores/krkr/lib_krkr2:`$app/lib_system_sdl:`$app/lib:/usr/lib:/lib"
if LD_LIBRARY_PATH="`$runtime_ld" ldd "`$target" 2>&1 | grep -q 'not found'; then
  echo 'runtime dependency missing' >&2
  exit 21
fi
"@

$remoteCommand = $remoteCommand -replace "`r`n", "`n"
& ssh $DeviceHost $remoteCommand
if ($LASTEXITCODE -ne 0) { throw "Remote atomic KRKR2 deployment failed" }
