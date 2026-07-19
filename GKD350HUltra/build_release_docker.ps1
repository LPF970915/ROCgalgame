param(
  [string]$Version = "",
  [ValidateRange(1, 16)]
  [int]$Jobs = 2,
  [string]$Distro = "Ubuntu",
  [string]$Image = "rocgalgame-gkd350h-release:22.04"
)

$ErrorActionPreference = "Stop"

function Get-NextReleaseVersion {
  param([string]$DownloadsPath)

  $highest = 0
  if (Test-Path -LiteralPath $DownloadsPath) {
    Get-ChildItem -LiteralPath $DownloadsPath -File -Filter "ROCgalgame ver* for GKD350H Ultra.zip" |
      ForEach-Object {
        $match = [regex]::Match($_.Name, '^ROCgalgame ver([0-9]+)\.([0-9]{2}) for GKD350H Ultra\.zip$')
        if ($match.Success) {
          $number = ([int]$match.Groups[1].Value * 100) + [int]$match.Groups[2].Value
          if ($number -gt $highest) { $highest = $number }
        }
      }
  }

  $next = $highest + 1
  return "{0}.{1:D2}" -f [math]::Floor($next / 100), ($next % 100)
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$downloads = Join-Path $PSScriptRoot "Downloads"
if ([string]::IsNullOrWhiteSpace($Version)) {
  $Version = Get-NextReleaseVersion -DownloadsPath $downloads
}
if ($Version -notmatch '^[0-9]+\.[0-9]{2}$') {
  throw "Version must look like 0.01"
}

$wslRepo = (wsl -d $Distro -- wslpath -a ($repoRoot -replace '\\', '/')).Trim()
if (-not $wslRepo) { throw "Unable to resolve repository path in WSL" }
$dockerContext = "$wslRepo/GKD350HUltra/docker"
$dockerfile = "$dockerContext/Dockerfile.release"

Write-Host "[release] build Docker image: $Image"
wsl -d $Distro -- docker build --tag $Image --file $dockerfile $dockerContext
if ($LASTEXITCODE -ne 0) { throw "Docker image build failed with exit code $LASTEXITCODE" }

Write-Host "[release] build package version $Version"
wsl -d $Distro -- docker run --rm `
  --env "ROC_RELEASE_JOBS=$Jobs" `
  --volume "${wslRepo}:/workspace" `
  --workdir /workspace `
  $Image `
  bash /workspace/GKD350HUltra/build_release_docker.sh $Version
if ($LASTEXITCODE -ne 0) { throw "Docker release build failed with exit code $LASTEXITCODE" }

$packageName = "ROCgalgame ver$Version for GKD350H Ultra.zip"
$packagePath = Join-Path $downloads $packageName
if (-not (Test-Path -LiteralPath $packagePath)) {
  throw "Release build finished without producing $packagePath"
}
Write-Host "[release] completed: $packagePath"
