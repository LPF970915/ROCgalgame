param(
  [string]$Distro = "Ubuntu",
  [ValidateRange(1, 2)]
  [int]$Jobs = 1,
  [switch]$Clean
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$cleanValue = if ($Clean) { "1" } else { "0" }
$cmd = "cd '$wslDir' && ROC_BUILD_JOBS='$Jobs' ROC_CLEAN_BUILD='$cleanValue' ./build_low_glibc.sh"
wsl -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "Frontend cross-build failed with exit code $LASTEXITCODE" }
& (Join-Path $PSScriptRoot "write_build_checkpoint.ps1")
