param(
  [string]$Distro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$cmd = "cd '$wslDir' && ./build_low_glibc.sh"
wsl -d $Distro -- bash -lc $cmd
