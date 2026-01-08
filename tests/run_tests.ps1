# PowerShell script for running DONTFLOAT tests
# Usage: .\run_tests.ps1 [test_name]

$ErrorActionPreference = "Stop"

# Go to build directory
$buildDir = Join-Path $PSScriptRoot "..\build"
Set-Location $buildDir

if (-not (Test-Path "Debug")) {
    Write-Host "Error: build\Debug directory not found!" -ForegroundColor Red
    Write-Host "Build the project first: cmake --build build" -ForegroundColor Yellow
    exit 1
}

# Add Qt DLL path to PATH
$qtPaths = @(
    "C:\Qt\6.9.3\msvc2022_64\bin",
    "C:\Qt\6.8.3\msvc2022_64\bin"
)

foreach ($qtPath in $qtPaths) {
    if (Test-Path $qtPath) {
        $env:PATH = "$qtPath;$env:PATH"
        break
    }
}

# If specific test is specified, run it
if ($args.Count -gt 0) {
    $testName = $args[0]
    $testExe = Join-Path "Debug" "$testName.exe"
    
    if (Test-Path $testExe) {
        Write-Host "Running test: $testName" -ForegroundColor Green
        Write-Host ""
        & $testExe
        exit $LASTEXITCODE
    }
    else {
        Write-Host "Error: Test $testName.exe not found in build\Debug\" -ForegroundColor Red
        Write-Host "Available tests:" -ForegroundColor Yellow
        Get-ChildItem -Path "Debug" -Filter "*_test.exe" | ForEach-Object { Write-Host "  $($_.Name)" }
        exit 1
    }
}

# Find ctest.exe
$ctestPath = $null

# Try to find via Get-Command (most reliable way)
try {
    $ctestPath = (Get-Command ctest -ErrorAction Stop).Source
}
catch {
    # If not found in PATH, check standard paths
    $ctestPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
        "C:\Program Files\CMake\bin\ctest.exe"
    )
    
    foreach ($path in $ctestPaths) {
        if (Test-Path $path) {
            $ctestPath = $path
            break
        }
    }
}

if (-not $ctestPath) {
    Write-Host "Error: ctest.exe not found!" -ForegroundColor Red
    Write-Host "Install CMake or add it to PATH" -ForegroundColor Yellow
    exit 1
}

# Run all tests via CTest
Write-Host "Running all tests via CTest..." -ForegroundColor Green
Write-Host ""

& $ctestPath -C Debug --output-on-failure

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "All tests passed successfully!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
}
else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "Some tests failed!" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    exit $LASTEXITCODE
}

