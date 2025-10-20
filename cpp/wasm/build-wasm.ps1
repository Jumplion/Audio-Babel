# Build script for WebAssembly module
# Run this with Emscripten environment activated

param(
    [switch]$Debug,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

Write-Host "=== Building Audio Index WASM Module ===" -ForegroundColor Cyan

# Check if Emscripten is available
try {
    $emccVersion = & emcc --version 2>&1
    Write-Host "✓ Emscripten found" -ForegroundColor Green
}
catch {
    Write-Host "❌ Emscripten not found!" -ForegroundColor Red
    Write-Host "Please install Emscripten:" -ForegroundColor Yellow
    Write-Host "  git clone https://github.com/emscripten-core/emsdk.git" -ForegroundColor Yellow
    Write-Host "  cd emsdk" -ForegroundColor Yellow
    Write-Host "  .\emsdk install latest" -ForegroundColor Yellow
    Write-Host "  .\emsdk activate latest" -ForegroundColor Yellow
    Write-Host "  .\emsdk_env.ps1" -ForegroundColor Yellow
    exit 1
}

# Directories
$wasmDir = $PSScriptRoot
$buildDir = Join-Path $wasmDir "build"
$outputDir = Join-Path $PSScriptRoot "..\..\audio-babel-record-store\public\wasm"

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir
    }
}

# Create directories
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}
if (!(Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

Write-Host "`nConfiguring CMake..." -ForegroundColor Cyan
Set-Location $buildDir

# Configure with Emscripten
$configType = if ($Debug) { "Debug" } else { "Release" }
& emcmake cmake .. -DCMAKE_BUILD_TYPE=$configType

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`nBuilding WASM module..." -ForegroundColor Cyan
& emmake make -j4

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`nCopying files to output directory..." -ForegroundColor Cyan

# Copy WASM and JS files
$wasmFile = Join-Path $buildDir "audio-index.wasm"
$jsFile = Join-Path $buildDir "audio-index.js"

if (Test-Path $wasmFile) {
    Copy-Item $wasmFile $outputDir -Force
    Write-Host "✓ Copied audio-index.wasm" -ForegroundColor Green
}
else {
    Write-Host "❌ WASM file not found!" -ForegroundColor Red
}

if (Test-Path $jsFile) {
    Copy-Item $jsFile $outputDir -Force
    Write-Host "✓ Copied audio-index.js" -ForegroundColor Green
}
else {
    Write-Host "❌ JS file not found!" -ForegroundColor Red
}

# Show file sizes
if (Test-Path $wasmFile) {
    $wasmSize = (Get-Item $wasmFile).Length / 1KB
    Write-Host "`nWASM module size: $($wasmSize.ToString('0.00')) KB" -ForegroundColor Cyan
}

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "WASM module available at: $outputDir" -ForegroundColor Green

Set-Location $PSScriptRoot
