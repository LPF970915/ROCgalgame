param(
  [string]$Image = "rocgalgame-gkd350h-glibc234:22.04",
  [switch]$Recover,
  [switch]$RestoreInstalled,
  [switch]$RefreshExisting,
  [switch]$RebuildStatus,
  [switch]$AllowLegacyBuildtrees,
  [string[]]$Ports = @()
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$dockerRepo = $repoRoot -replace '\\', '/'
foreach ($port in $Ports) {
  if ($port -notmatch '^[a-z0-9-]+$') {
    throw "Invalid vcpkg port name: $port"
  }
}
$recoverArg = if ($Recover) { " --recover" } else { "" }
$restoreArg = if ($RestoreInstalled) { " --restore-installed" } else { "" }
$refreshArg = if ($RefreshExisting) { " --refresh-existing" } else { "" }
$statusArg = if ($RebuildStatus) { " --rebuild-status" } else { "" }
$portArg = if ($Ports.Count) { " --ports " + ($Ports -join " ") } else { "" }
$arguments = @(
  "run", "--rm", "--cpus=1", "--memory=10g", "--pids-limit=512",
  "--volume", "${dockerRepo}:/workspace",
  "--volume", "rocgalgame-vcpkg-binary-cache:/workspace/build/gkd350h-glibc234/vcpkg/binary-cache",
  "--volume", "rocgalgame-vcpkg-recovery-staging:/workspace/.local/vcpkg-recovery",
  "--env", "PYTHONUNBUFFERED=1",
  "--workdir", "/workspace", $Image,
  "bash", "-lc"
)
if ($AllowLegacyBuildtrees) {
  $legacyVolumes = @(
    "rocgalgame-egl-registry-buildtree:/workspace/build/gkd350h-glibc234/vcpkg/buildtrees/egl-registry:ro",
    "rocgalgame-boost-config-buildtree:/workspace/build/gkd350h-glibc234/vcpkg/buildtrees/boost-config:ro",
    "rocgalgame-opengl-registry-buildtree:/workspace/build/gkd350h-glibc234/vcpkg/buildtrees/opengl-registry:ro"
  )
  foreach ($volume in $legacyVolumes) {
    $arguments = $arguments[0..10] + @("--volume", $volume) +
      $arguments[11..($arguments.Count - 1)]
  }
}
$command = @(
  "set -euo pipefail",
  "exec 9>/workspace/build/gkd350h-glibc234/.krkr2-build.lock",
  "flock -n 9",
  "python3 /workspace/GKD350HUltra/recover_vcpkg_cache.py --workspace /workspace$recoverArg$restoreArg$refreshArg$statusArg$portArg"
) -join "; "
$arguments += $command

& docker @arguments
if ($LASTEXITCODE -ne 0) {
  throw "vcpkg cache recovery failed with exit code $LASTEXITCODE"
}
