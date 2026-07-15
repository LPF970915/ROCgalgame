param(
  [string]$Distro = "Ubuntu",
  [string]$OnsRoot = "D:\Works\Tyranor\OnscripterYuri",
  [string]$Version = "0.1"
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$onsRootPath = (Convert-Path $OnsRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$wslOnsRoot = (wsl -d $Distro -- wslpath -a "$onsRootPath").Trim()
$cmd = "cd '$wslDir' && chmod +x ./build_package.sh && ONS_ROOT='$wslOnsRoot' ROCGALGAME_VERSION='$Version' ./build_package.sh"
wsl -d $Distro -- bash -lc $cmd

$downloads = Join-Path $PSScriptRoot "Downloads"
$zipPath = Join-Path $downloads "ROCgalgame_GKD350HUltra_v$Version.zip"
$tarPath = Join-Path $downloads "ROCgalgame_GKD350HUltra_v$Version.tar.gz"
if (-not (Test-Path -LiteralPath $zipPath)) {
  $distRoot = Join-Path $PSScriptRoot "dist_lowglibc"
  $launcher = Join-Path $distRoot "ROCgalgame.sh"
  $runtime = Join-Path $distRoot "ROCgalgame"
  if ((Test-Path -LiteralPath $launcher) -and (Test-Path -LiteralPath $runtime)) {
    Remove-Item -LiteralPath (Join-Path $runtime "cache\launch_request.ini") -Force -ErrorAction SilentlyContinue
    Compress-Archive -LiteralPath $launcher, $runtime -DestinationPath $zipPath -Force
  }
}
if (Test-Path -LiteralPath $zipPath) {
  Write-Host "[package] output: $zipPath"
} elseif (Test-Path -LiteralPath $tarPath) {
  Write-Host "[package] output: $tarPath"
} else {
  throw "Package build finished without producing $zipPath or $tarPath"
}
