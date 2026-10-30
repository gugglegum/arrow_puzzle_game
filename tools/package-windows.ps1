param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9A-Za-z][0-9A-Za-z.+-]*$')]
    [string]$Version,

    [string]$BuildDirectory = 'build\windows-msvc',
    [string]$OutputDirectory = 'out\release',
    [switch]$Portable
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
$suffix = if ($Portable) { '-portable' } else { '' }
$targetName = if ($Portable) { 'arrow_puzzle_portable.exe' } else { 'arrow_puzzle.exe' }
$executableName = "ArrowPuzzle$suffix.exe"
$executable = Join-Path $buildRoot $targetName
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the Release executable before packaging.'
}
$cache = Get-Content -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt')
if (-not ($cache -match '^CMAKE_BUILD_TYPE:STRING=Release$')) {
    throw 'Release packaging requires a Release build.'
}

$storageMarker = if ($Portable) { 'ARROWPUZZLE_STORAGE=portable' } else { 'ARROWPUZZLE_STORAGE=user' }
$otherMarker = if ($Portable) { 'ARROWPUZZLE_STORAGE=user' } else { 'ARROWPUZZLE_STORAGE=portable' }
$binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($executable))
if (-not $binaryText.Contains($storageMarker) -or $binaryText.Contains($otherMarker)) {
    throw 'Executable storage mode does not match the requested package.'
}
if ($binaryText -match 'ARROWPUZZLE_(SCREENSHOT[A-Z_]*|SEED|DIFFICULTY|RESUME|SAVE_ON_EXIT)' -or
    $binaryText.Contains('screenshot%03i.png') -or $binaryText.Contains('screenrec%03i.gif')) {
    throw 'Developer capture features found in the production executable.'
}
$archiveName = "ArrowPuzzle-$Version-windows-x64$suffix.zip"
$archive = Join-Path $outputRoot $archiveName
foreach ($name in @($archiveName, 'build-info.json', 'SHA256SUMS.txt')) {
    if (Test-Path -LiteralPath (Join-Path $outputRoot $name)) {
        throw "Refusing to overwrite an existing package file: $name; choose a new output directory."
    }
}
$stage = Join-Path $buildRoot ('package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $stage $executableName)
foreach ($filename in @('README.md', 'README_RU.md', 'CHANGELOG.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $filename) -Destination (Join-Path $stage $filename)
}
$fontLicenseDirectory = Join-Path $stage 'assets\fonts'
New-Item -ItemType Directory -Path $fontLicenseDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\fonts\Inter-LICENSE.txt') -Destination $fontLicenseDirectory
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination (Join-Path $stage 'docs') -Recurse
$commit = & git -C $projectRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not identify the source commit.' }
$dirty = @(& git -C $projectRoot status --porcelain --untracked-files=no).Count -ne 0
$labelHeader = Get-Content -LiteralPath (Join-Path $buildRoot 'generated\BuildVersion.hpp') -Raw
$labelMatch = [regex]::Match($labelHeader, 'kVersionLabel\[\] = "([^"]+)"')
if (-not $labelMatch.Success) { throw 'Could not read the executable build version.' }
$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $stage $executableName)).Hash.ToLowerInvariant()
$runUrl = if ($env:GITHUB_ACTIONS -eq 'true') {
    "$($env:GITHUB_SERVER_URL)/$($env:GITHUB_REPOSITORY)/actions/runs/$($env:GITHUB_RUN_ID)"
} else { $null }
$metadata = [ordered]@{
    package_version = $Version
    portable = [bool]$Portable
    executable_name = $executableName
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
    "$exeHash  $executableName`n$zipHash  $archiveName`n", $utf8)

if ($env:GITHUB_OUTPUT) {
    "archive_name=$archiveName" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "archive_path=$archive" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "exe_path=$(Join-Path $stage $executableName)" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
}
Write-Output "Packaged $archiveName from $($metadata.source_commit)"
