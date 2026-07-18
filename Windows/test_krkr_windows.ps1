param(
  [string]$KrkrExe = "D:\Works\ROCgalgame\cores\krkr\tvpwin64.exe",
  [string]$Font = "D:\Works\ROCgalgame\fonts\ui_font_02.ttf",
  [string]$PluginProject = "D:\Works\ROCgalgame\tests\krkr\plugin_compat",
  [string]$SenrenProject = "D:\Works\ROCgalgame\build\test-data\senren-motion-host-20260716-222303",
  [ValidateRange(10, 300)]
  [int]$SenrenSeconds = 60,
  [switch]$SkipSenren
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
foreach ($path in @($KrkrExe, $Font, $PluginProject)) {
  if (-not (Test-Path -LiteralPath $path)) { throw "Missing KRKR test input: $path" }
}

function New-TestDataPath([string]$Prefix) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $path = Join-Path $projectRoot "build\test-data\$Prefix-$stamp"
  New-Item -ItemType Directory -Force -Path $path | Out-Null
  return $path
}

function Start-Krkr([string]$Project, [string]$DataPath) {
  $stdout = Join-Path $DataPath "stdout.log"
  $stderr = Join-Path $DataPath "stderr.log"
  $arguments = @("-datapath=$DataPath", "-deffont=$Font", $Project)
  return Start-Process -FilePath $KrkrExe -ArgumentList $arguments `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
    -WindowStyle Hidden -PassThru
}

function Invoke-KrkrAndWait([string]$Project, [string]$DataPath) {
  $stdout = Join-Path $DataPath "stdout.log"
  $stderr = Join-Path $DataPath "stderr.log"
  $arguments = @("-datapath=$DataPath", "-deffont=$Font", $Project)
  return Start-Process -FilePath $KrkrExe -ArgumentList $arguments `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
    -WindowStyle Hidden -Wait -PassThru
}

$pluginData = New-TestDataPath "plugin-compat-font02"
$plugin = Invoke-KrkrAndWait $PluginProject $pluginData
$plugin.Refresh()
if ($plugin.ExitCode -ne 0) {
  throw "KRKR plugin compatibility test failed with exit code $($plugin.ExitCode); logs: $pluginData"
}
Write-Host "[krkr_test] plugin compatibility passed: $pluginData"

if (-not $SkipSenren) {
  if (-not (Test-Path -LiteralPath $SenrenProject)) {
    throw "Missing read-only Senren test project: $SenrenProject"
  }
  $senrenData = New-TestDataPath "senren-webp-font02"
  $senren = Start-Krkr $SenrenProject $senrenData
  if ($senren.WaitForExit($SenrenSeconds * 1000)) {
    $senren.Refresh()
    throw "Senren exited early with code $($senren.ExitCode); logs: $senrenData"
  }
  Stop-Process -Id $senren.Id -Force -ErrorAction SilentlyContinue
  Wait-Process -Id $senren.Id -Timeout 10 -ErrorAction SilentlyContinue
  Write-Host "[krkr_test] Senren remained stable for $SenrenSeconds seconds: $senrenData"
}
