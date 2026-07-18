param(
  [string]$Distro = "Ubuntu",
  [string]$OnsRoot = "D:\Works\Tyranor\OnscripterYuri",
  [ValidateRange(1, 2)]
  [int]$Jobs = 1,
  [switch]$ForceRebuild,
  [switch]$Clean
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$onsRootPath = (Convert-Path $OnsRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$wslOnsRoot = (wsl -d $Distro -- wslpath -a "$onsRootPath").Trim()
$forceValue = if ($ForceRebuild) { "1" } else { "0" }
$cleanValue = if ($Clean) { "1" } else { "0" }
$cmd = "cd '$wslDir' && ONS_ROOT='$wslOnsRoot' ONS_BUILD_JOBS='$Jobs' ONS_FORCE_REBUILD='$forceValue' ONS_CLEAN_BUILD='$cleanValue' ./build_onsyuri.sh"
wsl -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "ONS cross-build failed with exit code $LASTEXITCODE" }
& (Join-Path $PSScriptRoot "write_build_checkpoint.ps1")
