param(
  [string]$KrkrRoot = "D:\Works\Tyranor\krkrsdl2",
  [ValidateRange(1, 2)]
  [int]$Jobs = 1,
  [switch]$ConfirmHeavyBuild
)

$ErrorActionPreference = "Stop"
if (-not $ConfirmHeavyBuild) {
  throw "Refusing a heavy KRKR build without -ConfirmHeavyBuild. Two prior parallel builds coincided with Windows power-offs."
}

$hostProcess = [System.Diagnostics.Process]::GetCurrentProcess()
$hostProcess.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
$affinityCount = [Math]::Min(2, [Environment]::ProcessorCount)
$affinityMask = (1 -shl $affinityCount) - 1
$hostProcess.ProcessorAffinity = [IntPtr]$affinityMask

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$krkrRootPath = (Resolve-Path $KrkrRoot).Path
$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "D:\Program Files\MSYS2" }
$gcc = Join-Path $msysRoot "ucrt64\bin\gcc.exe"
$gxx = Join-Path $msysRoot "ucrt64\bin\g++.exe"
$make = Join-Path $msysRoot "ucrt64\bin\mingw32-make.exe"
$windres = Join-Path $msysRoot "ucrt64\bin\windres.exe"
$webpSourceInclude = Join-Path $msysRoot "ucrt64\include\webp"
$webpInclude = Join-Path $projectRoot "build\deps\webp\include"
$webpLibrary = Join-Path $msysRoot "ucrt64\lib\libwebpdecoder.a"
$fallbackCmake = "D:\Program Files\ComfyUI-aki-v1.4\python\Lib\site-packages\cmake\data\bin\cmake.exe"
$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } elseif (Test-Path $fallbackCmake) { $fallbackCmake } else { $null }

foreach ($tool in @($cmake, $gcc, $gxx, $make, $windres, (Join-Path $webpSourceInclude "decode.h"), $webpLibrary)) {
  if (-not $tool -or -not (Test-Path -LiteralPath $tool)) { throw "Missing build tool: $tool" }
}

$webpOverlayDir = Join-Path $webpInclude "webp"
New-Item -ItemType Directory -Force -Path $webpOverlayDir | Out-Null
Copy-Item -Path (Join-Path $webpSourceInclude "*") -Destination $webpOverlayDir -Recurse -Force

$env:Path = "$(Join-Path $msysRoot 'ucrt64\bin');$(Join-Path $msysRoot 'usr\bin');$env:Path"
$buildDir = Join-Path $krkrRootPath "build-host-mingw-safe2"
$makeCmake = $make -replace '\\', '/'
$gccCmake = $gcc -replace '\\', '/'
$gxxCmake = $gxx -replace '\\', '/'
$windresCmake = $windres -replace '\\', '/'
$webpIncludeCmake = $webpInclude -replace '\\', '/'
$webpLibraryCmake = $webpLibrary -replace '\\', '/'
Write-Warning "Starting a sustained compile with Jobs=$Jobs, BelowNormal priority, affinity mask $affinityMask."
& $cmake -S $krkrRootPath -B $buildDir -G "MinGW Makefiles" `
  "-DCMAKE_MAKE_PROGRAM=$makeCmake" "-DCMAKE_C_COMPILER=$gccCmake" "-DCMAKE_CXX_COMPILER=$gxxCmake" `
  "-DCMAKE_RC_COMPILER=$windresCmake" -DCMAKE_BUILD_TYPE=Release -DOPTION_ENABLE_CANVAS=OFF `
  -DOPTION_ENABLE_EXTERNAL_PLUGINS=ON -DKRKRSDL2_ENABLE_PRECOMPILED_HEADERS=OFF `
  "-DKRKRSDL2_WEBP_INCLUDE_DIR=$webpIncludeCmake" "-DKRKRSDL2_WEBP_LIBRARY=$webpLibraryCmake"
if ($LASTEXITCODE -ne 0) { throw "KRKR CMake configure failed with exit code $LASTEXITCODE" }
& $cmake --build $buildDir --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "KRKR host build failed with exit code $LASTEXITCODE" }

$target = Join-Path $buildDir "tvpwin64.exe"
if (-not (Test-Path -LiteralPath $target)) { throw "Build succeeded without producing $target" }
$runtimeDir = Join-Path $projectRoot "cores\krkr"
New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null
Copy-Item -LiteralPath $target -Destination (Join-Path $runtimeDir "tvpwin64.exe") -Force
Write-Host "[krkr_host] installed: $(Join-Path $runtimeDir 'tvpwin64.exe')"
