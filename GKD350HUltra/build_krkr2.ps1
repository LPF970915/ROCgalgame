param(
  [string]$Distro = "Ubuntu",
  [string]$Krkr2Root = "D:\Works\ROCgalgame-krkr2-port",
  [string]$Sysroot = "$PSScriptRoot\..\build\gkd350h-glibc234\sysroot",
  [string]$VcpkgRoot = "$PSScriptRoot\..\build\gkd350h-glibc234\vcpkg",
  [ValidateSet("Probe", "Configure", "Build", "FastBuild", "Full")]
  [string]$Mode = "Probe",
  [ValidateRange(1, 4)]
  [int]$Jobs = 1,
  [string]$SafeCpuSet = "0",
  [ValidateRange(60, 1800)]
  [int]$WorkSeconds = 300,
  [ValidateRange(15, 900)]
  [int]$CoolSeconds = 240,
  [ValidateSet("Auto", "On", "Off")]
  [string]$Ccache = "Auto",
  [ValidateSet("Auto", "mold", "lld", "bfd")]
  [string]$Linker = "Auto",
  [switch]$PeriodicCooling,
  [switch]$ConfirmHeavyBuild
)

$ErrorActionPreference = "Stop"
if ($Mode -ne "Probe" -and -not $ConfirmHeavyBuild) {
  throw "KrKr2 configure/build may compile a large vcpkg graph. Pass -ConfirmHeavyBuild explicitly."
}

$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$krkr2RootPath = (Convert-Path $Krkr2Root) -replace '\\', '/'
$sysrootPath = (Convert-Path $Sysroot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$wslKrkr2Root = (wsl -d $Distro -- wslpath -a "$krkr2RootPath").Trim()
$wslSysroot = (wsl -d $Distro -- wslpath -a "$sysrootPath").Trim()
$periodicCoolingValue = if ($PeriodicCooling) { 1 } else { 0 }
$envArgs = "KRKR2_ROOT='$wslKrkr2Root' SYSROOT='$wslSysroot' KRKR2_BUILD_MODE='$Mode' KRKR2_BUILD_JOBS='$Jobs' KRKR2_SAFE_CPU_SET='$SafeCpuSet' KRKR2_WORK_SECONDS='$WorkSeconds' KRKR2_COOL_SECONDS='$CoolSeconds' KRKR2_PERIODIC_COOLING='$periodicCoolingValue' KRKR2_USE_CCACHE='$Ccache' KRKR2_LINKER='$Linker'"

if ($Mode -ne "Probe") {
  $vcpkgRootPath = (Convert-Path $VcpkgRoot) -replace '\\', '/'
  $wslVcpkgRoot = (wsl -d $Distro -- wslpath -a "$vcpkgRootPath").Trim()
  $envArgs += " VCPKG_ROOT='$wslVcpkgRoot' KRKR2_CONFIRM_HEAVY_BUILD=1"
}

$cmd = "cd '$wslDir' && chmod +x ./build_krkr2.sh && $envArgs ./build_krkr2.sh"
wsl -d $Distro -u root -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) {
  throw "KrKr2 AArch64 $Mode failed with exit code $LASTEXITCODE"
}
