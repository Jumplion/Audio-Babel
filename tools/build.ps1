param(
    [string]$Configuration = "Release",
    [string]$Generator = "MinGW Makefiles",
    [string]$BuildDir = ""
)

# Determine repository root (one level up from the tools folder where this script lives)
$RepoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')

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

$FullBuildDir = Resolve-Path -LiteralPath $BuildDir
Push-Location $FullBuildDir.Path

Write-Host "[build.ps1] Configuring with generator '$Generator' and configuration '$Configuration' in $PWD"
# Use the repository root as the CMake source directory so CMake finds CMakeLists.txt reliably
cmake -G "$Generator" -DCMAKE_BUILD_TYPE=$Configuration $RepoRoot
if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }

if ($Generator -match "Makefiles") {
    $jobs = [Environment]::ProcessorCount
    Write-Host "[build.ps1] Building with mingw32-make -j $jobs"
    mingw32-make -j $jobs
}
else {
    Write-Host "[build.ps1] Building with 'cmake --build'"
    cmake --build . --config $Configuration
}

$exit = $LASTEXITCODE
Pop-Location
exit $exit
