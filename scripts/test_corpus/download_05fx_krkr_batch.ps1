[CmdletBinding()]
param(
    [ValidateRange(1, 1000)]
    [int]$Count = 10,
    [ValidateRange(1, 1000)]
    [int]$StartAt = 1
)

$ErrorActionPreference = 'Stop'
$site = 'https://05fx.022016.xyz'
$remoteDirectory = '/kirikiroid2' + (-join [char[]](0x6E38, 0x620F, 0x8D44, 0x6E90, 0x5E93))
$archivePassword = '05' + (-join [char[]](0x53F7, 0x673A))
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$gamesDirectory = Join-Path $repoRoot 'games'
$archiveDirectory = Join-Path $repoRoot 'cache\05fx_downloads'
$extractDirectory = Join-Path $repoRoot 'cache\05fx_extract'
$sevenZip = 'C:\Program Files\7-Zip\7z.exe'

if (-not (Test-Path -LiteralPath $sevenZip)) {
    throw "7-Zip is missing: $sevenZip"
}

New-Item -ItemType Directory -Force -Path $gamesDirectory, $archiveDirectory, $extractDirectory | Out-Null

function Invoke-OpenListApi {
    param(
        [Parameter(Mandatory)]
        [string]$Endpoint,
        [Parameter(Mandatory)]
        [hashtable]$Body
    )

    $json = $Body | ConvertTo-Json -Compress
    $bytes = [Text.Encoding]::UTF8.GetBytes($json)
    Invoke-RestMethod `
        -Uri "$site/api/fs/$Endpoint" `
        -Method Post `
        -ContentType 'application/json; charset=utf-8' `
        -Body $bytes `
        -TimeoutSec 45
}

$listing = Invoke-OpenListApi -Endpoint 'list' -Body @{
    path = $remoteDirectory
    password = ''
    page = 1
    per_page = [Math]::Max(100, $Count + $StartAt)
    refresh = $false
}
if ($listing.code -ne 200) {
    throw "Unable to list remote directory: $($listing.message)"
}

$files = @($listing.data.content | Where-Object { -not $_.is_dir })
$selected = @($files | Select-Object -Skip ($StartAt - 1) -First $Count)
if ($selected.Count -eq 0) {
    throw "No files selected at index $StartAt"
}

for ($offset = 0; $offset -lt $selected.Count; $offset++) {
    $ordinal = $StartAt + $offset
    $entry = $selected[$offset]
    $name = $entry.name
    $baseName = [IO.Path]::GetFileNameWithoutExtension($name)
    $archive = Join-Path $archiveDirectory $name
    $partial = "$archive.part"
    $staging = Join-Path $extractDirectory $baseName
    $target = Join-Path $gamesDirectory $baseName

    if (Test-Path -LiteralPath $target) {
        Write-Host "[SKIP] $ordinal target exists: $target"
        continue
    }
    if (Test-Path -LiteralPath $staging) {
        throw "Incomplete extraction directory must be inspected first: $staging"
    }

    $attempt = 0
    $expected = [int64]$entry.size
    while ($true) {
        $metadata = $null
        try {
            $metadata = Invoke-OpenListApi -Endpoint 'get' -Body @{
                path = "$remoteDirectory/$name"
                password = ''
            }
        }
        catch {
            Write-Host "[retry] $ordinal metadata request failed: $($_.Exception.Message)"
            Start-Sleep -Seconds 5
            continue
        }
        if ($metadata.code -ne 200) {
            Write-Host "[retry] $ordinal metadata request failed: $($metadata.message)"
            Start-Sleep -Seconds 5
            continue
        }

        $expected = [int64]$metadata.data.size
        if ((Test-Path -LiteralPath $archive) -and (Get-Item -LiteralPath $archive).Length -eq $expected) {
            break
        }
        if (Test-Path -LiteralPath $archive) {
            Move-Item -LiteralPath $archive -Destination $partial -Force
        }

        $current = if (Test-Path -LiteralPath $partial) {
            (Get-Item -LiteralPath $partial).Length
        }
        else {
            0
        }
        if ($current -eq $expected) {
            Move-Item -LiteralPath $partial -Destination $archive -Force
            break
        }
        if ($current -gt $expected) {
            throw "$ordinal partial file is larger than the remote file: $current > $expected"
        }

        $attempt++
        if ($attempt -gt 100) {
            throw "$ordinal exceeded 100 download attempts"
        }
        $percent = if ($expected -gt 0) { [Math]::Round(100 * $current / $expected, 1) } else { 0 }
        Write-Host "[download] $ordinal attempt=$attempt bytes=$current/$expected percent=$percent name=$name"
        & curl.exe `
            -L `
            --fail `
            --continue-at - `
            --connect-timeout 30 `
            --speed-limit 1024 `
            --speed-time 45 `
            --silent `
            --show-error `
            --output $partial `
            $metadata.data.raw_url
        $curlExit = $LASTEXITCODE
        $downloaded = if (Test-Path -LiteralPath $partial) {
            (Get-Item -LiteralPath $partial).Length
        }
        else {
            0
        }
        Write-Host "[attempt] $ordinal exit=$curlExit bytes=$downloaded/$expected"
        if ($downloaded -eq $expected) {
            Move-Item -LiteralPath $partial -Destination $archive -Force
            break
        }
        Start-Sleep -Seconds 3
    }

    Write-Host "[verify] $ordinal bytes=$expected archive=$archive"
    & $sevenZip t "-p$archivePassword" -bso1 -bsp0 -- $archive
    if ($LASTEXITCODE -ne 0) {
        throw "$ordinal archive test failed with exit code $LASTEXITCODE"
    }

    New-Item -ItemType Directory -Path $staging | Out-Null
    & $sevenZip x -y "-p$archivePassword" -bso1 -bsp0 "-o$staging" -- $archive
    if ($LASTEXITCODE -ne 0) {
        throw "$ordinal extraction failed with exit code $LASTEXITCODE"
    }

    $items = @(Get-ChildItem -LiteralPath $staging -Force)
    if ($items.Count -eq 1 -and $items[0].PSIsContainer) {
        $inner = $items[0].FullName
        Get-ChildItem -LiteralPath $inner -Force | Move-Item -Destination $staging
        Remove-Item -LiteralPath $inner
        Write-Host "[normalize] $ordinal flattened single root folder"
    }

    if (Test-Path -LiteralPath $target) {
        Write-Host "[SKIP] $ordinal target appeared during extraction: $target"
        continue
    }
    Move-Item -LiteralPath $staging -Destination $target
    Write-Host "[done] $ordinal target=$target"
}
