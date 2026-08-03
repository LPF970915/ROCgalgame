param(
  [ValidatePattern('^[0-9A-Za-z._-]+$')]
  [string]$Version = "0.01",
  [ValidateSet("Stage", "Zip")]
  [string]$Output = "Stage",
  [string]$RuntimeSource = "",
  [string]$BuildSysroot = "",
  [string]$H700LibSource = "",
  [string]$Distro = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $RuntimeSource) { $RuntimeSource = Join-Path $repoRoot "GKD350HUltra\dist_glibc234\ROCgalgame" }
if (-not $BuildSysroot) { $BuildSysroot = Join-Path $repoRoot "build\gkd350h-glibc234\sysroot" }
if (-not $H700LibSource) {
  $H700LibSource = Join-Path (Split-Path $repoRoot -Parent) "ROCreader\H700\dist_lowglibc\APPS\ROCreader"
}

function WslPath([string]$Path) {
  $resolved = (Resolve-Path -LiteralPath $Path).Path -replace '\\', '/'
  $value = (wsl -d $Distro -- wslpath -a "$resolved").Trim()
  if ($LASTEXITCODE -ne 0 -or -not $value) { throw "Unable to convert path to WSL: $Path" }
  return $value
}

$rootWsl = WslPath $repoRoot
$runtimeWsl = WslPath $RuntimeSource
$sysrootWsl = WslPath $BuildSysroot
$h700LibWsl = WslPath $H700LibSource
$cmd = "cd '$rootWsl' && chmod +x ./H700/build_package.sh && ROCGALGAME_RUNTIME_SOURCE='$runtimeWsl' ROCGALGAME_BUILD_SYSROOT='$sysrootWsl' ROCGALGAME_H700_LIB_SOURCE='$h700LibWsl' ROCGALGAME_VERSION='$Version' ROCGALGAME_OUTPUT='$Output' bash ./H700/build_package.sh"
wsl -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "H700 package build failed with exit code $LASTEXITCODE" }

$archive = Join-Path $PSScriptRoot "Downloads\ROCgalgame ver$Version for H700 34xxSP.zip"
if ($Output -eq "Zip") {
  if (-not (Test-Path -LiteralPath $archive)) { throw "Expected archive was not produced: $archive" }
  Write-Host "[h700] output: $archive"
} else {
  Write-Host "[h700] staged: $(Join-Path $PSScriptRoot 'dist_lowglibc\release_stage')"
}
