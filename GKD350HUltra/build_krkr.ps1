param(
  [string]$Distro = "Ubuntu",
  [string]$KrkrRoot = "D:\Works\Tyranor\krkrsdl2",
  [ValidateRange(1, 2)]
  [int]$Jobs = 1,
  [ValidateSet("Fast", "Configure", "Full")]
  [string]$Mode = "Fast",
  [ValidateSet("Auto", "On", "Off")]
  [string]$Ccache = "Auto",
  [switch]$Clean,
  [switch]$ForceConfigure,
  [switch]$ConfirmHeavyBuild
)

$ErrorActionPreference = "Stop"
if (-not $ConfirmHeavyBuild) {
  throw "Refusing a KRKR build without -ConfirmHeavyBuild. It runs at low priority with at most two jobs, but may still be lengthy."
}
if ($Clean) {
  $Mode = "Full"
} elseif ($ForceConfigure -and $Mode -eq "Fast") {
  $Mode = "Configure"
}
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$krkrRootPath = (Convert-Path $KrkrRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$wslKrkrRoot = (wsl -d $Distro -- wslpath -a "$krkrRootPath").Trim()
$cmd = "cd '$wslDir' && chmod +x ./build_krkr.sh && KRKR_ROOT='$wslKrkrRoot' KRKR_BUILD_JOBS='$Jobs' KRKR_BUILD_MODE='$Mode' KRKR_USE_CCACHE='$Ccache' KRKR_CONFIRM_HEAVY_BUILD=1 ./build_krkr.sh"
wsl -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "KRKR cross-build failed with exit code $LASTEXITCODE" }

& (Join-Path $PSScriptRoot "write_build_checkpoint.ps1") -KrkrRoot $KrkrRoot
