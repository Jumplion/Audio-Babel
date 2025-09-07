<#
Runs clang-tidy over the repository C++ sources using the build/ compile_commands.json.
Usage:
    # analyze all cpp files under cpp/src
    .\tools\run-clang-tidy.ps1

    # analyze specific files
    .\tools\run-clang-tidy.ps1 -Files "cpp\\src\\AudioIndex.cpp"

    # apply compile error fixes
    .\tools\run-clang-tidy.ps1 -FixErrors

Output is written to tools/clang-tidy-output.txt
#>
Param(
    [Parameter(Mandatory = $false)]
    [string[]]$Files
)

$scriptDir = Split-Path -Parent $PSCommandPath
$buildDir = Join-Path $scriptDir '..\build'
$compileCommands = Join-Path $buildDir 'compile_commands.json'
if (-not (Test-Path $compileCommands)) {
    Write-Error "compile_commands.json not found in $buildDir. Run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    exit 2
}

if (-not $Files) {
    # Collect common C/C++ source and header extensions from the repository root
    $root = Join-Path $scriptDir '..\cpp'
    $extensions = @('.cpp', '.cxx', '.cc', '.c', '.h', '.hpp', '.hxx')
    $Files = Get-ChildItem -Path $root -Recurse -File | Where-Object {
        $ext = $_.Extension.ToLower()
        ($extensions -contains $ext) -and ($_ -ne $null) -and ($_.FullName -notlike "*\\build\\*") -and ($_.FullName -notlike "*.git\\*") -and ($_.FullName -notlike "*\\node_modules\\*") -and ($_.FullName -notlike "*\\third_party\\*")
    } | Select-Object -ExpandProperty FullName
}
else {
    # Normalize provided paths to full paths
    $Files = $Files | ForEach-Object {
        try { (Resolve-Path -Path $_).Path } catch { $_ }
    }
}

if (-not $Files -or $Files.Count -eq 0) {
    Write-Error "No C/C++ source or header files found to analyze."
    exit 3
}

$outputFile = Join-Path $scriptDir 'clang-tidy-output.txt'
Remove-Item -Path $outputFile -ErrorAction SilentlyContinue
Write-Host "Found $($Files.Count) files. Output -> $outputFile"

# Ensure required tools are available
$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangTidy) {
    Write-Error "clang-tidy not found in PATH. Install LLVM/Clang or add clang-tidy to PATH."
    exit 4
}
if (-not $clangFormat) {
    Write-Warning "clang-format not found in PATH. Formatting step will be skipped."
}

# Run clang-tidy with --fix per-file to avoid command-line length issues
Write-Host "Running clang-tidy --fix on each file (this may be slow)..."
$tidyFailures = @()
# Limit header diagnostics to our source folders to avoid overwhelming external headers
$headerFilter = '.*(\\cpp\\|\\include\\|\\tests\\).*'
foreach ($f in $Files) {
    Write-Host "--- clang-tidy: $f"
    $out = & clang-tidy -p $buildDir --fix --format-style=file --header-filter="$headerFilter" $f 2>&1
    if ($out) { $out | Tee-Object -FilePath $outputFile -Append }
    # clang-tidy returns 1 when diagnostics were found (and fixes may have been applied).
    # Treat exit codes >= 2 as failures (tool errors); 1 is non-fatal here.
    if ($LASTEXITCODE -ge 2) { $tidyFailures += @{ File = $f; Exit = $LASTEXITCODE } }
}

# Run clang-format to apply style fixes (if available)
if ($clangFormat) {
    Write-Host "Running clang-format -i --style=file on files..."
    foreach ($f in $Files) {
        & clang-format -i --style=file $f
    }
}
else {
    Write-Host "Skipping clang-format because it's unavailable."
}

# Summary
if ($tidyFailures.Count -gt 0) {
    Write-Host "clang-tidy reported non-zero exit codes for $($tidyFailures.Count) file(s). See $outputFile for details." -ForegroundColor Yellow
    foreach ($t in $tidyFailures) { Write-Host "$($t.File) -> exit $($t.Exit)" }
    exit 5
}
else {
    Write-Host "clang-tidy finished with no non-zero per-file exit codes. Output saved to $outputFile" -ForegroundColor Green
    exit 0
}
