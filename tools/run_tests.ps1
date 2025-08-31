param(
    [string]$BuildDir = "",
    [string]$TestExe = "tests_runner.exe"
)

# Resolve repo root and default build dir
$RepoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot 'build'
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

$FullBuildDir = Resolve-Path -LiteralPath $BuildDir -ErrorAction SilentlyContinue
if (-not $FullBuildDir) {
    Write-Error "Build directory '$BuildDir' does not exist. Run build.ps1 first."
    exit 2
}

Push-Location $FullBuildDir.Path

$testPath = Join-Path $PWD.Path $TestExe
if (-not (Test-Path -LiteralPath $testPath)) {
    Write-Error "Test executable '$TestExe' not found in $PWD"
    Pop-Location
    exit 3
}

Write-Host "[run_tests.ps1] Running tests: $testPath"
& $testPath
$exit = $LASTEXITCODE

Pop-Location
exit $exit
