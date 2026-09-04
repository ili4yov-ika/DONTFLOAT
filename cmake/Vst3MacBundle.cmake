# Info.plist и PkgInfo для бандла VST3 под macOS.
#
# Без Info.plist хост бандл не откроет вовсе: macOS считает такой каталог
# обычной папкой, а не загружаемым модулем. Скрипт зовётся из POST_BUILD цели
# плагина (см. cmake/PluginProducts.cmake) в режиме `cmake -P`, поэтому все
# значения приходят через -D.
#
# Ожидает: BUNDLE_DIR, MODULE_NAME, BUNDLE_VERSION.

if(NOT DEFINED BUNDLE_DIR OR NOT DEFINED MODULE_NAME)
    message(FATAL_ERROR "Vst3MacBundle.cmake: нужны BUNDLE_DIR и MODULE_NAME")
endif()
if(NOT DEFINED BUNDLE_VERSION OR BUNDLE_VERSION STREQUAL "")
    set(BUNDLE_VERSION "0.0.0")
endif()

# Идентификатор обязан быть уникальным на систему: пробелы в имени редакции
# («DONTFLOAT Pitcher») в него не годятся
string(REPLACE " " "" _module_id "${MODULE_NAME}")
string(TOLOWER "${_module_id}" _module_id)

file(MAKE_DIRECTORY "${BUNDLE_DIR}/Contents")

file(WRITE "${BUNDLE_DIR}/Contents/Info.plist"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleExecutable</key>
    <string>${MODULE_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>com.dontfloat.${_module_id}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${MODULE_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>${BUNDLE_VERSION}</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>${BUNDLE_VERSION}</string>
    <key>CSResourcesFileMapped</key>
    <true/>
</dict>
</plist>
")

# Четыре байта типа и четыре — создателя; так их пишет Steinberg SDK
file(WRITE "${BUNDLE_DIR}/Contents/PkgInfo" "BNDL????")
