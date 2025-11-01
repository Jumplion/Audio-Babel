param(
    [string]$BuildDir = "",
    [ValidateSet("unit", "performance", "both", "")]
    [string]$TestMode = ""
)

# Resolve repo root and default build dir
$RepoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot 'build'
}
elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

$FullBuildDir = Resolve-Path -LiteralPath $BuildDir -ErrorAction SilentlyContinue
if (-not $FullBuildDir) {
    Write-Error "Build directory '$BuildDir' does not exist. Run build.ps1 first."
    exit 2
}

Push-Location $FullBuildDir.Path

# If TestMode not specified, prompt user
if ([string]::IsNullOrWhiteSpace($TestMode)) {
    Write-Host "`nSelect tests to run:"
    Write-Host "  1) Unit tests (tests_catch2.exe)"
    Write-Host "  2) Performance benchmarks (performance_benchmarks.exe)"
    Write-Host "  3) Both"
    Write-Host ""
    $choice = Read-Host "Enter choice (1-3)"
    
    switch ($choice) {
        "1" { $TestMode = "unit" }
        "2" { $TestMode = "performance" }
        "3" { $TestMode = "both" }
        default {
            Write-Error "Invalid choice. Please enter 1, 2, or 3."
            Pop-Location
            exit 1
        }
    }
}

$exitCode = 0

# Run unit tests
if ($TestMode -eq "unit" -or $TestMode -eq "both") {
    $unitTestPath = Join-Path $PWD.Path "tests_catch2.exe"
    if (-not (Test-Path -LiteralPath $unitTestPath)) {
        Write-Error "Unit test executable 'tests_catch2.exe' not found in $PWD"
        Pop-Location
        exit 3
    }
    
    Write-Host "`n[run_tests.ps1] Running unit tests: $unitTestPath" -ForegroundColor Cyan
    & $unitTestPath
    if ($LASTEXITCODE -ne 0) {
        $exitCode = $LASTEXITCODE
    }
}

# Run performance benchmarks
if ($TestMode -eq "performance" -or $TestMode -eq "both") {
    $perfTestPath = Join-Path $PWD.Path "performance_benchmarks.exe"
    if (-not (Test-Path -LiteralPath $perfTestPath)) {
        Write-Warning "Performance benchmark executable 'performance_benchmarks.exe' not found in $PWD"
        if ($TestMode -eq "performance") {
            Pop-Location
            exit 3
        }
    }
    else {
        Write-Host "`n[run_tests.ps1] Running performance benchmarks: $perfTestPath" -ForegroundColor Cyan
        & $perfTestPath
        if ($LASTEXITCODE -ne 0 -and $exitCode -eq 0) {
            $exitCode = $LASTEXITCODE
        }
    }
}

Pop-Location
exit $exitCode
