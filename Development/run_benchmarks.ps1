# Run MIDIQy benchmarks in a manageable way (short runs, minimal output).
#
# Usage:
#   .\run_benchmarks.ps1              # quick subset (default)
#   .\run_benchmarks.ps1 -Full        # all benchmarks, longer runs
#   .\run_benchmarks.ps1 -Filter "ManualNote.*"
#
# Alternatively via CTest (after cmake --build build-debug):
#   cd build-debug && ctest -R MIDIQy_Benchmarks_Quick -V

param(
    [switch]$Quick = $true,
    [switch]$Full,
    [string]$Filter
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

# Find benchmark exe (build or build-debug, Debug or Release)
$dirs = @(
    (Join-Path $rootDir "build\Debug\MIDIQy_Benchmarks.exe"),
    (Join-Path $rootDir "build\Release\MIDIQy_Benchmarks.exe"),
    (Join-Path $rootDir "build-debug\Debug\MIDIQy_Benchmarks.exe"),
    (Join-Path $rootDir "build-debug\Release\MIDIQy_Benchmarks.exe")
)
$exe = $null
foreach ($d in $dirs) {
    if (Test-Path $d) { $exe = $d; break }
}
if (-not $exe) {
    Write-Error "MIDIQy_Benchmarks.exe not found. Build the project first (e.g. cmake --build build --config Debug --target MIDIQy_Benchmarks)."
    exit 1
}

# Build arguments as array so PowerShell does not misinterpret = in --key=value
$argsList = @(
    "--benchmark_min_time=0.1",
    "--benchmark_repetitions=1"
)

if ($Full) {
    $argsList = @(
        "--benchmark_min_time=0.5",
        "--benchmark_repetitions=2"
    )
}

if ($Filter) {
    $argsList += "--benchmark_filter=$Filter"
} elseif ($Quick) {
    # Run a small subset for quick sanity check
    $argsList += "--benchmark_filter=ManualNote_SingleKey|Expression_CC_NoADSR|Zone_SingleNote_Poly|Stress_PolyChords_10Keys"
}

& $exe @argsList
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) { exit $exitCode }
