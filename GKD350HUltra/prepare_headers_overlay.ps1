param(
  [string]$Distro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$wslDir = (wsl -d $Distro -- wslpath -a "$PSScriptRoot").Trim()
$cmd = "cd '$wslDir' && ./prepare_headers_overlay.sh"
wsl -d $Distro -- bash -lc $cmd
