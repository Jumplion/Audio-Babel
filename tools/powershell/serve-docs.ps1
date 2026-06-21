<#
.SYNOPSIS
    Serve the docs/ static site (the GitHub Pages app) locally.

.PARAMETER Port
    Port to serve on (default: 3000).

.EXAMPLE
    .\serve-docs.ps1
    .\serve-docs.ps1 -Port 8080
#>
param(
    [int]$Port = 3000
)

function Open-Browser($url) {
    if (Get-Command start -ErrorAction SilentlyContinue) {
        Start-Process $url
    } else {
        Write-Host "Open $url in your browser"
    }
}

# Resolve repo root
$RepoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')
$DocsDir = Join-Path $RepoRoot 'docs'

if (-not (Test-Path $DocsDir)) {
    Write-Error "docs/ folder not found at $DocsDir"
    exit 2
}

Write-Host "Serving static site from: $DocsDir"

# Prefer python (PowerShell 5 compatible)
$pyCmd = Get-Command python3 -ErrorAction SilentlyContinue
if (-not $pyCmd) { $pyCmd = Get-Command python -ErrorAction SilentlyContinue }
if ($pyCmd) {
    Open-Browser "http://localhost:$Port"
    & $pyCmd.Path -m http.server $Port --directory $DocsDir
    exit $LASTEXITCODE
}

# Fallback to npx http-server
$npxCmd = Get-Command npx -ErrorAction SilentlyContinue
if ($npxCmd) {
    Open-Browser "http://localhost:$Port"
    & $npxCmd.Path http-server $DocsDir -p $Port
    exit $LASTEXITCODE
}

Write-Error "Unable to start a server: install Python 3 or Node (npx)."
exit 3
