param(
  [string]$DeviceHost = "root@192.168.31.13",
  [string]$Version = "0.04",
  [string]$AppDir = "/storage/roms/ports/ROCgalgame",
  [string]$PackagePath = ""
)

$ErrorActionPreference = "Stop"
if ($Version -notmatch '^[0-9]+\.[0-9]{2}$') {
  throw "Version must look like 0.04"
}
if ($AppDir -ne "/storage/roms/ports/ROCgalgame") {
  throw "This deployment script is intentionally restricted to /storage/roms/ports/ROCgalgame"
}
if ([string]::IsNullOrWhiteSpace($PackagePath)) {
  $PackagePath = Join-Path $PSScriptRoot "Downloads\ROCgalgame ver$Version for GKD350H Ultra.zip"
}

$package = (Resolve-Path -LiteralPath $PackagePath).Path
$dist = Join-Path $PSScriptRoot "dist_lowglibc\ROCgalgame"
$frontend = (Resolve-Path -LiteralPath (Join-Path $dist "rocgalgame_sdl")).Path
$ons = (Resolve-Path -LiteralPath (Join-Path $dist "cores\ons\onsyuri")).Path
$krkr = (Resolve-Path -LiteralPath (Join-Path $dist "cores\krkr\krkrsdl2")).Path
$packageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $package).Hash.ToLowerInvariant()
$frontendHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $frontend).Hash.ToLowerInvariant()
$onsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ons).Hash.ToLowerInvariant()
$krkrHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $krkr).Hash.ToLowerInvariant()
$remotePackage = "/tmp/rocgalgame-release-$Version-$packageHash.zip"

& ssh -o BatchMode=yes -o ConnectTimeout=8 $DeviceHost "test -d '$AppDir' && command -v unzip >/dev/null 2>&1"
if ($LASTEXITCODE -ne 0) { throw "Device, installed ROCgalgame tree, or unzip is unavailable" }

& scp -p $package "${DeviceHost}:$remotePackage"
if ($LASTEXITCODE -ne 0) { throw "Release upload failed" }

$remoteCommand = @"
set -eu
app='$AppDir'
ports='/storage/roms/ports'
incoming='$remotePackage'
version='$Version'
package_expected='$packageHash'
frontend_expected='$frontendHash'
ons_expected='$onsHash'
krkr_expected='$krkrHash'
stage="`$ports/.rocgalgame-release-`$version-`$$"
archive="`$stage/archive"
new="`$archive/roms/ports/ROCgalgame"
new_launcher="`$archive/roms/ports/ROCgalgame.sh"
backup=''
state=0

rollback() {
  rc=`$?
  trap - EXIT INT TERM
  if [ "`$state" = 1 ]; then
    for d in games covers game_covers saves cache logs; do
      if [ -e "`$new/`$d" ] && [ ! -e "`$backup/runtime/`$d" ]; then
        mv "`$new/`$d" "`$backup/runtime/`$d"
      fi
    done
    if [ ! -e "`$app" ] && [ -d "`$backup/runtime" ]; then mv "`$backup/runtime" "`$app"; fi
  elif [ "`$state" = 2 ]; then
    failed="`$stage/failed-runtime"
    if [ -d "`$app" ]; then mv "`$app" "`$failed"; fi
    for d in games covers game_covers saves cache logs; do
      if [ -e "`$failed/`$d" ] && [ ! -e "`$backup/runtime/`$d" ]; then
        mv "`$failed/`$d" "`$backup/runtime/`$d"
      fi
    done
    if [ -d "`$backup/runtime" ]; then mv "`$backup/runtime" "`$app"; fi
    if [ -f "`$backup/ROCgalgame.sh" ]; then cp -p "`$backup/ROCgalgame.sh" "`$ports/ROCgalgame.sh"; fi
  fi
  rm -rf "`$stage"
  rm -f "`$incoming"
  exit "`$rc"
}
trap rollback EXIT INT TERM

case "`$stage" in /storage/roms/ports/.rocgalgame-release-*) ;; *) exit 90 ;; esac
set -- `$(sha256sum "`$incoming")
test "`$1" = "`$package_expected"
if pidof rocgalgame_sdl >/dev/null 2>&1 || pidof onsyuri >/dev/null 2>&1 || pidof krkrsdl2 >/dev/null 2>&1; then
  echo 'ROCgalgame or a game core is running; refusing deployment.' >&2
  exit 20
fi

mkdir -p "`$archive"
unzip -q "`$incoming" -d "`$archive"
test -d "`$new"
test -x "`$new/rocgalgame_sdl"
test -x "`$new/cores/ons/onsyuri"
test -x "`$new/cores/krkr/krkrsdl2"
test -x "`$new_launcher"
test -f "`$new/ui.pack"
test ! -e "`$new/ui"
test "`$(cat "`$new/version.txt")" = "`$version"
set -- `$(sha256sum "`$new/rocgalgame_sdl"); test "`$1" = "`$frontend_expected"
set -- `$(sha256sum "`$new/cores/ons/onsyuri"); test "`$1" = "`$ons_expected"
set -- `$(sha256sum "`$new/cores/krkr/krkrsdl2"); test "`$1" = "`$krkr_expected"

stamp=`$(date +%Y%m%d-%H%M%S)
backup="`$ports/ROCgalgame-backups/release-`$version-`$stamp"
mkdir -p "`$backup"
if [ -f "`$ports/ROCgalgame.sh" ]; then cp -p "`$ports/ROCgalgame.sh" "`$backup/ROCgalgame.sh"; fi
mv "`$app" "`$backup/runtime"
state=1

for d in games covers game_covers saves cache logs; do
  if [ -e "`$backup/runtime/`$d" ]; then
    rm -rf "`$new/`$d"
    mv "`$backup/runtime/`$d" "`$new/`$d"
  fi
done
for f in native_config.ini native_keymap.ini; do
  if [ -f "`$backup/runtime/`$f" ]; then cp -p "`$backup/runtime/`$f" "`$new/`$f"; fi
done

mv "`$new" "`$app"
state=2
cp "`$new_launcher" "`$ports/.ROCgalgame.sh.new"
chmod 755 "`$ports/.ROCgalgame.sh.new" "`$app/rocgalgame_sdl" \
  "`$app/cores/ons/onsyuri" "`$app/cores/krkr/krkrsdl2"
mv "`$ports/.ROCgalgame.sh.new" "`$ports/ROCgalgame.sh"
sync

test "`$(cat "`$app/version.txt")" = "`$version"
set -- `$(sha256sum "`$app/rocgalgame_sdl"); test "`$1" = "`$frontend_expected"
set -- `$(sha256sum "`$app/cores/ons/onsyuri"); test "`$1" = "`$ons_expected"
set -- `$(sha256sum "`$app/cores/krkr/krkrsdl2"); test "`$1" = "`$krkr_expected"

state=3
rm -rf "`$stage"
rm -f "`$incoming"
trap - EXIT INT TERM
echo "version=`$version"
echo "frontend=`$frontend_expected"
echo "ons=`$ons_expected"
echo "krkr=`$krkr_expected"
echo "backup=`$backup"
for d in games covers game_covers saves cache logs; do
  if [ -e "`$app/`$d" ]; then printf '%s:' "`$d"; stat -c %i:%s:%Y "`$app/`$d"; fi
done
"@

& ssh $DeviceHost $remoteCommand
if ($LASTEXITCODE -ne 0) { throw "Remote release deployment failed" }
