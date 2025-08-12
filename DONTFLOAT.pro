QT += core gui widgets multimedia

CONFIG += c++17

TEMPLATE = app
TARGET = DONTFLOAT

INCLUDEPATH += $$PWD/include
INCLUDEPATH += $$PWD/thirdparty/mixxx/lib/qm-dsp
INCLUDEPATH += $$PWD/thirdparty/mixxx/lib/qm-dsp/include

DEFINES += kiss_fft_scalar=double

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/waveformview.cpp \
    src/pitchgridwidget.cpp \
    src/waveformcolors.cpp \
    src/bpmanalyzer.cpp \
    src/audiocommand.cpp \
    src/bpmfixdialog.cpp \
    src/metronomesettingsdialog.cpp \
    src/beatfixcommand.cpp \
    src/mixxxbpmanalyzer.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/onsets/DetectionFunction.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/onsets/PeakPicking.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/tempotracking/TempoTrackV2.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/tempotracking/TempoTrack.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/phasevocoder/PhaseVocoder.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/signalconditioning/DFProcess.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/signalconditioning/FiltFilt.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/signalconditioning/Framer.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/signalconditioning/Filter.cpp \
    thirdparty/mixxx/lib/qm-dsp/maths/Correlation.cpp \
    thirdparty/mixxx/lib/qm-dsp/dsp/transforms/FFT.cpp \
    thirdparty/mixxx/lib/qm-dsp/maths/MathUtilities.cpp \
    thirdparty/mixxx/lib/qm-dsp/ext/kissfft/kiss_fft.c \
    thirdparty/mixxx/lib/qm-dsp/ext/kissfft/tools/kiss_fftr.c

HEADERS += \
    include/mainwindow.h \
    include/waveformview.h \
    include/pitchgridwidget.h \
    include/waveformcolors.h \
    include/bpmanalyzer.h \
    include/audiocommand.h \
    include/bpmfixdialog.h \
    include/metronomesettingsdialog.h \
    include/beatfixcommand.h \
    include/mixxxbpmanalyzer.h

FORMS += \
    ui/mainwindow.ui

TRANSLATIONS += \
    translations/DONTFLOAT_ru_RU.ts

RESOURCES += \
    resources.qrc

# Deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target