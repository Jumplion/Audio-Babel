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
& clang-tidy -p $buildDir @Files 2>&1 | Tee-Object -FilePath $outputFile
$ec = $LASTEXITCODE
Write-Host "clang-tidy exit code: $ec"
exit $ec
