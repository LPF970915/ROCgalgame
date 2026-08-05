param(
  [string]$DeviceHost = "root@192.168.31.13",
  [string]$Version = "0.33",
  [string]$AppDir = "/storage/games-external/app/ROCgalgame",
  [ValidateRange(20, 120)][int]$RunSeconds = 45,
  [string]$BindAddress = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$distRoot = Join-Path $PSScriptRoot "dist_glibc234\ROCgalgame"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$localReport = Join-Path $repoRoot ".local\device_acceptance\$Version-$stamp"
$remoteRoot = "/tmp/rocgalgame-$Version-acceptance-$stamp"
$remoteLogRoot = "$AppDir/logs/$Version-acceptance-$stamp"
New-Item -ItemType Directory -Force -Path $localReport | Out-Null
$sshArgs = @("-o", "BatchMode=yes", "-o", "ConnectTimeout=8")
$scpArgs = @("-q", "-p")
if (-not [string]::IsNullOrWhiteSpace($BindAddress)) {
  $sshArgs += @("-b", $BindAddress)
  $scpArgs += @("-o", "BindAddress=$BindAddress")
}

function Get-Sha256([string]$Path) {
  return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Invoke-Ssh([string]$Command) {
  $output = & ssh @sshArgs $DeviceHost $Command 2>&1
  $exitCode = $LASTEXITCODE
  $output | ForEach-Object { Write-Host $_ }
  if ($exitCode -ne 0) { throw "Remote command failed with exit code $exitCode" }
  return @($output)
}

function Copy-ToDevice([string]$LocalPath, [string]$RemotePath) {
  $destination = "{0}:{1}" -f $DeviceHost, $RemotePath
  & scp @scpArgs $LocalPath $destination
  if ($LASTEXITCODE -ne 0) { throw "Failed to upload $LocalPath" }
}

function Run-SweepCase(
  [string]$Id,
  [string]$ExpectedRuntime,
  [string]$Label,
  [string]$ForceKrkrRuntime = "",
  [string]$GamesDir = "",
  [string]$TitleRegex = ""
) {
  $caseLog = "$remoteLogRoot/$Label"
  $forceRuntimeEnv = if ([string]::IsNullOrWhiteSpace($ForceKrkrRuntime)) {
    ""
  } else {
    " FORCE_KRKR_RUNTIME='$ForceKrkrRuntime'"
  }
  $gamesDirEnv = if ([string]::IsNullOrWhiteSpace($GamesDir)) {
    ""
  } else {
    " GAMES_DIR='$GamesDir'"
  }
  $caseSelector = if ([string]::IsNullOrWhiteSpace($Id)) {
    "TITLE_REGEX='$TitleRegex'"
  } else {
    "CASE_FILTER=',$Id,'"
  }
  $command = "set -eu; rm -rf '$caseLog'; " +
    "if APP_DIR='$AppDir' HELPER='$remoteRoot/gkd_uinput_sequence.py' CORE_FILTER=all$forceRuntimeEnv$gamesDirEnv " +
    "$caseSelector MAX_CASES=1 RUN_SECONDS='$RunSeconds' " +
    "CAPTURE_SECONDS='3 12 24 36' REQUIRE_FRAME_DIFF=0 REQUIRE_SWAP_FRAME=1 " +
    "LOG_DIR='$caseLog' TEST_ROOT='$remoteRoot/$Label-test' " +
    "sh '$remoteRoot/frontend_sweep.sh' >'$remoteRoot/$Label.out' 2>&1; " +
    "then cat '$remoteRoot/$Label.out'; else cat '$remoteRoot/$Label.out'; exit 1; fi"
  $caseOutput = & ssh @sshArgs $DeviceHost $command 2>&1
  $caseExit = $LASTEXITCODE
  $caseOutput | ForEach-Object { Write-Host $_ }
  $caseOutput | Out-File -LiteralPath (Join-Path $localReport "$Label.out") -Encoding utf8
  $source = "{0}:{1}" -f $DeviceHost, $caseLog
  $downloadArgs = @($scpArgs) + @("-r")
  & scp @downloadArgs $source $localReport
  if ($LASTEXITCODE -ne 0) { throw "Failed to download $Label report" }
  if ($caseExit -ne 0) { throw "$Label sweep failed with exit code $caseExit" }

  $summary = Join-Path $localReport "$Label\summary.tsv"
  $rows = @(Import-Csv -Delimiter ([char]9) -LiteralPath $summary)
  if ($rows.Count -ne 1) { throw "$Label did not run exactly one game" }
  if ($rows[0].status -ne "pass") { throw "$Label failed with status $($rows[0].status)" }
  if ($rows[0].notes -notmatch "runtime=$([regex]::Escape($ExpectedRuntime))(;|$)") {
    throw "$Label selected the wrong runtime: $($rows[0].notes)"
  }
}

$expected = [ordered]@{
  "rocgalgame_sdl" = Get-Sha256 (Join-Path $distRoot "rocgalgame_sdl")
  "cores/ons/onsyuri" = Get-Sha256 (Join-Path $distRoot "cores\ons\onsyuri")
  "cores/krkr/krkrsdl2" = Get-Sha256 (Join-Path $distRoot "cores\krkr\krkrsdl2")
  "cores/krkr/krkr2" = Get-Sha256 (Join-Path $distRoot "cores\krkr\krkr2")
}

try {
  $remoteHashes = Invoke-Ssh "set -eu; test -d '$AppDir'; cat '$AppDir/version.txt'; sha256sum '$AppDir/rocgalgame_sdl' '$AppDir/cores/ons/onsyuri' '$AppDir/cores/krkr/krkrsdl2' '$AppDir/cores/krkr/krkr2'; mkdir -p '$remoteRoot' '$remoteLogRoot'"
  if (@($remoteHashes)[0].Trim() -ne $Version) { throw "Device version is not $Version" }
  foreach ($entry in $expected.GetEnumerator()) {
    $matched = @($remoteHashes | Where-Object { $_ -match "^$($entry.Value)\s+.*$([regex]::Escape($entry.Key))$" })
    if ($matched.Count -ne 1) { throw "Device hash mismatch for $($entry.Key)" }
  }

  Copy-ToDevice (Join-Path $repoRoot "scripts\gkd_uinput_sequence.py") "$remoteRoot/gkd_uinput_sequence.py"
  Copy-ToDevice (Join-Path $repoRoot "scripts\krkr2_frontend_interactive_sweep.sh") "$remoteRoot/frontend_sweep.sh"
  Copy-ToDevice (Join-Path $PSScriptRoot "run_krkr2_minimal_test.sh") "$remoteRoot/run_krkr2_minimal_test.sh"
  Copy-ToDevice (Join-Path $PSScriptRoot "krkr2_minimal_test\startup.tjs") "$remoteRoot/startup.tjs"
  Copy-ToDevice (Join-Path $PSScriptRoot "krkr2_minimal_test\game.ini") "$remoteRoot/game.ini"
  Invoke-Ssh "mkdir -p '$remoteRoot/fallback-games/krkrsdl2-minimal'" | Out-Null
  Copy-ToDevice (Join-Path $repoRoot "tests\krkr\minimal_tjs\startup.tjs") "$remoteRoot/fallback-games/krkrsdl2-minimal/startup.tjs"
  Copy-ToDevice (Join-Path $repoRoot "tests\krkr\minimal_tjs\game.ini") "$remoteRoot/fallback-games/krkrsdl2-minimal/game.ini"

  $hardware = "set -eu; chmod 755 '$remoteRoot/'*.sh '$remoteRoot/'*.py; " +
    "APP_DIR='$AppDir' PROJECT='$remoteRoot' LOG_DIR='$remoteLogRoot/krkr2-hardware' " +
    "ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland GDK_BACKEND=wayland SDL_VIDEODRIVER=wayland " +
    "REQUIRE_HARDWARE=1 RUN_SECONDS=12 sh '$remoteRoot/run_krkr2_minimal_test.sh'"
  Invoke-Ssh $hardware | Out-File -LiteralPath (Join-Path $localReport "krkr2-hardware.out") -Encoding utf8

  Run-SweepCase "game-98a71a832a1d" "onsyuri" "ons-moon-princess"
  Run-SweepCase "" "krkrsdl2" "krkrsdl2-minimal" "krkrsdl2" "$remoteRoot/fallback-games" "^krkrsdl2-minimal$"
  Run-SweepCase "game-6eb87c1dda4b" "krkr2" "krkr2-nekopara-vol2"

  Invoke-Ssh "rm -rf '$remoteRoot'" | Out-Null
  Write-Host "[device_acceptance] PASS version=$Version report=$localReport"
} catch {
  Write-Error "[device_acceptance] FAIL report=$localReport error=$($_.Exception.Message)"
  throw
}
