param(
  [string]$Krkr2Root = "D:\Works\Tyranor\krkr2",
  [string]$CompatSource = "$PSScriptRoot\patches\krkr2-layerex-compat"
)

$ErrorActionPreference = "Stop"

$targetPluginDir = Join-Path $Krkr2Root "cpp\plugins"
$cmakePath = Join-Path $targetPluginDir "CMakeLists.txt"

foreach ($name in @("layerExImage.cpp", "layerExRaster.cpp", "layerExBTOA.cpp", "layerExBase_wamsoft.hpp")) {
  $source = Join-Path $CompatSource $name
  $target = Join-Path $targetPluginDir $name
  if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Missing AetherKiri compatibility source: $source"
  }
  Copy-Item -LiteralPath $source -Destination $target -Force
}

$cmake = Get-Content -LiteralPath $cmakePath -Raw
$newline = if ($cmake.Contains("`r`n")) { "`r`n" } else { "`n" }
$publicSources = [regex]::Match(
  $cmake,
  '(?s)target_sources\(\$\{PROJECT_NAME\} PUBLIC.*?\r?\n\)'
)
if (-not $publicSources.Success) {
  throw "Could not find the krkr2plugin PUBLIC source list in $cmakePath"
}
$cleanPublicSources = [regex]::Replace(
  $publicSources.Value,
  '(?m)^\s+layerEx(?:Image|Raster|BTOA)\.cpp\r?\n',
  ''
)
$cmake = $cmake.Remove($publicSources.Index, $publicSources.Length)
$cmake = $cmake.Insert($publicSources.Index, $cleanPublicSources)
$compatMarker = "# ROCgalgame layerEx compatibility sources"
$compatPattern = '(?s)\r?\n# ROCgalgame layerEx compatibility sources.*?\r?\ntarget_sources\(\$\{(?:PROJECT_NAME|APP_NAME)\} PRIVATE.*?\r?\n\)'
$cmake = [regex]::Replace($cmake, $compatPattern, '')
$publicSources = [regex]::Match(
  $cmake,
  '(?s)target_sources\(\$\{PROJECT_NAME\} PUBLIC.*?\r?\n\)'
)
$compatBlock = @(
  "",
  "$compatMarker must be direct executable objects.",
  "# A static library drops their ncbind self-registration, while PUBLIC sources",
  "# are recompiled by every static plugin that links krkr2plugin.",
  'target_sources(${APP_NAME} PRIVATE',
  "    layerExImage.cpp",
  "    layerExRaster.cpp",
  "    layerExBTOA.cpp",
  ")"
) -join $newline
$insertAt = $publicSources.Index + $publicSources.Length
$cmake = $cmake.Insert($insertAt, $compatBlock)
Set-Content -LiteralPath $cmakePath -Value $cmake -NoNewline

Write-Host "Applied KRKR2 layerEx compatibility sources to $targetPluginDir"
Write-Host "Registered layerExImage, layerExRaster and layerExBTOA in $cmakePath"
