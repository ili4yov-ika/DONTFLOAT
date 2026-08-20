<#
.SYNOPSIS
    Обновляет установленные плагины DONTFLOAT (Windows): CLAP, VST3, LV2.

.DESCRIPTION
    Ставит в системные папки готовый payload установщика — тот же набор файлов,
    который кладёт DONTFLOAT_Setup.exe, вместе с уже развёрнутым и проверенным
    Qt:
        build\install\lib\clap  -> %CommonProgramFiles%\CLAP
        build\install\lib\vst3  -> %CommonProgramFiles%\VST3
        build\install\lib\lv2   -> %CommonProgramFiles%\LV2

    Payload готовит tools\build_windows_installer.bat. Брать плагины прямо из
    дерева сборки нельзя: Qt туда не разворачивается, и DAW показала бы пустое
    окно вместо интерфейса.

    Что делается при обновлении, помимо копирования:
      * бандлы VST3/LV2 **заменяются целиком**, а не сливаются — иначе файлы
        прошлой версии остаются лежать рядом и подхватываются вместо новых;
      * сносятся посторонние копии `<имя>.vst3.bak_*`. REAPER обходит папку
        VST3 рекурсивно и ключует кэш по голому имени файла, поэтому такая
        копия занимает имя настоящего плагина и грузится вместо него;
      * сносятся плоские `<имя>.vst3`-файлы от старых версий (сейчас бандлы);
      * рядом с каждым модулем проверяется Qt6Core.dll и platforms\qwindows.dll;
      * чистятся записи DONTFLOAT в кэшах сканирования DAW, чтобы хост
        перечитал плагины, а не отдал прошлый результат.

    Скрипт сам поднимает права через UAC: Program Files иначе недоступен на запись.

.PARAMETER Source
    Каталог payload (по умолчанию build\install в корне репозитория).

.PARAMETER SkipCacheReset
    Не трогать кэши сканирования DAW.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\install_plugins_windows.ps1
#>
[CmdletBinding()]
param(
    [string]$Source = "",
    [switch]$SkipCacheReset
)

$ErrorActionPreference = "Stop"

# --- права администратора -----------------------------------------------------
function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Admin)) {
    Write-Host "Запрашиваю права администратора (UAC)..."
    $childArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    if ($Source) { $childArgs += @('-Source', $Source) }
    if ($SkipCacheReset) { $childArgs += '-SkipCacheReset' }
    # -PassThru обязателен: Start-Process не выставляет $LASTEXITCODE, и без
    # него скрипт рапортовал бы успех даже после провалившегося прохода
    $child = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList $childArgs
    exit $child.ExitCode
}

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Source) { $Source = Join-Path $repoRoot "build\install" }
if (-not (Test-Path $Source)) {
    throw "Не найден payload: $Source`nСоберите его: tools\build_windows_installer.bat"
}

$clapSrc = Join-Path $Source "lib\clap"
$vst3Src = Join-Path $Source "lib\vst3"
$lv2Src  = Join-Path $Source "lib\lv2"

$clapDst = Join-Path $env:CommonProgramFiles "CLAP"
$vst3Dst = Join-Path $env:CommonProgramFiles "VST3"
$lv2Dst  = Join-Path $env:CommonProgramFiles "LV2"

# --- DAW не должна держать модули открытыми ----------------------------------
$busy = Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ProcessName -match '^(reaper|Ableton Live|Bitwig|Cubase|Studio One|FL64|Waveform)' }
if ($busy) {
    $names = ($busy | Select-Object -ExpandProperty ProcessName -Unique) -join ', '
    throw "Закройте DAW перед обновлением ($names): она держит DLL плагинов открытыми, файлы не заменятся."
}

function Assert-QtNextTo([string]$dir, [string]$label) {
    foreach ($needed in @("Qt6Core.dll", "platforms\qwindows.dll")) {
        if (-not (Test-Path (Join-Path $dir $needed))) {
            throw "$label`: рядом с модулем нет $needed ($dir). DAW покажет пустое окно. Пересоберите payload: tools\build_windows_installer.bat"
        }
    }
}

# --- CLAP --------------------------------------------------------------------
# Папка общая для всех производителей, поэтому чистим только своё и копируем
# поверх, а не сносим каталог целиком.
if (Test-Path $clapSrc) {
    New-Item -ItemType Directory -Force $clapDst | Out-Null
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $clapDst "dontfloat_track_tool.clap")
    Copy-Item (Join-Path $clapSrc "*") $clapDst -Recurse -Force
    Assert-QtNextTo $clapDst "CLAP"
    Write-Host "CLAP  -> $clapDst"
} else {
    Write-Warning "нет $clapSrc — CLAP пропущен"
}

# --- VST3 --------------------------------------------------------------------
if (Test-Path $vst3Src) {
    New-Item -ItemType Directory -Force $vst3Dst | Out-Null
    foreach ($bundle in Get-ChildItem $vst3Src -Directory) {
        $dst = Join-Path $vst3Dst $bundle.Name

        # Плоский файл от старых версий занимает имя каталога-бандла
        if (Test-Path $dst -PathType Leaf) { Remove-Item $dst -Force }
        if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }

        # Посторонние копии бандла: REAPER находит их рекурсивно и грузит
        # вместо настоящего плагина — имя файла внутри то же самое
        Get-ChildItem $vst3Dst -Directory -Filter "$($bundle.Name).bak_*" -ErrorAction SilentlyContinue |
            ForEach-Object {
                Write-Host "  сношу постороннюю копию: $($_.Name)"
                Remove-Item $_.FullName -Recurse -Force
            }

        Copy-Item $bundle.FullName $dst -Recurse -Force
        Assert-QtNextTo (Join-Path $dst "Contents\x86_64-win") "VST3 $($bundle.Name)"
        Write-Host "VST3  -> $($bundle.Name)"
    }
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $vst3Dst "DONTFLOAT Track Tool.vst3")
} else {
    Write-Warning "нет $vst3Src — VST3 пропущен"
}

# --- LV2 ---------------------------------------------------------------------
if (Test-Path $lv2Src) {
    New-Item -ItemType Directory -Force $lv2Dst | Out-Null
    foreach ($bundle in Get-ChildItem $lv2Src -Directory) {
        $dst = Join-Path $lv2Dst $bundle.Name
        if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
        Get-ChildItem $lv2Dst -Directory -Filter "$($bundle.Name).bak_*" -ErrorAction SilentlyContinue |
            ForEach-Object {
                Write-Host "  сношу постороннюю копию: $($_.Name)"
                Remove-Item $_.FullName -Recurse -Force
            }
        Copy-Item $bundle.FullName $dst -Recurse -Force
        Assert-QtNextTo $dst "LV2 $($bundle.Name)"
        Write-Host "LV2   -> $($bundle.Name)"
    }
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $lv2Dst "dontfloat_track_tool.lv2")
} else {
    Write-Warning "нет $lv2Src — LV2 пропущен"
}

# --- кэши сканирования DAW ----------------------------------------------------
# Без этого хост отдаёт прошлый результат разбора — в том числе запомненную
# неудачу, и обновлённый плагин в списке так и не появляется.
if (-not $SkipCacheReset) {
    $reaper = Join-Path $env:APPDATA "REAPER"
    foreach ($ini in @("reaper-vstplugins64.ini", "reaper-clap-win64.ini")) {
        $path = Join-Path $reaper $ini
        if (-not (Test-Path $path)) { continue }
        $content = Get-Content $path -Raw
        $cleaned = $content `
            -replace '(?im)^.*DONTFLOAT.*\r?\n', '' `
            -replace '(?im)^\[dontfloat[^\]]*\]\r?\n(?:_[^\r\n]*\r?\n)?', ''
        if ($cleaned -ne $content) {
            Set-Content -Path $path -Value $cleaned -NoNewline
            Write-Host "Кэш очищен: $ini"
        }
    }
}

Write-Host ""
Write-Host "Готово. Запустите DAW и пересканируйте плагины."
