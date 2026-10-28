param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9A-Za-z][0-9A-Za-z.+-]*$')]
    [string]$Version,

    [string]$BuildDirectory = 'build\windows-msvc',
    [string]$OutputDirectory = 'out\release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
$executable = Join-Path $buildRoot 'arrow_puzzle.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the Release executable before packaging.'
}
$cache = Get-Content -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt')
if (-not ($cache -match '^CMAKE_BUILD_TYPE:STRING=Release$')) {
    throw 'Release packaging requires a Release build.'
}

$archiveName = "ArrowPuzzle-$Version-windows-x64.zip"
$archive = Join-Path $outputRoot $archiveName
if (Test-Path -LiteralPath $archive) {
    throw "Refusing to overwrite an existing package: $archive"
}
$stage = Join-Path $buildRoot ('package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $stage 'ArrowPuzzle.exe')
foreach ($filename in @('README.md', 'README_RU.md', 'CHANGELOG.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $filename) -Destination (Join-Path $stage $filename)
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\fonts\Inter-LICENSE.txt') -Destination $stage
$commit = & git -C $projectRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not identify the source commit.' }
$dirty = @(& git -C $projectRoot status --porcelain --untracked-files=no).Count -ne 0
$labelHeader = Get-Content -LiteralPath (Join-Path $buildRoot 'generated\BuildVersion.hpp') -Raw
$labelMatch = [regex]::Match($labelHeader, 'kVersionLabel\[\] = "([^"]+)"')
if (-not $labelMatch.Success) { throw 'Could not read the executable build version.' }
$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $stage 'ArrowPuzzle.exe')).Hash.ToLowerInvariant()
$runUrl = if ($env:GITHUB_ACTIONS -eq 'true') {
    "$($env:GITHUB_SERVER_URL)/$($env:GITHUB_REPOSITORY)/actions/runs/$($env:GITHUB_RUN_ID)"
} else { $null }
$metadata = [ordered]@{
    package_version = $Version
    executable_version = $labelMatch.Groups[1].Value
    source_commit = $commit.Trim()
    source_ref = $env:GITHUB_REF
    source_dirty = $dirty
    build_utc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    workflow_run = $runUrl
    executable_sha256 = $exeHash
}
$utf8 = New-Object System.Text.UTF8Encoding($false)
$json = ($metadata | ConvertTo-Json) + "`n"
[System.IO.File]::WriteAllText((Join-Path $stage 'build-info.json'), $json, $utf8)
[System.IO.File]::WriteAllText((Join-Path $outputRoot 'build-info.json'), $json, $utf8)
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
$zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText((Join-Path $outputRoot 'SHA256SUMS.txt'),
    "$exeHash  ArrowPuzzle.exe`n$zipHash  $archiveName`n", $utf8)

if ($env:GITHUB_OUTPUT) {
    "archive_name=$archiveName" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "archive_path=$archive" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "exe_path=$(Join-Path $stage 'ArrowPuzzle.exe')" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
}
Write-Output "Packaged $archiveName from $($metadata.source_commit)"
