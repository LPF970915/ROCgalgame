param(
  [string]$Distro = "Ubuntu",
  [string]$Image = "rocgalgame-gkd350h-glibc234:22.04",
  [ValidateRange(0.25, 4.0)][double]$CpuLimit = 3.0,
  [string]$CpuSet = "0-2",
  [ValidateRange(1, 4)][int]$BuildJobs = 3,
  [ValidateRange(60, 1800)][int]$WorkSeconds = 300,
  [ValidateRange(15, 900)][int]$CoolSeconds = 240
)

$ErrorActionPreference = "Stop"
$resumeBuild = if ($env:RESUME_BUILD -eq "1") { "1" } else { "0" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$h700 = (Resolve-Path "D:\Works\ROCreader\H700\sysroot_device").Path
$ons = (Resolve-Path "D:\Works\Tyranor\OnscripterYuri").Path
$krkrsdl2 = (Resolve-Path "D:\Works\Tyranor\krkrsdl2").Path
$krkr2 = (Resolve-Path "D:\Works\ROCgalgame-krkr2-port").Path
$ffmpeg = (Resolve-Path "D:\Works\ROCgalgame-ffmpeg-n6-headers").Path

function Convert-ToWslPath([string]$Path) {
  if ($Path -notmatch '^([A-Za-z]):\\(.*)$') { throw "Not a Windows drive path: $Path" }
  return "/mnt/$($Matches[1].ToLowerInvariant())/$($Matches[2] -replace '\\', '/')"
}

$wslRepo = Convert-ToWslPath $repoRoot
$mounts = @(
  "${wslRepo}:/workspace",
  "$(Convert-ToWslPath $h700):/sources/h700-sysroot:ro",
  "$(Convert-ToWslPath $ons):/sources/ons:ro",
  "$(Convert-ToWslPath $krkrsdl2):/sources/krkrsdl2:ro",
  "$(Convert-ToWslPath $krkr2):/sources/krkr2:ro",
  "$(Convert-ToWslPath $ffmpeg):/sources/ffmpeg:ro"
)

Write-Host "[glibc234] build image $Image"
wsl -d $Distro -- docker build --cpu-period 100000 --cpu-quota ([int]($CpuLimit * 100000)) `
  --cpuset-cpus $CpuSet --tag $Image `
  --file "$wslRepo/GKD350HUltra/docker/Dockerfile.glibc234" "$wslRepo/GKD350HUltra/docker"
if ($LASTEXITCODE -ne 0) { throw "Docker image build failed: $LASTEXITCODE" }

$args = @("-d", $Distro, "--", "docker", "run", "--rm",
  "--cpus=$CpuLimit", "--cpuset-cpus=$CpuSet", "--memory=10g", "--pids-limit=512",
  "--env", "RESUME_BUILD=$resumeBuild",
  "--env", "GLIBC234_BUILD_JOBS=$BuildJobs", "--env", "GLIBC234_SAFE_CPU_SET=$CpuSet",
  "--env", "KRKR2_WORK_SECONDS=$WorkSeconds", "--env", "KRKR2_COOL_SECONDS=$CoolSeconds")
foreach ($mount in $mounts) { $args += @("--volume", $mount) }
$args += @(
  "--volume", "rocgalgame-vcpkg-binary-cache:/workspace/build/gkd350h-glibc234/vcpkg/binary-cache",
  "--volume", "rocgalgame-krkr2-ccache:/workspace/build/gkd350h-glibc234/ccache/krkr2"
)
$args += @("--workdir", "/workspace", $Image, "bash", "/workspace/GKD350HUltra/build_glibc234_all.sh")
Write-Host "[glibc234] run at $CpuLimit CPU on logical CPU $CpuSet; jobs=$BuildJobs; work/cool ${WorkSeconds}s/${CoolSeconds}s"
& wsl @args
if ($LASTEXITCODE -ne 0) { throw "glibc 2.34 full rebuild failed: $LASTEXITCODE" }
