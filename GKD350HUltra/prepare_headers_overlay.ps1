param(
  [string]$Distro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$cmd = "cd '$wslDir' && ./prepare_headers_overlay.sh"
wsl -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "Header overlay preparation failed with exit code $LASTEXITCODE" }
