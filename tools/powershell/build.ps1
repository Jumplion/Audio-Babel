<#
.SYNOPSIS
    Configure and build the project with CMake.

.PARAMETER Configuration
    Debug | Release | RelWithDebInfo | MinSizeRel   (default: Release)

.PARAMETER Generator
    CMake generator string, e.g.:
      "MinGW Makefiles"  (default — native Windows with MinGW)
      "Unix Makefiles"   (native Linux/WSL/macOS)
      "Ninja"
      "Visual Studio 17 2022"

.PARAMETER BuildDir
    Path to the build directory. Relative paths are resolved against the
    repository root. Defaults to "<repo root>/build".

.EXAMPLE
    .\build.ps1
.EXAMPLE
    .\build.ps1 -Configuration Debug
.EXAMPLE
    .\build.ps1 -Configuration Debug -Generator "Ninja" -BuildDir build-ninja
#>
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Generator = "MinGW Makefiles",
    [string]$BuildDir = ""
)

# Determine repository root (one level up from the tools folder where this script lives)
$RepoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')

# If BuildDir is empty, default to <repoRoot>/build. If relative, interpret relative to repo root.
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot 'build'
}
elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

# Ensure build directory exists
if (-not (Test-Path -LiteralPath $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

$MinGWBin = "C:\msys64\mingw64\bin"
if ($env:PATH -notlike "*$MinGWBin*") {
    $env:PATH = "$MinGWBin;$env:PATH"
    Write-Host "[build.ps1] Prepended $MinGWBin to PATH"
}

$FullBuildDir = Resolve-Path -LiteralPath $BuildDir
Push-Location $FullBuildDir.Path

Write-Host "[build.ps1] Configuring with generator '$Generator' and configuration '$Configuration' in $PWD"
# Use the repository root as the CMake source directory so CMake finds CMakeLists.txt reliably
cmake -G "$Generator" -DCMAKE_BUILD_TYPE=$Configuration -DCMAKE_CXX_COMPILER="$MinGWBin\g++.exe" -DCMAKE_C_COMPILER="$MinGWBin\gcc.exe" -DCMAKE_MAKE_PROGRAM="$MinGWBin\mingw32-make.exe" $RepoRoot
if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }

if ($Generator -match "Makefiles") {
    $jobs = [Environment]::ProcessorCount
    Write-Host "[build.ps1] Building with mingw32-make -j $jobs"
    & "$MinGWBin\mingw32-make.exe" -j $jobs
}
else {
    Write-Host "[build.ps1] Building with 'cmake --build'"
    cmake --build . --config $Configuration
}

$exit = $LASTEXITCODE
Pop-Location
exit $exit
