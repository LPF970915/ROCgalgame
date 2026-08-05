param(
  [ValidateSet("Lock", "Install", "Metadata")]
  [string]$Action,
  [string]$Image = "rocgalgame-gkd350h-glibc234:22.04",
  [string]$LockFile = "$PSScriptRoot\krkr2-vcpkg-dependencies.lock.json",
  [string]$StatusFile = "$PSScriptRoot\..\build\gkd350h-glibc234\krkr2\vcpkg_installed\vcpkg\status",
  [string]$Destination = "$PSScriptRoot\..\build\gkd350h-glibc234\krkr2-clean\vcpkg_installed"
)

$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build\gkd350h-glibc234"))

function Convert-ToContainerPath([string]$Path) {
  $resolved = [System.IO.Path]::GetFullPath($Path)
  if (-not $resolved.StartsWith($repoRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Path must be inside the repository: $resolved"
  }
  $relative = $resolved.Substring($repoRoot.Length).TrimStart('\') -replace '\\', '/'
  return "/workspace/$relative"
}

$lockContainer = Convert-ToContainerPath $LockFile
if ($Action -eq "Lock") {
  $statusContainer = Convert-ToContainerPath $StatusFile
  $command = @(
    "python3", "/workspace/GKD350HUltra/materialize_vcpkg_cache.py", "lock",
    "--cache", "/cache", "--status", $statusContainer, "--output", $lockContainer,
    "--triplet", "arm64-linux-gkd-glibc234"
  )
} else {
  $destinationPath = [System.IO.Path]::GetFullPath($Destination)
  if (-not $destinationPath.StartsWith($buildRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Destination must be inside the isolated build root: $destinationPath"
  }
  if ($Action -eq "Install" -and (Test-Path -LiteralPath $destinationPath)) {
    throw "Clean destination already exists: $destinationPath"
  }
  if ($Action -eq "Metadata" -and -not (Test-Path -LiteralPath $destinationPath)) {
    throw "Installed dependency tree is missing: $destinationPath"
  }
  $destinationContainer = Convert-ToContainerPath $destinationPath
  $subcommand = if ($Action -eq "Install") { "install" } else { "metadata" }
  $command = @("python3", "/workspace/GKD350HUltra/materialize_vcpkg_cache.py", $subcommand)
  if ($Action -eq "Install") { $command += @("--cache", "/cache") }
  $command += @("--lock", $lockContainer, "--destination", $destinationContainer)
}

& docker run --rm `
  --volume "${repoRoot}:/workspace" `
  --volume "rocgalgame-vcpkg-binary-cache:/cache:ro" `
  --workdir /workspace $Image @command
if ($LASTEXITCODE -ne 0) {
  throw "vcpkg cache $Action failed with exit code $LASTEXITCODE"
}
