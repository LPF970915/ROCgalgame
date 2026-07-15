param(
  [string]$Distro = "Ubuntu",
  [string]$OnsRoot = "D:\Works\Tyranor\OnscripterYuri"
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$onsRootPath = (Convert-Path $OnsRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$wslOnsRoot = (wsl -d $Distro -- wslpath -a "$onsRootPath").Trim()
$cmd = "cd '$wslDir' && ONS_ROOT='$wslOnsRoot' ./build_onsyuri.sh"
wsl -d $Distro -- bash -lc $cmd
