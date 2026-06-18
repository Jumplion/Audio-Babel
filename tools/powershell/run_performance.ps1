#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Run performance benchmarks for the Audio Babel library.

.DESCRIPTION
    Builds the performance benchmark executable in Release mode and runs it.
    Results are written to build/performance_results.txt.

.PARAMETER Clean
    Clean the build directory before building.

.EXAMPLE
    .\run_performance.ps1
    .\run_performance.ps1 -Clean
#>

param(
    [switch]$Clean
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

    # Configure with Release mode
    Write-Host "Configuring CMake (Release mode)..." -ForegroundColor Yellow
    Push-Location "build"
    try {
        cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }
    } finally {
        Pop-Location
    }

    # Build
    Write-Host "`nBuilding performance benchmarks..." -ForegroundColor Yellow
    Push-Location "build"
    try {
        mingw32-make performance_benchmarks -j 4
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
    } finally {
        Pop-Location
    }

    # Check if executable exists
    $ExePath = Join-Path "build" "performance_benchmarks.exe"
    if (-not (Test-Path $ExePath)) {
        throw "Performance benchmarks executable not found at: $ExePath"
    }

    # Run benchmarks
    Write-Host "`n==================================================" -ForegroundColor Green
    Write-Host "Running benchmarks (this may take a few minutes)..." -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Green
    Write-Host ""

    & $ExePath

    if ($LASTEXITCODE -ne 0) {
        throw "Benchmarks failed with exit code: $LASTEXITCODE"
    }

    # Display results
    Write-Host "`n==================================================" -ForegroundColor Cyan
    Write-Host "BENCHMARK RESULTS" -ForegroundColor Cyan
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host ""

    $ResultsPath = Join-Path "build" "performance_results.txt"
    if (Test-Path $ResultsPath) {
        Get-Content $ResultsPath | Write-Host
        Write-Host "`nResults saved to: $ResultsPath" -ForegroundColor Green
    } else {
        Write-Warning "Results file not found at: $ResultsPath"
    }

    Write-Host "`n==================================================" -ForegroundColor Green
    Write-Host "Performance benchmarks completed successfully!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Green

} catch {
    Write-Host "`n==================================================" -ForegroundColor Red
    Write-Host "ERROR: $_" -ForegroundColor Red
    Write-Host "==================================================" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
