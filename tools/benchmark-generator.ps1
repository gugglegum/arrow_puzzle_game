param(
    [string]$Executable = "$PSScriptRoot\..\build\windows-msvc\arrow_generator_benchmark.exe",
    [uint32[]]$Seeds = @(0, 1, 2, 64, 747, 23000),
    [int]$Size = 160,
    [switch]$NoLoops,
    [int]$Candidates = 8,
    [string]$OutputCsv = "$PSScriptRoot\..\build\generator-benchmark.csv"
)

$ErrorActionPreference = "Stop"
$loops = if ($NoLoops) { 0 } else { 1 }
$results = foreach ($seed in $Seeds) {
    # Each seed gets a fresh process, so peak memory is not inherited from a
    # previous generation or from the independent solution verification.
    $lines = & $Executable $seed $Size $loops $Candidates
    if ($LASTEXITCODE -ne 0) {
        throw "Generator benchmark failed for seed $seed (exit $LASTEXITCODE)."
    }
    $lines | ConvertFrom-Csv
}
$results | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding utf8
$results | Format-Table seed, seconds, peak_MiB, arrows, initial, short_border_cells, verified
Write-Host "Saved: $OutputCsv"
