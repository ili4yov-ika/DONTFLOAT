# Repairs installed DONTFLOAT DAW plugins:
# - packs VST3 as Steinberg bundles with Qt runtime next to the .impl.dll
# - deploys Qt next to CLAP/LV2 impl modules (stub hosts load via LoadLibraryEx)
# - requires platforms\qwindows.dll (plugin code pumps Qt via Win32 timer)
# Self-elevates via UAC when not already running as Administrator.

$ErrorActionPreference = "Stop"
$b = "d:\Devs\C++\DONTFLOAT_exp\build\Desktop_Qt_6_9_3_MSVC2022_64bit-Release"
$wdq = "C:\Qt\6.9.3\msvc2022_64\bin\windeployqt.exe"

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Admin)) {
    Write-Host "Requesting Administrator privileges (UAC)..."
    $args = @(
        '-NoProfile'
        '-ExecutionPolicy', 'Bypass'
        '-File', $PSCommandPath
    )
    Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -ArgumentList $args
    exit $LASTEXITCODE
}

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

# Ensure VST3 bundles exist: stub (.vst3) + impl (.impl.dll) + Qt beside impl
foreach ($n in @("DONTFLOAT", "DONTFLOAT Scratch", "DONTFLOAT Pitcher")) {
    $arch = "$b\plugins\vst3\$n.vst3\Contents\x86_64-win"
    New-Item -ItemType Directory -Force -Path $arch | Out-Null
    Copy-Item (Join-Path "$b\Release" "$n.vst3") (Join-Path $arch "$n.vst3") -Force
    Copy-Item (Join-Path "$b\Release" "$n.vst3.impl.dll") (Join-Path $arch "$n.vst3.impl.dll") -Force
    Remove-Item (Join-Path $arch "$n.impl.dll") -Force -ErrorAction SilentlyContinue
    Deploy-Qt $arch (Join-Path $arch "$n.vst3.impl.dll")
}

# CLAP: stub .clap + impl.dll + Qt
$clapStage = "$b\plugin_stage\clap"
New-Item -ItemType Directory -Force -Path $clapStage | Out-Null
foreach ($n in @("dontfloat", "dontfloat_scratch", "dontfloat_pitcher")) {
    Copy-Item "$b\Release\$n.clap" $clapStage -Force
    Copy-Item "$b\Release\${n}_clap.impl.dll" $clapStage -Force
}
Deploy-Qt $clapStage "$clapStage\dontfloat_clap.impl.dll"

# LV2 Qt against UI impl modules
foreach ($pair in @(
    @{ Bundle = "dontfloat.lv2"; Impl = "dontfloat_ui.impl.dll"; Stub = "dontfloat_ui.dll" },
    @{ Bundle = "dontfloat_scratch.lv2"; Impl = "dontfloat_scratch_ui.impl.dll"; Stub = "dontfloat_scratch_ui.dll" },
    @{ Bundle = "dontfloat_pitcher.lv2"; Impl = "dontfloat_pitcher_ui.impl.dll"; Stub = "dontfloat_pitcher_ui.dll" }
)) {
    $dir = "$b\plugins\lv2\$($pair.Bundle)"
    Deploy-Qt $dir "$dir\$($pair.Impl)"
}

$clapDst = "$env:COMMONPROGRAMFILES\CLAP"
$lv2Dst = "$env:COMMONPROGRAMFILES\LV2"
$vstDst = "$env:COMMONPROGRAMFILES\VST3"

# CLAP install
Get-ChildItem $clapStage | ForEach-Object {
    $dest = Join-Path $clapDst $_.Name
    if ($_.PSIsContainer) {
        if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
        Copy-Item $_.FullName $dest -Recurse -Force
    } else {
        Copy-Item $_.FullName $dest -Force
    }
}
Write-Host "CLAP stub: $(Test-Path (Join-Path $clapDst 'dontfloat.clap')) impl: $(Test-Path (Join-Path $clapDst 'dontfloat_clap.impl.dll')) Qt: $(Test-Path (Join-Path $clapDst 'Qt6Core.dll')) qwindows: $(Test-Path (Join-Path $clapDst 'platforms\qwindows.dll'))"

# LV2 install (replace whole bundles)
foreach ($n in @("dontfloat.lv2", "dontfloat_scratch.lv2", "dontfloat_pitcher.lv2")) {
    $src = "$b\plugins\lv2\$n"
    $dst = Join-Path $lv2Dst $n
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    Copy-Item $src $dst -Recurse -Force
    Write-Host "LV2 $n Qt: $(Test-Path (Join-Path $dst 'Qt6Core.dll')) qwindows: $(Test-Path (Join-Path $dst 'platforms\qwindows.dll'))"
}

# VST3 install (replace with stub+impl bundles)
foreach ($n in @("DONTFLOAT", "DONTFLOAT Scratch", "DONTFLOAT Pitcher")) {
    $flat = Join-Path $vstDst "$n.vst3"
    if (Test-Path $flat -PathType Leaf) { Remove-Item $flat -Force }
    $src = "$b\plugins\vst3\$n.vst3"
    $dst = Join-Path $vstDst "$n.vst3"
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    Copy-Item $src $dst -Recurse -Force
    $qtOk = Test-Path (Join-Path $dst "Contents\x86_64-win\Qt6Core.dll")
    $qpaOk = Test-Path (Join-Path $dst "Contents\x86_64-win\platforms\qwindows.dll")
    $implOk = Test-Path (Join-Path $dst "Contents\x86_64-win\$n.vst3.impl.dll")
    Write-Host "VST3 $n bundle=$([bool](Test-Path $dst -PathType Container)) impl=$implOk Qt=$qtOk qwindows=$qpaOk"
}

# Remove legacy flat / track tool leftovers
Remove-Item -Force -ErrorAction SilentlyContinue @(
    "$vstDst\DONTFLOAT Track Tool.vst3",
    "$clapDst\dontfloat_track_tool.clap"
)

# Clear failed Reaper scan cache entries so next launch rescans.
$reaper = "$env:APPDATA\REAPER"
foreach ($ini in @(
    "$reaper\reaper-vstplugins64.ini",
    "$reaper\reaper-clap-win64.ini"
)) {
    if (-not (Test-Path $ini)) { continue }
    $content = Get-Content $ini -Raw
    $cleaned = $content -replace '(?m)^.*DONTFLOAT.*\r?\n', '' -replace '(?m)^\[dontfloat[^\]]*\]\r?\n(?:_[^\r\n]*\r?\n)?', ''
    if ($cleaned -ne $content) {
        Set-Content -Path $ini -Value $cleaned -NoNewline
        Write-Host "Cleared DONTFLOAT entries in $(Split-Path $ini -Leaf)"
    }
}

Write-Host "Done. Restart Reaper and rescan plugins (Clear cache / re-scan)."
