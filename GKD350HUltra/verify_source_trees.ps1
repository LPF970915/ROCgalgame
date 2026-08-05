param(
  [string]$ProjectRoot = "D:\Works\ROCgalgame",
  [string]$OnsRoot = "D:\Works\Tyranor\OnscripterYuri",
  [string]$Krkr2Root = "D:\Works\ROCgalgame-krkr2-port",
  [string]$KrkrSdl2Root = "D:\Works\Tyranor\krkrsdl2",
  [string]$FfmpegRoot = "D:\Works\ROCgalgame-ffmpeg-n6-headers",
  [switch]$RequireClean,
  [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"

function Get-LockValue([string]$Path, [string]$Key) {
  if (-not (Test-Path -LiteralPath $Path)) { return "" }
  $line = Get-Content -LiteralPath $Path | Where-Object { $_ -match "^$([regex]::Escape($Key))=" } | Select-Object -First 1
  if ($null -eq $line) { return "" }
  return ($line -split "=", 2)[1]
}

function Get-RepoState([string]$Name, [string]$Path, [string]$LockPath, [string]$ExpectedRemote) {
  if (-not (Test-Path -LiteralPath (Join-Path $Path ".git"))) {
    throw "$Name is not a Git worktree: $Path"
  }
  $head = (& git -C $Path rev-parse HEAD).Trim()
  $remote = (& git -C $Path config --get remote.origin.url 2>$null).Trim()
  $dirty = @(& git -c core.autocrlf=true -C $Path status --porcelain=v1)
  $locked = if ([string]::IsNullOrWhiteSpace($LockPath)) { $head } else { Get-LockValue $LockPath "source_commit" }
  $remoteOk = $remote -eq $ExpectedRemote -or $remote -eq ($ExpectedRemote + ".git")
  $commitOk = $head -eq $locked
  $clean = $dirty.Count -eq 0
  [pscustomobject]@{
    name = $Name
    path = $Path
    head = $head
    locked_commit = $locked
    commit_ok = $commitOk
    remote = $remote
    remote_ok = $remoteOk
    clean = $clean
    dirty_entries = $dirty.Count
    status = if (-not $commitOk) { "LOCK_MISMATCH" } elseif (-not $remoteOk) { "REMOTE_MISMATCH" } elseif (-not $clean) { "DIRTY" } else { "OK" }
  }
}

$krkr2Lock = Join-Path $ProjectRoot "GKD350HUltra\krkr2-port.lock"
$krkrSdl2Lock = Join-Path $ProjectRoot "GKD350HUltra\krkrsdl2-port.lock"
$onsLock = Join-Path $ProjectRoot "GKD350HUltra\onsyuri-port.lock"
$ffmpegLock = Join-Path $ProjectRoot "GKD350HUltra\ffmpeg-headers.lock"
$states = @(
  Get-RepoState "rocgalgame" $ProjectRoot "" "https://github.com/LPF970915/ROCgalgame"
  Get-RepoState "onsyuri" $OnsRoot $onsLock "https://github.com/LPF970915/ROCgalgame-onsyuri-port"
  Get-RepoState "krkr2" $Krkr2Root $krkr2Lock "https://github.com/LPF970915/ROCgalgame-krkr2-port"
  Get-RepoState "krkrsdl2" $KrkrSdl2Root $krkrSdl2Lock "https://github.com/LPF970915/ROCgalgame-krkrsdl2-port"
  Get-RepoState "ffmpeg-headers" $FfmpegRoot $ffmpegLock "https://github.com/FFmpeg/FFmpeg"
)

$legacy = Join-Path (Split-Path $KrkrSdl2Root -Parent) "krkr2"
$warnings = @()
if (Test-Path -LiteralPath $legacy) {
  $warnings += "Obsolete checkout exists and must not be used: $legacy"
}

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
  $ReportPath = Join-Path $ProjectRoot ".local\source-tree-status.json"
}
New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath -Parent) | Out-Null
$report = [pscustomobject]@{
  generated_at = (Get-Date).ToString("o")
  project_root = $ProjectRoot
  states = $states
  warnings = $warnings
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
$states | Format-Table name,status,head,locked_commit,dirty_entries -AutoSize
foreach ($warning in $warnings) { Write-Warning $warning }
Write-Host "report=$ReportPath"

$fatal = @($states | Where-Object { -not $_.commit_ok -or -not $_.remote_ok })
if ($RequireClean) { $fatal += @($states | Where-Object { -not $_.clean }) }
if ($fatal.Count -gt 0) { exit 1 }
