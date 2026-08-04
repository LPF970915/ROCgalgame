param(
  [string]$KrkrRoot = "D:\Works\Tyranor\krkrsdl2",
  [string]$Krkr2Root = "D:\Works\ROCgalgame-krkr2-port",
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

$buildRoot = Join-Path $repoRoot "build\gkd350h-glibc234"
$distRoot = Join-Path $PSScriptRoot "dist_glibc234\ROCgalgame"
$frontendSource = Get-GitState $repoRoot
$krkrSource = Get-GitState $KrkrRoot
$krkr2Source = Get-GitState $Krkr2Root
$cmakeCache = Join-Path $buildRoot "krkrsdl2\CMakeCache.txt"
$krkr2CmakeCache = Join-Path $buildRoot "krkr2\CMakeCache.txt"
$checkpoint = [ordered]@{
  schema = 2
  generated_at = (Get-Date).ToUniversalTime().ToString("o")
  source = [ordered]@{
    frontend = $frontendSource
    krkr = $krkrSource
    krkr2 = $krkr2Source
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
    krkr_source_commit = if ($krkrSource) { $krkrSource.commit } else { $null }
    krkr2_source_commit = if ($krkr2Source) { $krkr2Source.commit } else { $null }
    krkr2_build_meta = Test-Path -LiteralPath (Join-Path $distRoot "cores\krkr\krkr2.build-meta")
  }
  build_policy = [ordered]@{
    default_krkr_mode = "Fast"
    jobs = 1
    priority = "nice 10"
    full_rebuild_only_for = @("toolchain or ABI change", "incompatible CMake option change", "corrupt build cache", "major KRKR source restructuring")
    require_clean_external_sources = $true
    fixed_container_root = "/workspace"
    compiler_cache = "ccache"
    dependency_cache = "vcpkg files binary cache"
    preferred_linker = "mold, then lld, then bfd"
  }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputFullPath) | Out-Null
$checkpoint | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputFullPath -Encoding UTF8
Write-Host "[checkpoint] wrote $outputFullPath"
