param(
    [string]$BuildDir = "",
    [string]$ExampleExe = "example_main.exe"
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

$exePath = Join-Path $PWD.Path $ExampleExe
if (-not (Test-Path -LiteralPath $exePath)) {
    Write-Error "Example executable '$ExampleExe' not found in $PWD"
    Pop-Location
    exit 3
}

Write-Host "[run_example.ps1] Running example: $exePath"
& $exePath
$exit = $LASTEXITCODE

Pop-Location
exit $exit
