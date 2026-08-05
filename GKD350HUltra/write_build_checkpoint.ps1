param(
  [string]$OnsRoot = "D:\Works\Tyranor\OnscripterYuri",
  [string]$KrkrRoot = "D:\Works\Tyranor\krkrsdl2",
  [string]$Krkr2Root = "D:\Works\ROCgalgame-krkr2-port",
  [string]$FfmpegRoot = "D:\Works\ROCgalgame-ffmpeg-n6-headers",
  [string]$DockerImage = "rocgalgame-gkd350h-glibc234:22.04",
  [string]$OutputPath = "$PSScriptRoot\..\build\gkd350h-glibc234\build_checkpoint.json"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
$allowedRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build\gkd350h-glibc234"))
if (-not $outputFullPath.StartsWith($allowedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "Checkpoint output must stay under $allowedRoot"
}

function Get-HashOrNull([string]$Path) {
  if (Test-Path -LiteralPath $Path -PathType Leaf) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
  }
  return $null
}

function Get-GitState([string]$Path) {
  if (-not (Test-Path -LiteralPath (Join-Path $Path ".git"))) { return $null }
  $oldPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  $commit = (& git -c "safe.directory=$Path" -C $Path rev-parse HEAD 2>$null)
  $commitExit = $LASTEXITCODE
  $dirtyLines = @(& git -c "safe.directory=$Path" -C $Path status --porcelain=v1 2>$null)
  $statusExit = $LASTEXITCODE
  $ErrorActionPreference = $oldPreference
  if ($commitExit -ne 0 -or $statusExit -ne 0) { return $null }
  return [ordered]@{
    commit = if ($commit) { "$commit".Trim() } else { $null }
    dirty = $dirtyLines.Count -gt 0
  }
}

function Get-Metadata([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
  $fields = [ordered]@{}
  foreach ($line in Get-Content -LiteralPath $Path) {
    if ($line -match '^([^#=]+)=(.*)$') { $fields[$Matches[1]] = $Matches[2] }
  }
  return $fields
}

function Get-DockerImageState([string]$Image) {
  $oldPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  $raw = & docker image inspect $Image --format '{{json .}}' 2>$null
  $exitCode = $LASTEXITCODE
  $ErrorActionPreference = $oldPreference
  if ($exitCode -ne 0 -or -not $raw) { return $null }
  $imageInfo = $raw | ConvertFrom-Json
  return [ordered]@{
    reference = $Image
    id = $imageInfo.Id
    repo_digests = @($imageInfo.RepoDigests)
    created = $imageInfo.Created
  }
}

$buildRoot = Join-Path $repoRoot "build\gkd350h-glibc234"
$distRoot = Join-Path $PSScriptRoot "dist_glibc234\ROCgalgame"
$frontendSource = Get-GitState $repoRoot
$onsSource = Get-GitState $OnsRoot
$krkrSource = Get-GitState $KrkrRoot
$krkr2Source = Get-GitState $Krkr2Root
$ffmpegSource = Get-GitState $FfmpegRoot
$cmakeCache = Join-Path $buildRoot "krkrsdl2\CMakeCache.txt"
$krkr2CmakeCache = Join-Path $buildRoot "krkr2\CMakeCache.txt"
$onsMetadata = Join-Path $distRoot "cores\ons\onsyuri.build-meta"
$krkrMetadata = Join-Path $distRoot "cores\krkr\krkrsdl2.build-meta"
$krkr2Metadata = Join-Path $distRoot "cores\krkr\krkr2.build-meta"
$checkpoint = [ordered]@{
  schema = 3
  generated_at = (Get-Date).ToUniversalTime().ToString("o")
  source = [ordered]@{
    frontend = $frontendSource
    ons = $onsSource
    krkr = $krkrSource
    krkr2 = $krkr2Source
    ffmpeg_headers = $ffmpegSource
  }
  build_environment = [ordered]@{
    container_root = "/workspace"
    docker_image = Get-DockerImageState $DockerImage
    target_triplet = "arm64-linux-gkd-glibc234"
    glibc_baseline = "2.34"
  }
  locks = [ordered]@{
    ons_sha256 = Get-HashOrNull (Join-Path $PSScriptRoot "onsyuri-port.lock")
    krkrsdl2_sha256 = Get-HashOrNull (Join-Path $PSScriptRoot "krkrsdl2-port.lock")
    krkr2_sha256 = Get-HashOrNull (Join-Path $PSScriptRoot "krkr2-port.lock")
    ffmpeg_headers_sha256 = Get-HashOrNull (Join-Path $PSScriptRoot "ffmpeg-headers.lock")
    krkr2_vcpkg_sha256 = Get-HashOrNull (Join-Path $PSScriptRoot "krkr2-vcpkg-dependencies.lock.json")
  }
  cache = [ordered]@{
    frontend_objects = Test-Path -LiteralPath (Join-Path $buildRoot "frontend\obj")
    krkr_cmake_cache = Test-Path -LiteralPath $cmakeCache
    krkr_binary = Test-Path -LiteralPath (Join-Path $buildRoot "krkrsdl2\krkrsdl2")
    krkr2_cmake_cache = Test-Path -LiteralPath $krkr2CmakeCache
    krkr2_binary = Test-Path -LiteralPath (Join-Path $buildRoot "krkr2\bin\krkr2\krkr2")
    sysroot = Test-Path -LiteralPath (Join-Path $buildRoot "sysroot\usr\lib")
  }
  artifacts = [ordered]@{
    frontend_sha256 = Get-HashOrNull (Join-Path $distRoot "rocgalgame_sdl")
    ons_sha256 = Get-HashOrNull (Join-Path $distRoot "cores\ons\onsyuri")
    krkr_sha256 = Get-HashOrNull (Join-Path $distRoot "cores\krkr\krkrsdl2")
    krkr2_sha256 = Get-HashOrNull (Join-Path $distRoot "cores\krkr\krkr2")
    krkr2_gl_sha256 = Get-HashOrNull (Join-Path $distRoot "cores\krkr\lib_krkr2\libGL.so.1")
    ons_build_meta = Get-Metadata $onsMetadata
    krkr_build_meta = Get-Metadata $krkrMetadata
    krkr2_build_meta = Get-Metadata $krkr2Metadata
  }
  build_policy = [ordered]@{
    default_krkr_mode = "Fast"
    jobs = 3
    cpu_set = "0-2"
    cooling_policy = "work 300s, cool 240s"
    krkr2_max_recompile = 20
    require_clean_external_sources = $true
    fixed_container_root = "/workspace"
    compiler_cache = "ccache"
    dependency_cache = "vcpkg files binary cache"
    preferred_linker = "mold, then lld, then bfd"
  }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputFullPath) | Out-Null
$checkpoint | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputFullPath -Encoding UTF8
Write-Host "[checkpoint] wrote $outputFullPath"
