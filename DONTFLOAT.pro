QT       += core gui multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Указываем версию MSVC компилятора для qmake
# MSVC 14.44 соответствует версии компилятора 19.44
win32-msvc*: QMAKE_MSC_VER = 1944

TARGET = DONTFLOAT
TEMPLATE = app

INCLUDEPATH += include/

# Third-party: Mixxx qm-dsp
QM_DSP_ROOT = $$PWD/thirdparty/mixxx/lib/qm-dsp
exists($$QM_DSP_ROOT) {
    # Добавляем пути включений
    INCLUDEPATH += $$QM_DSP_ROOT
    INCLUDEPATH += $$QM_DSP_ROOT/include
    
    # Добавляем файлы qm-dsp (те же, что использует Mixxx)
    SOURCES += \
        $$QM_DSP_ROOT/base/Pitch.cpp \
        $$QM_DSP_ROOT/dsp/chromagram/Chromagram.cpp \
        $$QM_DSP_ROOT/dsp/chromagram/ConstantQ.cpp \
        $$QM_DSP_ROOT/dsp/keydetection/GetKeyMode.cpp \
        $$QM_DSP_ROOT/dsp/onsets/DetectionFunction.cpp \
        $$QM_DSP_ROOT/dsp/onsets/PeakPicking.cpp \
        $$QM_DSP_ROOT/dsp/phasevocoder/PhaseVocoder.cpp \
        $$QM_DSP_ROOT/dsp/rateconversion/Decimator.cpp \
        $$QM_DSP_ROOT/dsp/signalconditioning/DFProcess.cpp \
        $$QM_DSP_ROOT/dsp/signalconditioning/FiltFilt.cpp \
        $$QM_DSP_ROOT/dsp/signalconditioning/Filter.cpp \
        $$QM_DSP_ROOT/dsp/signalconditioning/Framer.cpp \
        $$QM_DSP_ROOT/dsp/tempotracking/DownBeat.cpp \
        $$QM_DSP_ROOT/dsp/tempotracking/TempoTrack.cpp \
        $$QM_DSP_ROOT/dsp/tempotracking/TempoTrackV2.cpp \
        $$QM_DSP_ROOT/dsp/tonal/ChangeDetectionFunction.cpp \
        $$QM_DSP_ROOT/dsp/tonal/TCSgram.cpp \
        $$QM_DSP_ROOT/dsp/tonal/TonalEstimator.cpp \
        $$QM_DSP_ROOT/dsp/transforms/FFT.cpp \
        $$QM_DSP_ROOT/ext/kissfft/kiss_fft.c \
        $$QM_DSP_ROOT/ext/kissfft/tools/kiss_fftr.c \
        $$QM_DSP_ROOT/maths/Correlation.cpp \
        $$QM_DSP_ROOT/maths/KLDivergence.cpp \
        $$QM_DSP_ROOT/maths/MathUtilities.cpp
    
    # Определения компилятора (как в Mixxx)
    DEFINES += kiss_fft_scalar=double
    DEFINES += USE_MIXXX_QM_DSP
    
    # MSVC требует _USE_MATH_DEFINES для M_PI
    win32-msvc*: DEFINES += _USE_MATH_DEFINES
    
    # Подавляем предупреждения компилятора для внешней библиотеки Mixxx
    # C4244: преобразование типов (double->float, int64_t->int, size_t->int) - нормально для библиотеки
    # C4267: преобразование size_t в меньший тип - нормально для библиотеки
    # C4828: проблемы с кодировкой файла - не критично
    # В qmake сложно применять флаги только к отдельным файлам, поэтому применяем глобально для MSVC
    win32-msvc* {
        QMAKE_CXXFLAGS += /wd4244 /wd4267 /wd4828
        QMAKE_CFLAGS += /wd4244 /wd4267 /wd4828
    }
    
    message("Mixxx qm-dsp enabled: $$QM_DSP_ROOT")
} else {
    message("Warning: qm-dsp not found at $$QM_DSP_ROOT, using simplified BPM analyzer")
}

SOURCES += \
        src/main.cpp\
        src/mainwindow.cpp \
        src/waveformview.cpp \
        src/markerstretchengine.cpp \
        src/pitchgridwidget.cpp \
        src/waveformcolors.cpp \
        src/bpmanalyzer.cpp \
        src/keyanalyzer.cpp \
        src/waveformanalyzer.cpp \
        src/audiocommand.cpp \
        src/loadfiledialog.cpp \
        src/metronomesettingsdialog.cpp \
        src/metronomecontroller.cpp \
        src/beatfixcommand.cpp \
        src/timestretchprocessor.cpp \
        src/timeutils.cpp
        # src/beatvisualizer.cpp
        # src/beatvisualizationsettingsdialog.cpp

HEADERS += \
        include/mainwindow.h \
        include/waveformview.h \
        include/markerstretchengine.h \
        include/pitchgridwidget.h \
        include/waveformcolors.h \
        include/bpmanalyzer.h \
        include/keyanalyzer.h \
        include/waveformanalyzer.h \
        include/audiocommand.h \
        include/loadfiledialog.h \
        include/metronomesettingsdialog.h \
        include/metronomecontroller.h \
        include/beatfixcommand.h \
        include/timestretchprocessor.h \
        include/timeutils.h
        # include/beatvisualizer.h
        # include/beatvisualizationsettingsdialog.h

FORMS += \
        ui/mainwindow.ui \
        ui/loadfiledialog.ui \
        ui/metronomesettingsdialog.ui
        # ui/beatvisualizationsettingsdialog.ui

TRANSLATIONS += \
        translations/DONTFLOAT_ru_RU.ts

RESOURCES += \
        resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target