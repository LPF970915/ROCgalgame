param(
  [string]$LockFile = "$PSScriptRoot\krkr2-port.lock",
  [string]$Package = ""
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $LockFile)) {
  throw "KRKR2 port lock does not exist: $LockFile"
}

$values = @{}
foreach ($line in Get-Content -LiteralPath $LockFile) {
  if ($line -match '^([^#=][^=]*)=(.*)$') {
    $values[$Matches[1]] = $Matches[2]
  }
}
if ([string]::IsNullOrWhiteSpace($Package)) {
  $Package = Join-Path (Split-Path $PSScriptRoot -Parent) $values.baseline_package
}
if (-not (Test-Path -LiteralPath $Package)) {
  throw "0.28 baseline package does not exist: $Package"
}

$packageHash = (Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash.ToLowerInvariant()
if ($packageHash -ne $values.baseline_package_sha256.ToLowerInvariant()) {
  throw "0.28 package SHA256 mismatch: actual=$packageHash expected=$($values.baseline_package_sha256)"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Package))
try {
  $entry = $archive.Entries | Where-Object { $_.FullName -eq 'app/ROCgalgame/cores/krkr/krkr2' }
  if ($null -eq $entry) {
    throw "0.28 package does not contain app/ROCgalgame/cores/krkr/krkr2"
  }
  $sha = [System.Security.Cryptography.SHA256]::Create()
  try {
    $stream = $entry.Open()
    try { $coreHash = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose() }
  }
  finally { $sha.Dispose() }
}
finally { $archive.Dispose() }

if ($coreHash -ne $values.baseline_core_sha256.ToLowerInvariant()) {
  throw "0.28 KRKR2 core SHA256 mismatch: actual=$coreHash expected=$($values.baseline_core_sha256)"
}

Write-Host "[krkr2-baseline] package_sha256=$packageHash"
Write-Host "[krkr2-baseline] core_sha256=$coreHash"
Write-Host "[krkr2-baseline] created_at=$($values.baseline_package_created_at)"
