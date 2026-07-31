; NSIS Installer Script for DONTFLOAT
; Usage: makensis /DBUILD_DIR="path\to\build\install" nsis_installer.nsi

Unicode true
!include "MUI2.nsh"

; Product info
!define PRODUCT_NAME "DONTFLOAT"
!define PRODUCT_VERSION "0.0.0.1"
!define PRODUCT_PUBLISHER "DONTFLOAT Project"
!define PRODUCT_WEB_SITE "https://github.com/ili4yov-ika/DONTFLOAT"
!define CLAP_INSTALL_DIR "$COMMONFILES64\CLAP"
!define LV2_INSTALL_DIR "$COMMONFILES64\LV2"
!define VST3_INSTALL_DIR "$COMMONFILES64\VST3"

; Общие настройки
Name "${PRODUCT_NAME}"
OutFile "DONTFLOAT_Setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "Software\${PRODUCT_NAME}" "InstallPath"
RequestExecutionLevel admin

; Интерфейс
!define MUI_ABORTWARNING

; Страницы
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "English"

; Секция установки
Section "DONTFLOAT application" SEC_APP
  SectionIn RO

  SetOutPath "$INSTDIR"

  ; Копируем bin (exe и dll)
  SetOutPath "$INSTDIR"
  File /r "${BUILD_DIR}\bin\*.*"

  ; Копируем переводы, если есть
  SetOutPath "$INSTDIR\translations"
  File /nonfatal /r "${BUILD_DIR}\share\dontfloat\translations\*.*"
  File /nonfatal /r "${BUILD_DIR}\share\DONTFLOAT\translations\*.*"

  ; Записываем путь установки в реестр
  WriteRegStr HKLM "Software\${PRODUCT_NAME}" "InstallPath" "$INSTDIR"

  ; Создаём деинсталлятор
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Запись в Add/Remove Programs
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"

  ; Ярлык в меню Пуск
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\DONTFLOAT.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

SectionGroup /e "DAW plugins" SEC_PLUGINS
  ; CLAP: stub .clap + *.impl.dll + Qt runtime рядом
  Section "CLAP plugins (DONTFLOAT, Scratch, Pitcher)" SEC_CLAP
    SetOutPath "${CLAP_INSTALL_DIR}"
    File /nonfatal /r "${BUILD_DIR}\lib\clap\*.*"
  SectionEnd

  ; LV2: каждый bundle — DSP + UI stub/impl + Qt (включая platforms\qwindows.dll).
  ; Без platforms DAW: "no Qt platform plugin could be initialized".
  Section "LV2 plugins (DONTFLOAT, Scratch, Pitcher)" SEC_LV2
    SetOutPath "${LV2_INSTALL_DIR}\dontfloat.lv2"
    File /nonfatal /r "${BUILD_DIR}\lib\lv2\dontfloat.lv2\*.*"
    SetOutPath "${LV2_INSTALL_DIR}\dontfloat_scratch.lv2"
    File /nonfatal /r "${BUILD_DIR}\lib\lv2\dontfloat_scratch.lv2\*.*"
    SetOutPath "${LV2_INSTALL_DIR}\dontfloat_pitcher.lv2"
    File /nonfatal /r "${BUILD_DIR}\lib\lv2\dontfloat_pitcher.lv2\*.*"
  SectionEnd

  ; VST3: Steinberg bundles (DONTFLOAT.vst3/Contents/x86_64-win/...)
  ; Qt + platforms\qwindows.dll рядом с *.impl.dll (не в корне VST3).
  ; UI pump живёт в коде плагина (ensureQtApplication); без Qt runtime DAW зависает/пусто.
  Section "VST3 plugins (DONTFLOAT, Scratch, Pitcher)" SEC_VST3
    ; Удаляем устаревшие плоские .vst3 (DLL) от прошлых установок
    Delete "${VST3_INSTALL_DIR}\DONTFLOAT.vst3"
    Delete "${VST3_INSTALL_DIR}\DONTFLOAT Scratch.vst3"
    Delete "${VST3_INSTALL_DIR}\DONTFLOAT Pitcher.vst3"
    Delete "${VST3_INSTALL_DIR}\DONTFLOAT Track Tool.vst3"

    SetOutPath "${VST3_INSTALL_DIR}\DONTFLOAT.vst3"
    File /nonfatal /r "${BUILD_DIR}\lib\vst3\DONTFLOAT.vst3\*.*"
    SetOutPath "${VST3_INSTALL_DIR}\DONTFLOAT Scratch.vst3"
    File /nonfatal /r "${BUILD_DIR}\lib\vst3\DONTFLOAT Scratch.vst3\*.*"
    SetOutPath "${VST3_INSTALL_DIR}\DONTFLOAT Pitcher.vst3"
    File /nonfatal /r "${BUILD_DIR}\lib\vst3\DONTFLOAT Pitcher.vst3\*.*"
  SectionEnd
SectionGroupEnd

; Секция деинсталляции
Section "Uninstall"
  ; Удаляем файлы и подкаталоги
  RMDir /r "$INSTDIR\translations"
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\plugins"
  RMDir /r "$INSTDIR\clap"
  Delete "$INSTDIR\DONTFLOAT.exe"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\*.dll"

  ; Удаляем DAW-плагины
  Delete "${CLAP_INSTALL_DIR}\dontfloat.clap"
  Delete "${CLAP_INSTALL_DIR}\dontfloat_scratch.clap"
  Delete "${CLAP_INSTALL_DIR}\dontfloat_pitcher.clap"
  Delete "${CLAP_INSTALL_DIR}\dontfloat_clap.impl.dll"
  Delete "${CLAP_INSTALL_DIR}\dontfloat_scratch_clap.impl.dll"
  Delete "${CLAP_INSTALL_DIR}\dontfloat_pitcher_clap.impl.dll"
  ; Qt runtime рядом с CLAP (общие DLL)
  Delete "${CLAP_INSTALL_DIR}\Qt6Core.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Gui.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Widgets.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Multimedia.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Network.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Concurrent.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Svg.dll"
  Delete "${CLAP_INSTALL_DIR}\Qt6Pdf.dll"
  Delete "${CLAP_INSTALL_DIR}\opengl32sw.dll"
  Delete "${CLAP_INSTALL_DIR}\D3Dcompiler_47.dll"
  Delete "${CLAP_INSTALL_DIR}\dxcompiler.dll"
  Delete "${CLAP_INSTALL_DIR}\dxil.dll"
  Delete "${CLAP_INSTALL_DIR}\avcodec-61.dll"
  Delete "${CLAP_INSTALL_DIR}\avformat-61.dll"
  Delete "${CLAP_INSTALL_DIR}\avutil-59.dll"
  Delete "${CLAP_INSTALL_DIR}\swresample-5.dll"
  Delete "${CLAP_INSTALL_DIR}\swscale-8.dll"
  RMDir /r "${CLAP_INSTALL_DIR}\platforms"
  RMDir /r "${CLAP_INSTALL_DIR}\imageformats"
  RMDir /r "${CLAP_INSTALL_DIR}\multimedia"
  RMDir /r "${CLAP_INSTALL_DIR}\styles"
  RMDir /r "${CLAP_INSTALL_DIR}\tls"
  RMDir /r "${CLAP_INSTALL_DIR}\generic"
  RMDir /r "${CLAP_INSTALL_DIR}\iconengines"
  RMDir /r "${CLAP_INSTALL_DIR}\networkinformation"

  RMDir /r "${LV2_INSTALL_DIR}\dontfloat.lv2"
  RMDir /r "${LV2_INSTALL_DIR}\dontfloat_scratch.lv2"
  RMDir /r "${LV2_INSTALL_DIR}\dontfloat_pitcher.lv2"
  RMDir /r "${LV2_INSTALL_DIR}\dontfloat_track_tool.lv2"

  RMDir /r "${VST3_INSTALL_DIR}\DONTFLOAT.vst3"
  RMDir /r "${VST3_INSTALL_DIR}\DONTFLOAT Scratch.vst3"
  RMDir /r "${VST3_INSTALL_DIR}\DONTFLOAT Pitcher.vst3"
  Delete "${VST3_INSTALL_DIR}\DONTFLOAT.vst3"
  Delete "${VST3_INSTALL_DIR}\DONTFLOAT Scratch.vst3"
  Delete "${VST3_INSTALL_DIR}\DONTFLOAT Pitcher.vst3"
  Delete "${VST3_INSTALL_DIR}\DONTFLOAT Track Tool.vst3"

  RMDir "$INSTDIR"

  ; Удаляем ярлыки
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"

  ; Удаляем из реестра
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  DeleteRegKey HKLM "Software\${PRODUCT_NAME}"
SectionEnd
