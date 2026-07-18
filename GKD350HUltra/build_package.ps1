param(
  [string]$Distro = "Ubuntu",
  [string]$OnsRoot = "D:\Works\Tyranor\OnscripterYuri",
  [string]$KrkrRoot = "D:\Works\Tyranor\krkrsdl2",
  [ValidatePattern('^[0-9A-Za-z._-]+$')]
  [string]$Version = "0.1",
  [ValidateSet("Fast", "Incremental", "Full")]
  [string]$Mode = "Fast",
  [string[]]$BuildTargets = @(),
  [ValidateSet("Stage", "Zip", "Tar", "Both")]
  [string]$Output = "Stage",
  [ValidateSet("Auto", "On", "Off")]
  [string]$Ccache = "Auto",
  [switch]$ConfirmHeavyBuild
)

$ErrorActionPreference = "Stop"
$selectedTargets = @($BuildTargets | ForEach-Object { $_ -split '[,\s]+' } |
  ForEach-Object { $_.Trim().ToUpperInvariant() } | Where-Object { $_ })
$invalidTargets = @($selectedTargets | Where-Object { $_ -notin @("FRONTEND", "ONS", "KRKR") })
if ($invalidTargets.Count -gt 0) {
  throw "Unknown build target(s): $($invalidTargets -join ', '). Use Frontend, ONS, and/or KRKR."
}
if ($Mode -eq "Fast" -and $selectedTargets.Count -gt 0) {
  throw "Fast mode never compiles. Use -Mode Incremental with -BuildTargets to build selected components."
}
if ($Mode -ne "Fast" -and $selectedTargets.Count -eq 0) {
  $selectedTargets = @("FRONTEND", "ONS", "KRKR")
}
$buildFrontend = $Mode -ne "Fast" -and $selectedTargets -contains "FRONTEND"
$buildOns = $Mode -ne "Fast" -and $selectedTargets -contains "ONS"
$buildKrkr = $Mode -ne "Fast" -and $selectedTargets -contains "KRKR"
if ($buildKrkr -and -not $ConfirmHeavyBuild) {
  throw "Building KRKR requires -ConfirmHeavyBuild. Omit KRKR from -BuildTargets to reuse the staged core."
}
$buildValueFrontend = if ($buildFrontend) { "1" } else { "0" }
$buildValueOns = if ($buildOns) { "1" } else { "0" }
$buildValueKrkr = if ($buildKrkr) { "1" } else { "0" }
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$onsRootPath = (Convert-Path $OnsRoot) -replace '\\', '/'
$krkrRootPath = (Convert-Path $KrkrRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$wslOnsRoot = (wsl -d $Distro -- wslpath -a "$onsRootPath").Trim()
$wslKrkrRoot = (wsl -d $Distro -- wslpath -a "$krkrRootPath").Trim()
$cleanValue = if ($Mode -eq "Full") { "1" } else { "0" }
$forceValue = if ($Mode -eq "Full") { "1" } else { "0" }
$confirmValue = if ($ConfirmHeavyBuild) { "1" } else { "0" }
$krkrMode = if ($Mode -eq "Full") { "Full" } else { "Fast" }
$cmd = "cd '$wslDir' && chmod +x ./build_package.sh ./build_low_glibc.sh ./build_onsyuri.sh ./build_krkr.sh ./sync_runtime_assets.sh ./validate_runtime_deps.sh && ONS_ROOT='$wslOnsRoot' KRKR_ROOT='$wslKrkrRoot' PACKAGE_BUILD_FRONTEND='$buildValueFrontend' PACKAGE_BUILD_ONS='$buildValueOns' PACKAGE_BUILD_KRKR='$buildValueKrkr' PACKAGE_OUTPUT='$Output' ROC_BUILD_JOBS=1 ROC_CLEAN_BUILD='$cleanValue' ONS_BUILD_JOBS=1 ONS_FORCE_REBUILD='$forceValue' ONS_CLEAN_BUILD='$cleanValue' KRKR_CONFIRM_HEAVY_BUILD='$confirmValue' KRKR_BUILD_JOBS=1 KRKR_BUILD_MODE='$krkrMode' KRKR_USE_CCACHE='$Ccache' ROCGALGAME_VERSION='$Version' ./build_package.sh"
wsl -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "Package build failed with exit code $LASTEXITCODE" }

& (Join-Path $PSScriptRoot "write_build_checkpoint.ps1") -KrkrRoot $KrkrRoot

$downloads = Join-Path $PSScriptRoot "Downloads"
$zipPath = Join-Path $downloads "ROCgalgame_GKD350HUltra_v$Version.zip"
$tarPath = Join-Path $downloads "ROCgalgame_GKD350HUltra_v$Version.tar.gz"
if ($Output -eq "Stage") {
  Write-Host "[package] staged and validated: $(Join-Path $PSScriptRoot 'dist_lowglibc')"
} elseif (($Output -eq "Zip" -or $Output -eq "Both") -and (Test-Path -LiteralPath $zipPath)) {
  Write-Host "[package] output: $zipPath"
} elseif (($Output -eq "Tar" -or $Output -eq "Both") -and (Test-Path -LiteralPath $tarPath)) {
  Write-Host "[package] output: $tarPath"
} else {
  throw "Package build finished without producing the requested $Output output"
}
if ($Output -eq "Both") { Write-Host "[package] output: $tarPath" }
