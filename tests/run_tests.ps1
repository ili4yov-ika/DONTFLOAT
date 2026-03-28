# PowerShell script for running DONTFLOAT tests
# Usage: .\run_tests.ps1 [test_name]

$ErrorActionPreference = "Stop"

# Корень build (может быть несколько пресетов: build/Desktop_Qt_*_64bit-Debug)
$buildRoot = Join-Path $PSScriptRoot "..\build"
if (-not (Test-Path $buildRoot)) {
    Write-Host "Error: build directory not found at $buildRoot" -ForegroundColor Red
    Write-Host "Configure and build the project first (CMake / Qt Creator / Cursor)." -ForegroundColor Yellow
    exit 1
}

# Директория CMake-сборки для ctest (предпочтительно пресет Qt MSVC)
$ctestBuildDir = $null
$presetCandidates = @(
    (Join-Path $buildRoot "Desktop_Qt_6_9_3_MSVC2022_64bit-Debug"),
    (Join-Path $buildRoot "Desktop_Qt_6_8_3_MSVC2022_64bit-Debug")
)
foreach ($c in $presetCandidates) {
    if (Test-Path (Join-Path $c "CTestTestfile.cmake")) {
        $ctestBuildDir = $c
        break
    }
}
if (-not $ctestBuildDir) {
    $found = Get-ChildItem -Path $buildRoot -Directory -Filter "Desktop_Qt_*" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "CTestTestfile.cmake") } |
        Select-Object -First 1
    if ($found) { $ctestBuildDir = $found.FullName }
}
if (-not $ctestBuildDir -and (Test-Path (Join-Path $buildRoot "CTestTestfile.cmake"))) {
    $ctestBuildDir = (Resolve-Path $buildRoot).Path
}

# Папка с *_test.exe (Visual Studio: .../Debug/*.exe)
$testDir = $null
if ($ctestBuildDir) {
    $d = Join-Path $ctestBuildDir "Debug"
    if (Test-Path $d) { $testDir = $d }
}
if (-not $testDir -and (Test-Path (Join-Path $buildRoot "Debug"))) {
    $testDir = Join-Path $buildRoot "Debug"
}
if (-not $testDir) {
    $qtDirs = Get-ChildItem -Path $buildRoot -Directory -Filter "Desktop_Qt_*" -ErrorAction SilentlyContinue
    foreach ($d in $qtDirs) {
        $dbg = Join-Path $d.FullName "Debug"
        if (Test-Path $dbg) {
            $testDir = $dbg
            break
        }
    }
}
if (-not $testDir) {
    Write-Host "Error: Debug directory with test executables not found under $buildRoot" -ForegroundColor Red
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
        $qtKitRoot = Split-Path $qtPath -Parent
        $env:QT_PLUGIN_PATH = Join-Path $qtKitRoot "plugins"
        break
    }
}

# If specific test is specified, run it
if ($args.Count -gt 0) {
    $testName = $args[0]
    $testExe = Join-Path $testDir "$testName.exe"
    
    if (Test-Path $testExe) {
        Write-Host "Running test: $testName" -ForegroundColor Green
        Write-Host ""
        & $testExe
        exit $LASTEXITCODE
    }
    else {
        Write-Host "Error: Test $testName.exe not found in $testDir" -ForegroundColor Red
        Write-Host "Available tests:" -ForegroundColor Yellow
        Get-ChildItem -Path $testDir -Filter "*_test.exe" | ForEach-Object { Write-Host "  $($_.Name)" }
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
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
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

# Run all tests via CTest (из каталога CMake build, где лежит CTestTestfile.cmake)
Write-Host "Running all tests via CTest..." -ForegroundColor Green
if ($ctestBuildDir) {
    Write-Host "CTest build dir: $ctestBuildDir" -ForegroundColor Gray
}
Write-Host ""

if ($ctestBuildDir) {
    & $ctestPath --test-dir $ctestBuildDir -C Debug --output-on-failure
} else {
    Set-Location $buildRoot
    & $ctestPath -C Debug --output-on-failure
}

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

