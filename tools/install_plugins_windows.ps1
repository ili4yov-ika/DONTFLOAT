<#
.SYNOPSIS
    Обновляет установленные плагины DONTFLOAT свежесобранными (Windows).

.DESCRIPTION
    Копирует CLAP / VST3 / LV2 из каталога сборки в стандартные папки:
        %CommonProgramFiles%\CLAP, \VST3, \LV2
    Qt-рантайм (Qt6*.dll, platforms\, imageformats\ и т.д.) не трогается — его
    кладёт установщик; если папки пустые, сначала пройдите установщиком
    (tools\build_windows_installer.bat) или укажите -DeployQt.

    Запускать из PowerShell **от имени администратора**: Program Files иначе
    недоступен на запись.

.PARAMETER BuildDir
    Каталог сборки CMake (по умолчанию build\Desktop_Qt_6_9_3_MSVC2022_64bit-Release).

.PARAMETER Config
    Конфигурация сборки (Release по умолчанию).

.PARAMETER DeployQt
    Дополнительно прогнать windeployqt рядом с CLAP-плагинами (cmake\DeployPluginQt.cmake).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\install_plugins_windows.ps1
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build\Desktop_Qt_6_9_3_MSVC2022_64bit-Release",
    [string]$Config = "Release",
    [switch]$DeployQt,
    [string]$QtRoot = "C:\Qt\6.9.3\msvc2022_64"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repoRoot $BuildDir
if (-not (Test-Path $build)) { throw "Каталог сборки не найден: $build" }

$clapSrc = Join-Path $build $Config
$vst3Src = Join-Path $build "plugins\vst3"
$lv2Src  = Join-Path $build "plugins\lv2"

$clapDst = Join-Path $env:CommonProgramFiles "CLAP"
$vst3Dst = Join-Path $env:CommonProgramFiles "VST3"
$lv2Dst  = Join-Path $env:CommonProgramFiles "LV2"

function Test-Writable([string]$dir) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $probe = Join-Path $dir ".dontfloat_write_probe"
    try { New-Item -ItemType File $probe -ErrorAction Stop | Out-Null; Remove-Item $probe -Force; $true }
    catch { $false }
}

if (-not (Test-Writable $clapDst)) {
    throw "Нет прав на запись в $clapDst — запустите PowerShell от имени администратора"
}

# CLAP: стаб + impl-модуль каждой редакции лежат плоско в каталоге плагинов
$clapFiles = @(
    "dontfloat.clap", "dontfloat_clap.impl.dll",
    "dontfloat_scratch.clap", "dontfloat_scratch_clap.impl.dll",
    "dontfloat_pitcher.clap", "dontfloat_pitcher_clap.impl.dll"
)
foreach ($name in $clapFiles) {
    $src = Join-Path $clapSrc $name
    if (-not (Test-Path $src)) { Write-Warning "нет $src — пропуск"; continue }
    Copy-Item $src (Join-Path $clapDst $name) -Force
    Write-Host "CLAP  -> $name"
}

# VST3 и LV2 — бандлы-каталоги целиком
foreach ($pair in @(@($vst3Src, $vst3Dst, "VST3"), @($lv2Src, $lv2Dst, "LV2"))) {
    $src, $dst, $tag = $pair
    if (-not (Test-Path $src)) { Write-Warning "нет $src — пропуск"; continue }
    if (-not (Test-Writable $dst)) { throw "Нет прав на запись в $dst" }
    foreach ($bundle in Get-ChildItem $src -Directory) {
        Copy-Item $bundle.FullName $dst -Recurse -Force
        Write-Host "$tag  -> $($bundle.Name)"
    }
}

if ($DeployQt) {
    # Обновляет Qt рядом с CLAP (Qt6Svg.dll обязателен: иконки рисует QSvgRenderer)
    & cmake "-DQT_ROOT=$QtRoot" "-DPLUGIN_DIR=$clapDst" -P (Join-Path $repoRoot "cmake\DeployPluginQt.cmake")
}

Write-Host "Готово. Перезапустите DAW и пересканируйте плагины."
