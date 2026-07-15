param(
  [Parameter(Mandatory = $true)]
  [string]$DeviceHost,
  [int]$Port = 22,
  [string]$Distro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$cmd = "cd '$wslDir' && DEVICE_HOST='$DeviceHost' SSH_PORT='$Port' ./sync_sysroot.sh"
wsl -d $Distro -- bash -lc $cmd
