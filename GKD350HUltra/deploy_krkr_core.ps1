param(
  [string]$DeviceHost = "root@192.168.31.13",
  [string]$AppDir = "/storage/roms/ports/ROCgalgame",
  [string]$CorePath = "$PSScriptRoot\dist_lowglibc\ROCgalgame\cores\krkr\krkrsdl2"
)

$ErrorActionPreference = "Stop"
$resolvedCore = (Resolve-Path -LiteralPath $CorePath).Path
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedCore).Hash.ToLowerInvariant()
$remoteTemp = "/tmp/krkrsdl2-$hash.new"

& ssh -o BatchMode=yes -o ConnectTimeout=8 $DeviceHost "test -d '$AppDir' && test -x '$AppDir/cores/krkr/krkrsdl2'"
if ($LASTEXITCODE -ne 0) { throw "Device or installed ROCgalgame tree is unavailable" }

& scp -p $resolvedCore "${DeviceHost}:$remoteTemp"
if ($LASTEXITCODE -ne 0) { throw "SCP upload failed" }

$remoteCommand = @"
set -eu
app='$AppDir'
incoming='$remoteTemp'
expected='$hash'
set -- `$(sha256sum "`$incoming")
test "`$1" = "`$expected"
if pidof krkrsdl2 >/dev/null 2>&1; then
  echo 'KRKR core is running; refusing replacement.' >&2
  exit 20
fi
set -- `$(sha256sum "`$app/cores/ons/onsyuri")
ons_before="`$1"
set -- `$(sha256sum "`$app/rocgalgame_sdl")
frontend_before="`$1"
stamp=`$(date +%Y%m%d-%H%M%S)
backup="`$app/cores/krkr/krkrsdl2.pre-update-`$stamp"
cp -p "`$app/cores/krkr/krkrsdl2" "`$backup"
cp "`$incoming" "`$app/cores/krkr/.krkrsdl2.aac.new"
chmod 755 "`$app/cores/krkr/.krkrsdl2.aac.new"
sync
mv "`$app/cores/krkr/.krkrsdl2.aac.new" "`$app/cores/krkr/krkrsdl2"
sync
set -- `$(sha256sum "`$app/cores/krkr/krkrsdl2")
test "`$1" = "`$expected"
set -- `$(sha256sum "`$app/cores/ons/onsyuri")
test "`$ons_before" = "`$1"
set -- `$(sha256sum "`$app/rocgalgame_sdl")
test "`$frontend_before" = "`$1"
rm -f "`$incoming"
echo "deployed=`$expected"
echo "backup=`$backup"
"@

& ssh $DeviceHost $remoteCommand
if ($LASTEXITCODE -ne 0) { throw "Remote atomic deployment failed" }
