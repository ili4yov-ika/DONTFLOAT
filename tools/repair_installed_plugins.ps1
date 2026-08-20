# Repairs DONTFLOAT plugins inside the *build tree*, then installs them.
#
# Use this when there is no installer payload yet: it stages what
# build_windows_installer.bat would produce — VST3 packed as Steinberg bundles,
# Qt (including platforms\qwindows.dll) deployed next to every impl module —
# and hands the result to install_plugins_windows.ps1, which owns the actual
# install: replacing bundles, clearing stale copies and resetting DAW caches.
#
# Normal path is the installer payload:
#   tools\build_windows_installer.bat
#   powershell -File tools\install_plugins_windows.ps1
#
# Self-elevates via UAC when not already running as Administrator.

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repoRoot "build\Desktop_Qt_6_9_3_MSVC2022_64bit-Release"
if ($env:DONTFLOAT_BUILD_DIR) { $build = $env:DONTFLOAT_BUILD_DIR }

function Find-WinDeployQt {
    foreach ($candidate in @(
        "C:\Qt\6.9.3\msvc2022_64\bin\windeployqt.exe",
        "C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe"
    )) {
        if (Test-Path $candidate) { return $candidate }
    }
    $onPath = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    throw "windeployqt не найден — укажите Qt в PATH или поправьте список путей"
}

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Admin)) {
    Write-Host "Запрашиваю права администратора (UAC)..."
    $childArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    # -PassThru обязателен: Start-Process не выставляет $LASTEXITCODE, и без него
    # скрипт возвращал бы ноль даже после провалившегося прохода
    $child = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList $childArgs
    exit $child.ExitCode
}

if (-not (Test-Path $build)) {
    throw "Каталог сборки не найден: $build`nЗадайте DONTFLOAT_BUILD_DIR или соберите конфигурацию Release."
}
$wdq = Find-WinDeployQt

function Deploy-Qt($dir, $binary) {
    if (-not (Test-Path $binary)) { throw "Missing $binary" }
    & $wdq --release --no-translations --no-compiler-runtime --dir $dir $binary | Out-Null
    if (-not (Test-Path (Join-Path $dir "Qt6Core.dll"))) {
        throw "Qt6Core.dll missing in $dir after windeployqt"
    }
    if (-not (Test-Path (Join-Path $dir "platforms\qwindows.dll"))) {
        throw "platforms\qwindows.dll missing in $dir (REAPER: no Qt platform plugin)"
    }
}

# VST3: stub (.vst3) + impl (.impl.dll) + Qt beside impl
foreach ($n in @("DONTFLOAT", "DONTFLOAT Scratch", "DONTFLOAT Pitcher")) {
    $arch = "$build\plugins\vst3\$n.vst3\Contents\x86_64-win"
    New-Item -ItemType Directory -Force -Path $arch | Out-Null
    Copy-Item (Join-Path "$build\Release" "$n.vst3") (Join-Path $arch "$n.vst3") -Force
    Copy-Item (Join-Path "$build\Release" "$n.vst3.impl.dll") (Join-Path $arch "$n.vst3.impl.dll") -Force
    Remove-Item (Join-Path $arch "$n.impl.dll") -Force -ErrorAction SilentlyContinue
    Deploy-Qt $arch (Join-Path $arch "$n.vst3.impl.dll")
    Write-Host "VST3 staged: $n"
}

# CLAP: stub .clap + impl.dll + Qt, разложенные как в payload установщика
$clapStage = "$build\plugin_stage\lib\clap"
if (Test-Path $clapStage) { Remove-Item $clapStage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $clapStage | Out-Null
foreach ($n in @("dontfloat", "dontfloat_scratch", "dontfloat_pitcher")) {
    Copy-Item "$build\Release\$n.clap" $clapStage -Force
    Copy-Item "$build\Release\${n}_clap.impl.dll" $clapStage -Force
}
Deploy-Qt $clapStage "$clapStage\dontfloat_clap.impl.dll"
Write-Host "CLAP staged"

# LV2: Qt рядом с UI-модулем каждого бандла
foreach ($pair in @(
    @{ Bundle = "dontfloat.lv2"; Impl = "dontfloat_ui.impl.dll" },
    @{ Bundle = "dontfloat_scratch.lv2"; Impl = "dontfloat_scratch_ui.impl.dll" },
    @{ Bundle = "dontfloat_pitcher.lv2"; Impl = "dontfloat_pitcher_ui.impl.dll" }
)) {
    $dir = "$build\plugins\lv2\$($pair.Bundle)"
    Deploy-Qt $dir "$dir\$($pair.Impl)"
    Write-Host "LV2 staged: $($pair.Bundle)"
}

# Собираем дерево в раскладке payload и отдаём установщику плагинов: замена
# бандлов, снос посторонних копий и сброс кэшей DAW живут там, в одном месте
$stage = "$build\plugin_stage"
Copy-Item "$build\plugins\vst3" (Join-Path $stage "lib") -Recurse -Force
Copy-Item "$build\plugins\lv2" (Join-Path $stage "lib") -Recurse -Force

& (Join-Path $PSScriptRoot "install_plugins_windows.ps1") -Source $stage
if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) { exit $LASTEXITCODE }
