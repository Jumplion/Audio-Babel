<#
Runs clang-tidy over the repository C++ sources using the build/ compile_commands.json.
Usage:
  # analyze all cpp files under cpp/src
  .\tools\run-clang-tidy.ps1

  # analyze specific files
  .\tools\run-clang-tidy.ps1 -Files "cpp\\src\\AudioIndex.cpp"

Output is written to tools/clang-tidy-output.txt
#>
Param(
    [Parameter(Mandatory=$false)]
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
    $srcDir = Join-Path $scriptDir '..\cpp\src'
    $Files = Get-ChildItem -Path $srcDir -Recurse -Filter *.cpp | Select-Object -ExpandProperty FullName
}

if (-not $Files -or $Files.Count -eq 0) {
    Write-Error "No .cpp files found to analyze."
    exit 3
}

$outputFile = Join-Path $scriptDir 'clang-tidy-output.txt'
Write-Host "Running clang-tidy on $($Files.Count) files. Output -> $outputFile"

# Invoke clang-tidy; capture stdout+stderr to output file while streaming to console
# Limit clang-tidy header analysis to our repository source folders so external
# headers (for example Boost's cpp_int.hpp) are not linted. The regex below
# matches absolute or relative paths containing cpp/, include/ or tests/.
    # Run clang-tidy but post-filter its textual output so only diagnostics that
    # reference the files provided on the command line are shown. This avoids
    # complex JSON quoting for --line-filter and ensures included headers like
    # Boost are effectively ignored in the reported diagnostics.
    $headerFilter = '^$'

    # Build a lightweight regex matching any of the file basenames or absolute paths
    $filePatterns = $Files | ForEach-Object { [regex]::Escape((Get-Item $_).FullName) }
    $basenamePatterns = $Files | ForEach-Object { [regex]::Escape((Get-Item $_).Name) }
    $allPatterns = ($filePatterns + $basenamePatterns) -join '|'

    Write-Host "Filtering clang-tidy output for: $allPatterns"

    # Run clang-tidy and filter output lines to those matching our files
    $raw = & clang-tidy -p $buildDir -header-filter $headerFilter @Files 2>&1
    $filtered = $raw | Select-String -Pattern $allPatterns
    $filtered | Tee-Object -FilePath $outputFile
    $ec = $LASTEXITCODE
Write-Host "clang-tidy exit code: $ec"
exit $ec
