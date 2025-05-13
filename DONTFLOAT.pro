QT       += core gui multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = DONTFLOAT
TEMPLATE = app

INCLUDEPATH += include/

SOURCES += \
        src/main.cpp\
        src/mainwindow.cpp \
        src/waveformview.cpp

HEADERS += \
        include/mainwindow.h \
        include/waveformview.h

FORMS += \
        ui/mainwindow.ui

TRANSLATIONS += \
        translations/DONTFLOAT_ru_RU.ts

RESOURCES += \
        resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target