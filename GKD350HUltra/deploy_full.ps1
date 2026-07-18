param(
  [string]$DeviceHost = "root@192.168.31.13",
  [string]$AppDir = "/storage/roms/ports/ROCgalgame",
  [string]$DistRoot = "$PSScriptRoot\dist_lowglibc\ROCgalgame"
)

$ErrorActionPreference = "Stop"
$frontend = (Resolve-Path -LiteralPath (Join-Path $DistRoot "rocgalgame_sdl")).Path
$krkr = (Resolve-Path -LiteralPath (Join-Path $DistRoot "cores\krkr\krkrsdl2")).Path
$frontendHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $frontend).Hash.ToLowerInvariant()
$krkrHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $krkr).Hash.ToLowerInvariant()
$frontendTemp = "/tmp/rocgalgame_sdl-$frontendHash.new"
$krkrTemp = "/tmp/krkrsdl2-$krkrHash.new"

& ssh -o BatchMode=yes -o ConnectTimeout=8 $DeviceHost "test -d '$AppDir' && test -x '$AppDir/rocgalgame_sdl' && test -x '$AppDir/cores/krkr/krkrsdl2'"
if ($LASTEXITCODE -ne 0) { throw "Device or installed ROCgalgame tree is unavailable" }

& scp -p $frontend "${DeviceHost}:$frontendTemp"
if ($LASTEXITCODE -ne 0) { throw "Frontend upload failed" }
& scp -p $krkr "${DeviceHost}:$krkrTemp"
if ($LASTEXITCODE -ne 0) { throw "KRKR upload failed" }

$remoteCommand = @"
set -eu
app='$AppDir'
frontend_incoming='$frontendTemp'
krkr_incoming='$krkrTemp'
frontend_expected='$frontendHash'
krkr_expected='$krkrHash'
set -- `$(sha256sum "`$frontend_incoming")
test "`$1" = "`$frontend_expected"
set -- `$(sha256sum "`$krkr_incoming")
test "`$1" = "`$krkr_expected"
if pidof krkrsdl2 >/dev/null 2>&1 || pidof rocgalgame_sdl >/dev/null 2>&1; then
  echo 'ROCgalgame or KRKR is running; refusing replacement.' >&2
  exit 20
fi
stamp=`$(date +%Y%m%d-%H%M%S)
backup="`$app/../ROCgalgame-backups/full-`$stamp"
mkdir -p "`$backup/cores/krkr"
cp -p "`$app/rocgalgame_sdl" "`$backup/rocgalgame_sdl"
cp -p "`$app/cores/krkr/krkrsdl2" "`$backup/cores/krkr/krkrsdl2"
cp "`$frontend_incoming" "`$app/.rocgalgame_sdl.new"
cp "`$krkr_incoming" "`$app/cores/krkr/.krkrsdl2.new"
chmod 755 "`$app/.rocgalgame_sdl.new" "`$app/cores/krkr/.krkrsdl2.new"
sync
mv "`$app/.rocgalgame_sdl.new" "`$app/rocgalgame_sdl"
mv "`$app/cores/krkr/.krkrsdl2.new" "`$app/cores/krkr/krkrsdl2"
sync
set -- `$(sha256sum "`$app/rocgalgame_sdl")
test "`$1" = "`$frontend_expected"
set -- `$(sha256sum "`$app/cores/krkr/krkrsdl2")
test "`$1" = "`$krkr_expected"
rm -f "`$frontend_incoming" "`$krkr_incoming"
echo "frontend=`$frontend_expected"
echo "krkr=`$krkr_expected"
echo "backup=`$backup"
"@

& ssh $DeviceHost $remoteCommand
if ($LASTEXITCODE -ne 0) { throw "Remote atomic deployment failed" }
