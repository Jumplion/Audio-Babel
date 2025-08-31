param(
    [string]$BuildDir = "",
    [switch]$RemoveDir
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
    Write-Host "No build directory found at '$BuildDir'. Nothing to clean."
    exit 0
}

if ($RemoveDir) {
    Write-Host "[clean.ps1] Removing directory: $($FullBuildDir.Path)"
    Remove-Item -LiteralPath $FullBuildDir.Path -Recurse -Force
    exit $LASTEXITCODE
}

Write-Host "[clean.ps1] Removing common build artifacts inside $($FullBuildDir.Path)"
Get-ChildItem -LiteralPath $FullBuildDir.Path -Force | Where-Object {
    $_.Name -notin ('.git', '.gitignore')
} | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Clean complete."
exit 0
