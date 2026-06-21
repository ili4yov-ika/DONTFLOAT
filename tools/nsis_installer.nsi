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
  Section /o "CLAP plugin" SEC_CLAP
    SetOutPath "${CLAP_INSTALL_DIR}"
    File /nonfatal "${BUILD_DIR}\lib\clap\dontfloat_pitch_shift.clap"
  SectionEnd

  Section /o "LV2 plugin" SEC_LV2
    SetOutPath "${LV2_INSTALL_DIR}\dontfloat_pitch_shift.lv2"
    File /nonfatal /r "${BUILD_DIR}\lib\lv2\dontfloat_pitch_shift.lv2\*.*"
  SectionEnd

  Section /o "VST3 plugin" SEC_VST3
    SetOutPath "${VST3_INSTALL_DIR}"
    File /nonfatal /r "${BUILD_DIR}\lib\vst3\*.*"
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

  ; Удаляем DAW-плагины, установленные опциональными секциями
  Delete "${CLAP_INSTALL_DIR}\dontfloat_pitch_shift.clap"
  RMDir /r "${LV2_INSTALL_DIR}\dontfloat_pitch_shift.lv2"
  Delete "${VST3_INSTALL_DIR}\DONTFLOAT Pitch Shift.vst3"
  RMDir /r "${VST3_INSTALL_DIR}\DONTFLOAT Pitch Shift.vst3"

  RMDir "$INSTDIR"

  ; Удаляем ярлыки
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"

  ; Удаляем из реестра
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  DeleteRegKey HKLM "Software\${PRODUCT_NAME}"
SectionEnd
