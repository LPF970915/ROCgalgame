param(
  [ValidateSet("Frontend", "ONS", "KRKRSDL2", "KRKR2", "All")]
  [string]$Target = "All",
  [string]$Distro = "Ubuntu",
  [string]$Image = "rocgalgame-gkd350h-glibc234:22.04",
  [ValidateRange(0.25, 4.0)][double]$CpuLimit = 0.5,
  [string]$CpuSet = "0",
  [ValidateRange(1, 4)][int]$BuildJobs = 1,
  [ValidateRange(60, 1800)][int]$WorkSeconds = 240,
  [ValidateRange(15, 900)][int]$CoolSeconds = 180
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Convert-ToWslPath([string]$Path) {
  if ($Path -notmatch '^([A-Za-z]):\\(.*)$') { throw "Not a Windows drive path: $Path" }
  return "/mnt/$($Matches[1].ToLowerInvariant())/$($Matches[2] -replace '\\', '/')"
}
$wslRepo = Convert-ToWslPath $repoRoot
$sources = @{
  "/sources/ons" = "D:\Works\Tyranor\OnscripterYuri"
  "/sources/krkrsdl2" = "D:\Works\Tyranor\krkrsdl2"
  "/sources/krkr2" = "D:\Works\Tyranor\krkr2"
  "/sources/ffmpeg" = "D:\Works\Tyranor\FFmpeg-n6.0"
}
$args = @("-d", $Distro, "--", "docker", "run", "--rm",
  "--cpus=$CpuLimit", "--cpuset-cpus=$CpuSet", "--memory=10g", "--pids-limit=512",
  "--env", "GLIBC234_BUILD_JOBS=$BuildJobs", "--env", "GLIBC234_SAFE_CPU_SET=$CpuSet",
  "--env", "KRKR2_WORK_SECONDS=$WorkSeconds", "--env", "KRKR2_COOL_SECONDS=$CoolSeconds",
  "--volume", "${wslRepo}:/workspace")
foreach ($entry in $sources.GetEnumerator()) {
  $source = (Resolve-Path $entry.Value).Path
  $args += @("--volume", "$(Convert-ToWslPath $source):$($entry.Key):ro")
}
$args += @(
  "--volume", "rocgalgame-egl-registry-buildtree:/workspace/build/gkd350h-glibc234/vcpkg/buildtrees/egl-registry",
  "--volume", "rocgalgame-boost-config-buildtree:/workspace/build/gkd350h-glibc234/vcpkg/buildtrees/boost-config",
  "--volume", "rocgalgame-opengl-registry-buildtree:/workspace/build/gkd350h-glibc234/vcpkg/buildtrees/opengl-registry"
)
$args += @("--workdir", "/workspace", $Image, "bash",
  "/workspace/GKD350HUltra/build_glibc234_incremental.sh", $Target)
Write-Host "[incremental] $Target at $CpuLimit CPU on logical CPU $CpuSet; jobs=$BuildJobs; work/cool ${WorkSeconds}s/${CoolSeconds}s"
& wsl @args
if ($LASTEXITCODE -ne 0) { throw "glibc 2.34 incremental build failed: $LASTEXITCODE" }
