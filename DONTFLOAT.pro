QT       += core gui multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = DONTFLOAT
TEMPLATE = app

INCLUDEPATH += include/

SOURCES += \
        src/main.cpp\
        src/mainwindow.cpp \
        src/waveformview.cpp \
        src/pitchgridwidget.cpp \
        src/waveformcolors.cpp \
        src/bpmanalyzer.cpp \
        src/audiocommand.cpp \
        src/bpmfixdialog.cpp \
        src/metronomesettingsdialog.cpp \
        src/beatfixcommand.cpp

HEADERS += \
        include/mainwindow.h \
        include/waveformview.h \
        include/pitchgridwidget.h \
        include/waveformcolors.h \
        include/bpmanalyzer.h \
        include/audiocommand.h \
        include/bpmfixdialog.h \
        include/metronomesettingsdialog.h \
        include/beatfixcommand.h

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