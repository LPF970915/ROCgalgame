param(
  [ValidateSet("Frontend", "ONS", "KRKRSDL2", "KRKR2", "All")]
  [string]$Target = "All",
  [string]$Distro = "Ubuntu",
  [string]$Image = "rocgalgame-gkd350h-glibc234:22.04",
  [ValidateRange(0.25, 4.0)][double]$CpuLimit = 3.0,
  [string]$CpuSet = "0-2",
  [ValidateRange(1, 4)][int]$BuildJobs = 3,
  [ValidateRange(60, 1800)][int]$WorkSeconds = 300,
  [ValidateRange(15, 900)][int]$CoolSeconds = 240,
  [switch]$CheckOnly,
  [switch]$ConfigureKrkr2
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Convert-ToWslPath([string]$Path) {
  if ($Path -notmatch '^([A-Za-z]):\\(.*)$') { throw "Not a Windows drive path: $Path" }
  return "/mnt/$($Matches[1].ToLowerInvariant())/$($Matches[2] -replace '\\', '/')"
}
$wslRepo = Convert-ToWslPath $repoRoot
$sources = switch ($Target) {
  "Frontend" { @{} }
  "ONS" { @{ "/sources/ons" = "D:\Works\Tyranor\OnscripterYuri" } }
  "KRKRSDL2" { @{
      "/sources/krkrsdl2" = "D:\Works\Tyranor\krkrsdl2"
      "/sources/ffmpeg" = "D:\Works\ROCgalgame-ffmpeg-n6-headers"
    } }
  "KRKR2" { @{ "/sources/krkr2" = "D:\Works\ROCgalgame-krkr2-port" } }
  "All" { @{
      "/sources/ons" = "D:\Works\Tyranor\OnscripterYuri"
      "/sources/krkrsdl2" = "D:\Works\Tyranor\krkrsdl2"
      "/sources/krkr2" = "D:\Works\ROCgalgame-krkr2-port"
      "/sources/ffmpeg" = "D:\Works\ROCgalgame-ffmpeg-n6-headers"
    } }
}
$args = @("-d", $Distro, "--", "docker", "run", "--rm",
  "--cpus=$CpuLimit", "--cpuset-cpus=$CpuSet", "--memory=10g", "--pids-limit=512",
  "--env", "GLIBC234_BUILD_JOBS=$BuildJobs", "--env", "GLIBC234_SAFE_CPU_SET=$CpuSet",
  "--env", "GLIBC234_CHECK_ONLY=$(if ($CheckOnly) { '1' } else { '0' })",
  "--env", "GLIBC234_CONFIGURE_KRKR2=$(if ($ConfigureKrkr2) { '1' } else { '0' })",
  "--env", "KRKR2_WORK_SECONDS=$WorkSeconds", "--env", "KRKR2_COOL_SECONDS=$CoolSeconds",
  "--volume", "${wslRepo}:/workspace")
foreach ($entry in $sources.GetEnumerator()) {
  $source = (Resolve-Path $entry.Value).Path
  $args += @("--volume", "$(Convert-ToWslPath $source):$($entry.Key):ro")
}
$args += @(
  "--volume", "rocgalgame-vcpkg-binary-cache:/workspace/build/gkd350h-glibc234/vcpkg/binary-cache",
  "--volume", "rocgalgame-krkr2-ccache:/workspace/build/gkd350h-glibc234/ccache/krkr2"
)
$args += @("--workdir", "/workspace", $Image, "bash",
  "/workspace/GKD350HUltra/build_glibc234_incremental.sh", $Target)
Write-Host "[incremental] $Target at $CpuLimit CPU on logical CPU $CpuSet; jobs=$BuildJobs; work/cool ${WorkSeconds}s/${CoolSeconds}s; check-only=$($CheckOnly.IsPresent)"
& wsl @args
if ($LASTEXITCODE -ne 0) { throw "glibc 2.34 incremental build failed: $LASTEXITCODE" }
