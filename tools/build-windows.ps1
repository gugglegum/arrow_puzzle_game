param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateRange(0, 1000000)]
    [int]$SeedCount = 0,

    [uint32]$FirstSeed = 0
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio Installer (vswhere.exe) was not found."
}

$visualStudio = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $visualStudio) {
    throw "Visual Studio with the MSVC x64 tools was not found."
}

$devShell = Join-Path $visualStudio "Common7\Tools\Launch-VsDevShell.ps1"
. $devShell -Arch amd64 -SkipAutomaticLocation

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) {
    $cmakeCommand.Source
} else {
    "C:\Program Files\JetBrains\CLion\bin\cmake\win\x64\bin\cmake.exe"
}

$ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
$ninja = if ($ninjaCommand) {
    $ninjaCommand.Source
} else {
    "C:\Program Files\JetBrains\CLion\bin\ninja\win\x64\ninja.exe"
}

if (-not (Test-Path -LiteralPath $cmake)) {
    throw "CMake was not found in PATH or in the standard CLion directory."
}
if (-not (Test-Path -LiteralPath $ninja)) {
    throw "Ninja was not found in PATH or in the standard CLion directory."
}

$buildName = if ($Configuration -eq "Debug") {
    "windows-msvc-debug"
} else {
    "windows-msvc"
}
$buildDirectory = Join-Path $projectRoot "build\$buildName"

& $cmake `
    -S $projectRoot `
    -B $buildDirectory `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_CXX_COMPILER=cl" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DARROWPUZZLE_BUILD_TESTS=ON" `
    "-DARROWPUZZLE_STATIC_RUNTIME=ON"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $cmake --build $buildDirectory
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($SeedCount -gt 0) {
    $testExecutable = Join-Path $buildDirectory "arrow_tests.exe"
    & $testExecutable $SeedCount $FirstSeed
} else {
    $ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
    & $ctest --test-dir $buildDirectory --output-on-failure
}
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Ready: $buildDirectory\arrow_puzzle.exe"
Write-Host "Ready: $buildDirectory\arrow_puzzle_portable.exe"
