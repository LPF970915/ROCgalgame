param(
  [string]$DeviceHost = "root@192.168.31.13",
  [string]$AppDir = "/storage/games-external/app/ROCgalgame",
  [string]$FrontendPath = "$PSScriptRoot\dist_glibc234\ROCgalgame\rocgalgame_sdl"
)

$ErrorActionPreference = "Stop"
$frontend = (Resolve-Path -LiteralPath $FrontendPath).Path
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $frontend).Hash.ToLowerInvariant()
$incoming = "/tmp/rocgalgame_sdl-$hash.new"

& ssh -o BatchMode=yes -o ConnectTimeout=8 $DeviceHost "test -d '$AppDir' && test -x '$AppDir/rocgalgame_sdl'"
if ($LASTEXITCODE -ne 0) { throw "Device or installed ROCgalgame tree is unavailable" }

& scp -p $frontend "${DeviceHost}:$incoming"
if ($LASTEXITCODE -ne 0) { throw "Frontend upload failed" }

$remoteCommand = @"
set -eu
app='$AppDir'
incoming='$incoming'
expected='$hash'
set -- `$(sha256sum "`$incoming")
test "`$1" = "`$expected"
if pidof rocgalgame_sdl >/dev/null 2>&1; then
  echo 'ROCgalgame is running; refusing replacement.' >&2
  exit 20
fi
stamp=`$(date +%Y%m%d-%H%M%S)
backup="/storage/games-external/.rocgalgame-backups/rocgalgame_sdl.pre-update-`$stamp"
mkdir -p "`$(dirname "`$backup")"
cp -p "`$app/rocgalgame_sdl" "`$backup"
cp "`$incoming" "`$app/.rocgalgame_sdl.new"
chmod 755 "`$app/.rocgalgame_sdl.new"
sync
mv "`$app/.rocgalgame_sdl.new" "`$app/rocgalgame_sdl"
sync
set -- `$(sha256sum "`$app/rocgalgame_sdl")
test "`$1" = "`$expected"
rm -f "`$incoming"
echo "deployed=`$expected"
echo "backup=`$backup"
"@

$remoteCommand = $remoteCommand -replace "`r`n", "`n"
& ssh $DeviceHost $remoteCommand
if ($LASTEXITCODE -ne 0) { throw "Remote atomic frontend deployment failed" }
