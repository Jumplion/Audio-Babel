param(
    [int]$Port = 3000,
    [switch]$Install
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

# Prefer server package.json in web/server
$serverPkg = Join-Path $RepoRoot 'web\server\package.json'

if (Test-Path $serverPkg) {
    Push-Location (Split-Path $serverPkg)
    if ($Install) {
        Write-Host "Running npm install in $(Get-Location)"
        npm install
        if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }
    }

    # Prefer 'start' script
    $pkg = Get-Content $serverPkg -Raw | ConvertFrom-Json
    if ($pkg.scripts -and $pkg.scripts.start) {
        Write-Host "Starting server via 'npm start' in $(Get-Location)"
            # Open browser and then run npm start inline so logs are visible in this terminal
        Open-Browser "http://localhost:$Port"
        # Use cmd.exe /c so the npm.cmd shim is executed correctly on Windows
        & cmd.exe /c "npm start"
            $exit = $LASTEXITCODE
            Pop-Location
            exit $exit
    }

    Pop-Location
}

# Fallback: serve web/frontend/public via python or npx
$publicDir = Join-Path $RepoRoot 'web\frontend\public'
if (-not (Test-Path $publicDir)) { Write-Error "No server package.json and no public folder found"; exit 2 }

Write-Host "No server start script found; serving static folder: $publicDir"

# Prefer python (PowerShell 5 compatible)
$pyCmd = Get-Command python3 -ErrorAction SilentlyContinue
if (-not $pyCmd) { $pyCmd = Get-Command python -ErrorAction SilentlyContinue }
if ($pyCmd) {
    Start-Process -NoNewWindow -FilePath $pyCmd.Path -ArgumentList '-m', 'http.server', "$Port", '--directory', $publicDir -WorkingDirectory $publicDir -PassThru | Out-Null
    Start-Sleep -Seconds 1
    Open-Browser "http://localhost:$Port"
    exit 0
}

# Fallback to npx http-server
$npxCmd = Get-Command npx -ErrorAction SilentlyContinue
if ($npxCmd) {
    Start-Process -NoNewWindow -FilePath $npxCmd.Path -ArgumentList 'http-server', $publicDir, '-p', $Port -WorkingDirectory $publicDir -PassThru | Out-Null
    Start-Sleep -Seconds 1
    Open-Browser "http://localhost:$Port"
    exit 0
}

Write-Error "Unable to start a server: install Node (npx) or Python 3."
exit 3
