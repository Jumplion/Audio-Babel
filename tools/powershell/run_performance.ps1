#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Run performance benchmarks for the Audio Babel library.

.DESCRIPTION
    Builds the performance benchmark executable in Release mode and runs it.
    Results are written to build/performance_results.txt.

.PARAMETER Clean
    Clean the build directory before building.

.PARAMETER Compare
    After running, compare results against cpp/perf/baseline.json using
    tools/node/compare-benchmarks.mjs (best-effort; skipped if node isn't on PATH).

.EXAMPLE
    .\run_performance.ps1
    .\run_performance.ps1 -Clean
    .\run_performance.ps1 -Compare
#>

param(
    [switch]$Clean,
    [switch]$Compare
)

$ErrorActionPreference = "Stop"

# Get the repository root (parent of tools/powershell/)
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $RepoRoot

try {
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host "AUDIO BABEL PERFORMANCE BENCHMARKS" -ForegroundColor Cyan
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host ""

    # Clean if requested
    if ($Clean) {
        Write-Host "Cleaning build directory..." -ForegroundColor Yellow
        if (Test-Path "build") {
            Remove-Item -Path "build" -Recurse -Force
        }
    }

    # Create build directory
    if (-not (Test-Path "build")) {
        Write-Host "Creating build directory..." -ForegroundColor Yellow
        New-Item -ItemType Directory -Path "build" | Out-Null
    }

    # Configure, build, and run from inside build/ so the benchmark binary's
    # relative output paths (performance_results.txt/.json) land at
    # build/performance_results.* as documented, matching run_performance.sh.
    Push-Location "build"
    try {
        Write-Host "Configuring CMake (Release mode)..." -ForegroundColor Yellow
        cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }

        Write-Host "`nBuilding performance benchmarks..." -ForegroundColor Yellow
        mingw32-make performance_benchmarks -j 4
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }

        # Check if executable exists
        $ExeName = "performance_benchmarks.exe"
        if (-not (Test-Path $ExeName)) {
            throw "Performance benchmarks executable not found at: build/$ExeName"
        }

        # Run benchmarks
        Write-Host "`n==================================================" -ForegroundColor Green
        Write-Host "Running benchmarks (this may take a few minutes)..." -ForegroundColor Green
        Write-Host "==================================================" -ForegroundColor Green
        Write-Host ""

        & ".\$ExeName"

        if ($LASTEXITCODE -ne 0) {
            throw "Benchmarks failed with exit code: $LASTEXITCODE"
        }

        # Display results
        Write-Host "`n==================================================" -ForegroundColor Cyan
        Write-Host "BENCHMARK RESULTS" -ForegroundColor Cyan
        Write-Host "==================================================" -ForegroundColor Cyan
        Write-Host ""

        $ResultsName = "performance_results.txt"
        if (Test-Path $ResultsName) {
            Get-Content $ResultsName | Write-Host
            Write-Host "`nResults saved to: build/$ResultsName" -ForegroundColor Green
        } else {
            Write-Warning "Results file not found at: build/$ResultsName"
        }
    } finally {
        Pop-Location
    }

    Write-Host "`n==================================================" -ForegroundColor Green
    Write-Host "Performance benchmarks completed successfully!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Green

    # Optionally compare against the committed baseline
    if ($Compare) {
        Write-Host "`n==================================================" -ForegroundColor Cyan
        Write-Host "COMPARING AGAINST BASELINE" -ForegroundColor Cyan
        Write-Host "==================================================" -ForegroundColor Cyan
        Write-Host ""

        if (Get-Command node -ErrorAction SilentlyContinue) {
            node tools/node/compare-benchmarks.mjs
        } else {
            Write-Host "NOTE: node not found on PATH; skipping baseline comparison." -ForegroundColor Yellow
            Write-Host "Install Node.js and re-run with -Compare to compare against cpp/perf/baseline.json." -ForegroundColor Yellow
        }
    }

} catch {
    Write-Host "`n==================================================" -ForegroundColor Red
    Write-Host "ERROR: $_" -ForegroundColor Red
    Write-Host "==================================================" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
