#include "../include/mainwindow.h"
#include "../include/uiconstants.h"
#include "../include/spectrogramsettingsdialog.h"
#include "../include/pitchshiftsettingsdialog.h"
#include "../include/beatfixcommand.h"
#include "../include/timestretchcommand.h"
#include "../include/timestretchprocessor.h"
#include "../include/pitchcorrection.h"
#include "../include/pitchnoteeditcommand.h"
#include "ui_mainwindow.h"
#include <QtWidgets/QApplication>
#include <QtCore/QFileInfo>
#include <QtGui/QIcon>
#include <QtCore/QTimer>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStatusBar>
#include <QtGui/QCloseEvent>
#include <QtMultimedia/QAudioDecoder>
#include <QtMultimedia/QAudioBuffer>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QSoundEffect>

#include <QtGui/QKeyEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QProgressBar>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QMessageBox>
#include <QtCore/QDir>
#include <QtCore/QtGlobal>
#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtCore/QSignalBlocker>
#include <QtCore/QEventLoop>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QFutureWatcher>
#include <memory>
#include <QtCore/QtMath>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QInputDialog>
#include "../include/bpmanalyzer.h"
#include "../include/keyanalyzer.h"
#include "../include/loadfiledialog.h"
#include "../include/metronomesettingsdialog.h"
#include "../include/markerengine.h"
#include "../include/timeutils.h"
#include "../include/wavwriter.h"
#include "../include/audiofileservice.h"
#include <QUndoStack>
#include <QtGui/QShortcut>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QValidator>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <QtCore/QSet>
#include <QtCore/QtAlgorithms>
#include <QtCore/QTranslator>
#include <QtCore/QLocale>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static QStringList translationSearchPaths()
{
    QStringList paths;
    const QString appDir = QCoreApplication::applicationDirPath();
    paths << appDir << (appDir + "/../") << (appDir + "/../translations/")
          << (appDir + "/translations/") << (appDir + "/../build");
    return paths;
}

static bool loadTranslation(QTranslator *translator, const QString &name)
{
    for (const QString &path : translationSearchPaths()) {
        if (translator->load(name, path))
            return true;
    }
    return false;
}

namespace {

void alignWaveformViewToBarGrid(WaveformView *waveformView, float bpm, int beatsPerBar, qint64 gridStartSample)
{
    if (!waveformView || bpm <= 0.f)
        return;
    waveformView->setZoomLevel(1.0f);
    const int sampleRate = waveformView->getSampleRate();
    const float samplesPerBeat = (60.0f * sampleRate) / bpm;
    const float barLengthInQuarters =
        (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    const float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    float offset = float(gridStartSample) / samplesPerBar;
    offset = offset - floor(offset);
    waveformView->setHorizontalOffset(offset);
}

/**
 * Кусочно-линейное отображение сэмпла из координат исходного аудио в координаты
 * текущего таймлайна по меткам (originalPosition → position).
 */
qint64 mapSampleThroughMarkers(qint64 sample, const QVector<Marker>& sorted)
{
    if (sorted.isEmpty()) {
        return sample;
    }
    if (sample <= sorted.first().originalPosition) {
        return sample + (sorted.first().position - sorted.first().originalPosition);
    }
    for (int i = 0; i + 1 < sorted.size(); ++i) {
        const Marker& a = sorted[i];
        const Marker& b = sorted[i + 1];
        if (sample <= b.originalPosition) {
            const qint64 origSpan = b.originalPosition - a.originalPosition;
            if (origSpan <= 0) {
                return b.position;
            }
            const double t = double(sample - a.originalPosition) / double(origSpan);
            return a.position + qint64(t * double(b.position - a.position) + 0.5);
        }
    }
    return sample + (sorted.last().position - sorted.last().originalPosition);
}

/** Warp координат нот через метки: ноты остаются в порядке исходного вектора. */
QVector<PitchDetector::PitchNote> warpNotesThroughMarkers(
    const QVector<PitchDetector::PitchNote>& notes, QVector<Marker> markers)
{
    if (markers.isEmpty()) {
        return notes;
    }
    std::sort(markers.begin(), markers.end(),
              [](const Marker& a, const Marker& b) {
                  return a.originalPosition < b.originalPosition;
              });

    QVector<PitchDetector::PitchNote> out = notes;
    for (PitchDetector::PitchNote& note : out) {
        const qint64 start = mapSampleThroughMarkers(note.startSample, markers);
        const qint64 end = mapSampleThroughMarkers(note.endSample, markers);
        note.startSample = qMin(start, end);
        note.endSample = qMax(start, end);
    }
    return out;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , waveformView(nullptr)
    , pitchGridWidget(nullptr)
    , pitchGridScrollContainer(nullptr)
    , pitchGridAnalyzeOverlay(nullptr)
    , pitchGridAnalyzeButton(nullptr)
    , pitchGridAnalyzeProgress(nullptr)
    , horizontalScrollBar(nullptr)
    , pitchGridVerticalScrollBar(nullptr)
    , mainSplitter(nullptr)
    , fileMenu(nullptr)
    , editMenu(nullptr)
    , viewMenu(nullptr)
    , settingsMenu(nullptr)
    , colorSchemeMenu(nullptr)
    , themesMenu(nullptr)
    , languageMenu(nullptr)
    , currentFileName("")
    , hasUnsavedChanges(false)
    , isPlaying(false)
    , currentPosition(0)
    , playbackTimer(nullptr)
    , mediaPlayer(nullptr)
    , audioOutput(nullptr)
    , settings("DONTFLOAT", "DONTFLOAT")
    , m_appTranslator(nullptr)
    , metronomeController(nullptr)
    , isLoopEnabled(false)
    , loopStartPosition(0)
    , loopEndPosition(0)
    , isPitchGridVisible(true) // По умолчанию питч-сетка видна
    , currentKey("")
    , currentKey2("")
    , keyMenu(nullptr)
    , keyMenu2(nullptr)
    , playShortcut(nullptr)
    , shiftAShortcut(nullptr)
    , shiftBShortcut(nullptr)
    , markerShortcut(nullptr)
    , waveformViewMenu(nullptr)
    , openAct(nullptr)
    , saveAct(nullptr)
    , exitAct(nullptr)
    , undoAct(nullptr)
    , redoAct(nullptr)
    , defaultThemeAct(nullptr)
    , darkSchemeAct(nullptr)
    , lightSchemeAct(nullptr)
    , metronomeSettingsAct(nullptr)
    , keyboardShortcutsAct(nullptr)
    , playPauseAct(nullptr)
    , stopAct(nullptr)
    , metronomeAct(nullptr)
    , loopStartAct(nullptr)
    , loopEndAct(nullptr)
    , togglePitchGridAct(nullptr)
    , toggleBeatWaveformAct(nullptr)
    , waveformPeaksAct(nullptr)
    , waveformSpectrogramAct(nullptr)
    , spectrogramSettingsAct(nullptr)
    , spectrogramSettingsDialog(nullptr)
    , pitchShiftSettingsAct(nullptr)
    , pitchShiftSettingsDialog(nullptr)
    , russianAction(nullptr)
    , englishAction(nullptr)
    , applyTimeStretchAct(nullptr)
    , markerPreviewTimer(nullptr)
    , previewRestorePosition(0)
    , previewOldDuration(0)
    , previewWasPlaying(false)
{
    undoStack = new QUndoStack(this);

    // Load and install translator before setupUi (language from settings or system)
    m_appTranslator = new QTranslator(this);
    QString lang = settings.value("language", QString()).toString();
    if (lang.isEmpty()) {
        QString sys = QLocale::system().name();
        if (sys.startsWith("en")) lang = "en_US";
        else if (sys.startsWith("ru")) lang = "ru_RU";
        else lang = "en_US";
    }
    // English msgid: en_US.qm is optional (identity). Russian requires ru_RU.qm.
    if (lang == "ru_RU") {
        if (loadTranslation(m_appTranslator, "ru_RU"))
            qApp->installTranslator(m_appTranslator);
        else
            lang = "en_US";
    } else if (loadTranslation(m_appTranslator, "en_US")) {
        qApp->installTranslator(m_appTranslator);
    }

    createActions();  // Create actions first
    ui->setupUi(this);  // Then setup UI

    // Меню тональностей вынесены в отдельный класс KeySelectionMenu.
    keyMenu = new KeySelectionMenu(this);
    keyMenu2 = new KeySelectionMenu(this);
    keyRegionMenu = new KeySelectionMenu(this);
    connect(keyMenu, &KeySelectionMenu::keySelected, this, &MainWindow::setKey);
    connect(keyMenu2, &KeySelectionMenu::keySelected, this, &MainWindow::setKey2);

    // Устанавливаем стандартные флаги окна (исправлено для корректного закрытия)
    setWindowFlags(Qt::Window);

    // Разрешаем перетаскивание файлов (Drag-and-Drop)
    setAcceptDrops(true);

    // Явно разрешаем изменение размера окна
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Убираем все ограничения размера
    setMinimumSize(800, 500);
    setMaximumSize(16777215, 16777215);

    createMenus();   // Then create menus

    // Drag-and-Drop: приём капель на центральном виджете (область волны)
    QWidget *cw = centralWidget();
    if (cw) {
        cw->setAcceptDrops(true);
        cw->installEventFilter(this);
    }

    russianAction->setChecked(lang == "ru_RU");
    englishAction->setChecked(lang == "en_US");

    // Create and setup WaveformView
    waveformView = new WaveformView(this);
    if (!ui->waveformWidget->layout()) {
        ui->waveformWidget->setLayout(new QVBoxLayout());
    }
    if (auto* waveLayout = qobject_cast<QVBoxLayout*>(ui->waveformWidget->layout())) {
        // Внутренний layout без отступов; L/R задаёт waveformLayout / pitchGridLayout.
        waveLayout->setContentsMargins(0, 0, 0, 0);
        waveLayout->setSpacing(0);
    }
    ui->waveformWidget->layout()->addWidget(waveformView);
    waveformView->installEventFilter(this);

    // Одинаковые горизонтальные отступы у волны и питч-сетки.
    if (ui->waveformLayout) {
        ui->waveformLayout->setContentsMargins(
            UiConstants::kTimelineHorizontalMarginPx,
            0,
            UiConstants::kTimelineHorizontalMarginPx,
            ui->waveformLayout->contentsMargins().bottom());
    }
    if (ui->pitchGridLayout) {
        const QMargins m = ui->pitchGridLayout->contentsMargins();
        ui->pitchGridLayout->setContentsMargins(
            UiConstants::kTimelineHorizontalMarginPx,
            m.top(),
            UiConstants::kTimelineHorizontalMarginPx,
            m.bottom());
    }

    // Create and setup PitchGridWidget
    pitchGridWidget = new PitchGridWidget(this);

    // Initialize main splitter
    mainSplitter = ui->mainSplitter;
    if (mainSplitter) {
        // Set initial splitter sizes (75% for waveform, 25% for pitch grid)
        QList<int> sizes;
        sizes << UiConstants::kDefaultSplitterWaveformHeight
              << UiConstants::kDefaultSplitterPitchGridHeight;
        mainSplitter->setSizes(sizes);

        // Set minimum sizes
        mainSplitter->setChildrenCollapsible(false);

        // Set minimum sizes programmatically for better control
        QWidget* waveformContainer = mainSplitter->widget(0);
        QWidget* pitchGridContainer = mainSplitter->widget(1);

        if (waveformContainer) {
            waveformContainer->setMinimumHeight(UiConstants::kWaveformContainerMinHeight);
        }
        if (pitchGridContainer) {
            pitchGridContainer->setMinimumHeight(UiConstants::kPitchGridContainerMinHeight);
            // По умолчанию питч-сетка видна (финальное состояние — в readSettings).
            pitchGridContainer->setVisible(true);
            mainSplitter->setChildrenCollapsible(false);
            QList<int> sizes;
            sizes << UiConstants::kDefaultSplitterWaveformHeight
                  << UiConstants::kDefaultSplitterPitchGridHeight;
            mainSplitter->setSizes(sizes);
        }

        // Connect splitter signals if needed
        connect(mainSplitter, &QSplitter::splitterMoved, this, [this]() {
            // Update any dependent UI elements when splitter moves
            update();
        });
    }

    // Синхронизируем размер такта по умолчанию между виджетами
    int defaultBeatsPerBar = 4; // Размер такта по умолчанию
    waveformView->setBeatsPerBar(defaultBeatsPerBar);
    pitchGridWidget->setBeatsPerBar(defaultBeatsPerBar);

    // Синхронизируем BPM по умолчанию между виджетами
    float defaultBPM = 120.0f; // BPM по умолчанию
    waveformView->setBPM(defaultBPM);
    pitchGridWidget->setBPM(defaultBPM);

    // Устанавливаем цветовую схему для виджетов
    QString widgetColorScheme = settings.value("colorScheme", "dark").toString();
    if (waveformView) {
        waveformView->setColorScheme(widgetColorScheme);
    }
    if (pitchGridWidget) {
        pitchGridWidget->setColorScheme(widgetColorScheme);
    }

    // Устанавливаем значения по умолчанию в UI
    ui->bpmEdit->setText(QString::number(defaultBPM, 'f', 2));
    const QList<int> bpbValues = { 4, 3, 1, 2, 6, 12 };
    for (int i = 0; i < qMin(ui->barsCombo->count(), bpbValues.size()); ++i) {
        ui->barsCombo->setItemData(i, bpbValues[i]);
    }
    ui->barsCombo->setCurrentIndex(0); // 4/4

    // Create and setup horizontal scrollbar
    horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar->setMinimum(0);
    horizontalScrollBar->setMaximum(0); // Начинаем с 0 - скроллбар не нужен при масштабе 1.0
    horizontalScrollBar->setSingleStep(10);
    horizontalScrollBar->setPageStep(100);
    horizontalScrollBar->setFixedHeight(UiConstants::kHorizontalScrollBarHeightPx);
    if (!ui->scrollBarWidget->layout()) {
        ui->scrollBarWidget->setLayout(new QHBoxLayout());
    }
    auto* scrollBarLayout = qobject_cast<QHBoxLayout*>(ui->scrollBarWidget->layout());
    scrollBarLayout->setContentsMargins(
        UiConstants::kHorizontalScrollBarHorizontalMarginPx,
        UiConstants::kHorizontalScrollBarTopMarginPx,
        UiConstants::kHorizontalScrollBarHorizontalMarginPx,
        UiConstants::kHorizontalScrollBarBottomMarginPx);
    scrollBarLayout->setSpacing(0);
    scrollBarLayout->addWidget(horizontalScrollBar);
    ui->scrollBarWidget->setMinimumHeight(UiConstants::kHorizontalScrollBarContainerHeightPx);
    ui->scrollBarWidget->setMaximumHeight(UiConstants::kHorizontalScrollBarContainerHeightPx);

    // Create and setup vertical scrollbar for pitch grid
    pitchGridVerticalScrollBar = new QScrollBar(Qt::Vertical, this);
    pitchGridVerticalScrollBar->setMinimum(0);
    pitchGridVerticalScrollBar->setMaximum(1000);
    pitchGridVerticalScrollBar->setSingleStep(10);  // Small step
    pitchGridVerticalScrollBar->setPageStep(100);   // Large step (for PageUp/PageDown)

    // Контейнер: питч-сетка на всю область, вертикальный скролл поверх слева
    pitchGridScrollContainer = new QWidget(this);
    pitchGridWidget->setParent(pitchGridScrollContainer);
    pitchGridVerticalScrollBar->setParent(pitchGridScrollContainer);
    pitchGridScrollContainer->installEventFilter(this);
    pitchGridVerticalScrollBar->raise();

    if (!ui->pitchGridTableContainer->layout()) {
        ui->pitchGridTableContainer->setLayout(new QVBoxLayout());
    }
    auto* pitchTableLayout = qobject_cast<QVBoxLayout*>(ui->pitchGridTableContainer->layout());
    pitchTableLayout->setContentsMargins(0, 0, 0, 0);
    pitchTableLayout->setSpacing(0);
    pitchTableLayout->addWidget(pitchGridScrollContainer);

    pitchGridVerticalScrollBar->setFixedWidth(UiConstants::kScrollBarWidthPx);
    layoutPitchGridScrollOverlay();
    setupPitchGridAnalyzeOverlay();
    setupKeyModulationStrip();

    // Стили скроллбаров применяются ниже (после readSettings) единым вызовом applyScrollBarStyles().

    // Initialize audio
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);

    // Setup connections after all objects are created
    setupConnections();

    // Set application icon
    QIcon appIcon(":/icons/resources/icons/logo.svg");
    setWindowIcon(appIcon);
    QApplication::setWindowIcon(appIcon);

    // Restore window state
    readSettings();

    // Применяем сохранённую цветовую схему (тёмная по умолчанию):
    // фон виджетов и стили скроллбаров вынесены в helpers / :/styles/*.qss.
    const QString currentScheme = settings.value("colorScheme", "dark").toString();
    applyWidgetBackgrounds(currentScheme);
    applyScrollBarStyles(currentScheme);

    // Initialize timer for time updates
    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    playbackTimer->setInterval(33); // ~30 fps

    // Таймер для отложенного обновления воспроизведения после перетаскивания меток
    markerPreviewTimer = new QTimer(this);
    markerPreviewTimer->setSingleShot(true);
    markerPreviewTimer->setInterval(250); // ждем паузы после перетаскивания
    connect(markerPreviewTimer, &QTimer::timeout, this, &MainWindow::updatePlaybackAfterMarkerDrag);

    // Initial window title
    updateWindowTitle();

    // Принудительно обновляем размеры виджетов
    QTimer::singleShot(100, this, [this]() {
        if (ui->waveformWidget) {
            ui->waveformWidget->updateGeometry();
            ui->waveformWidget->update();
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->updateGeometry();
            ui->pitchGridWidget->update();
        }
    });

}

void MainWindow::prepareShutdown()
{
    if (isShuttingDown) {
        return;
    }
    isShuttingDown = true;

    if (markerPreviewTimer) {
        markerPreviewTimer->stop();
    }
    if (playbackTimer) {
        playbackTimer->stop();
    }

    if (notePreviewPlayer) {
        notePreviewPlayer->stop();
    }

    // Инвалидируем фоновый preview; не ждём пул (может быть длинный PitchCorrection).
    ++markerPreviewEpoch;
    if (markerPreviewRunning) {
        markerPreviewRunning->store(false);
    }

    if (waveformView) {
        disconnect(waveformView, nullptr, this, nullptr);
    }
    if (pitchGridWidget) {
        disconnect(pitchGridWidget, nullptr, this, nullptr);
    }

    if (mediaPlayer) {
        mediaPlayer->blockSignals(true);
        disconnect(mediaPlayer, nullptr, this, nullptr);
        mediaPlayer->stop();
    }
}

MainWindow::~MainWindow()
{
    prepareShutdown();

    delete ui;
    ui = nullptr;

    // Освобождаем таймеры
    if (playbackTimer) {
        delete playbackTimer;
    }
    if (markerPreviewTimer) {
        delete markerPreviewTimer;
    }
    if (metronomeController) {
        delete metronomeController;
    }

    // Освобождаем аудио компоненты
    if (mediaPlayer) {
        delete mediaPlayer;
    }
    if (audioOutput) {
        delete audioOutput;
    }

    // Освобождаем визуальные компоненты
    if (waveformView) {
        delete waveformView;
    }
    if (pitchGridWidget) {
        delete pitchGridWidget;
    }
    if (horizontalScrollBar) {
        delete horizontalScrollBar;
    }

    if (pitchGridVerticalScrollBar) {
        delete pitchGridVerticalScrollBar;
    }

    // Освобождаем стек отмены
    if (undoStack) {
        delete undoStack;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        prepareShutdown();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // Принудительно обновляем размеры виджетов при показе окна
    QTimer::singleShot(100, this, [this]() {
        if (ui->waveformWidget) {
            ui->waveformWidget->updateGeometry();
            ui->waveformWidget->update();
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->updateGeometry();
            ui->pitchGridWidget->update();
        }

        updatePitchGridLayout();
        layoutPitchGridScrollOverlay();
        updateScrollBarTransparency();
    });
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Принудительно обновляем размеры виджетов при изменении размера окна
    QTimer::singleShot(50, this, [this]() {
        if (ui->waveformWidget) {
            ui->waveformWidget->updateGeometry();
            ui->waveformWidget->update();
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->updateGeometry();
            ui->pitchGridWidget->update();
        }

        updatePitchGridLayout();
        layoutPitchGridScrollOverlay();
        updateScrollBarTransparency();
    });
}

void MainWindow::updatePitchGridLayout()
{
    if (!mainSplitter) return;
    QWidget* pitchGridContainer = mainSplitter->widget(1);
    if (!pitchGridContainer) return;

    if (isPitchGridVisible) {
        pitchGridContainer->setVisible(true);
        mainSplitter->setChildrenCollapsible(false);
        int totalHeight = mainSplitter->height();
        if (totalHeight > 0) {
            QList<int> sizes;
            sizes << static_cast<int>(totalHeight * 0.75) << static_cast<int>(totalHeight * 0.25);
            mainSplitter->setSizes(sizes);
        }
        layoutPitchGridScrollOverlay();
        updateScrollBarTransparency();
    } else {
        pitchGridContainer->setVisible(false);
        mainSplitter->setChildrenCollapsible(true);
        QList<int> sizes;
        sizes << mainSplitter->height() << 0;
        mainSplitter->setSizes(sizes);
    }
}

void MainWindow::syncPitchGridTimelineWidth()
{
    if (!pitchGridWidget || !waveformView) {
        return;
    }

    pitchGridWidget->setTimelineReferenceWidth(waveformView->width());
    pitchGridWidget->setTimelineSampleCount(waveformView->displaySampleCount());
    pitchGridWidget->setCursorPosition(waveformView->getCursorXPosition());
    syncKeyModulationStripFromWaveform();
}

void MainWindow::syncPitchGridFromWaveform()
{
    if (!pitchGridWidget || !waveformView) {
        return;
    }

    pitchGridWidget->setAudioData(waveformView->getAudioData());
    pitchGridWidget->setSampleRate(waveformView->getSampleRate());
    pitchGridWidget->setBPM(waveformView->getBPM());
    pitchGridWidget->setBeatsPerBar(waveformView->getBeatsPerBar());
    pitchGridWidget->setGridStartSample(waveformView->getGridStartSample());
    pitchGridWidget->setZoomLevel(waveformView->getZoomLevel());
    pitchGridWidget->setHorizontalOffset(waveformView->getHorizontalOffset());
    pitchGridWidget->setPrimaryKey(currentKey);
    pitchGridWidget->setSecondaryKey(currentKey2);
    syncPitchGridTimelineWidth();
    syncKeyModulationStripFromWaveform();
    pitchGridWidget->update();
}

void MainWindow::retranslateMenus()
{
    if (fileMenu) fileMenu->setTitle(tr("&File"));
    if (editMenu) editMenu->setTitle(tr("&Edit"));
    if (viewMenu) viewMenu->setTitle(tr("&View"));
    if (themesMenu) themesMenu->setTitle(tr("Themes"));
    if (colorSchemeMenu) colorSchemeMenu->setTitle(tr("&Color Scheme"));
    if (settingsMenu) settingsMenu->setTitle(tr("&Settings"));
    if (languageMenu) languageMenu->setTitle(tr("Language"));
    if (openAct) openAct->setText(tr("&Open..."));
    if (saveAct) saveAct->setText(tr("&Save"));
    if (exitAct) exitAct->setText(tr("&Exit"));
    if (defaultThemeAct) { defaultThemeAct->setText(tr("Reset to default")); defaultThemeAct->setStatusTip(tr("Use default theme")); }
    if (darkSchemeAct) { darkSchemeAct->setText(tr("Dark")); darkSchemeAct->setStatusTip(tr("Use dark theme")); }
    if (lightSchemeAct) { lightSchemeAct->setText(tr("Light")); lightSchemeAct->setStatusTip(tr("Use light theme")); }
    if (metronomeSettingsAct) { metronomeSettingsAct->setText(tr("&Metronome Settings...")); metronomeSettingsAct->setStatusTip(tr("Metronome Settings")); }
    if (keyboardShortcutsAct) { keyboardShortcutsAct->setText(tr("&Hotkeys...")); keyboardShortcutsAct->setStatusTip(tr("Configure hotkeys")); }
    if (playPauseAct) playPauseAct->setText(tr("Play/Pause"));
    if (stopAct) stopAct->setText(tr("Stop"));
    if (metronomeAct) metronomeAct->setText(tr("Metronome"));
    if (loopStartAct) loopStartAct->setText(tr("Set loop start"));
    if (loopEndAct) loopEndAct->setText(tr("Set loop end"));
    if (togglePitchGridAct) { togglePitchGridAct->setStatusTip(tr("Toggle pitch grid visibility")); if (isPitchGridVisible) togglePitchGridAct->setText(tr("Hide Pitch Grid")); else togglePitchGridAct->setText(tr("Show Pitch Grid")); }
    if (toggleBeatWaveformAct) { toggleBeatWaveformAct->setText(tr("Beat Waveform")); toggleBeatWaveformAct->setStatusTip(tr("Toggle beat waveform overlay")); }
    if (undoAct) undoAct->setText(tr("&Undo"));
    if (redoAct) redoAct->setText(tr("&Redo"));
    if (russianAction) russianAction->setText(tr("Русский"));
    if (englishAction) englishAction->setText(tr("English"));
    if (applyTimeStretchAct) { applyTimeStretchAct->setText(tr("Apply time stretch")); applyTimeStretchAct->setStatusTip(tr("Apply time stretch with pitch compensation")); }
    if (applyPitchCorrectionAct) { applyPitchCorrectionAct->setText(tr("Apply note pitch correction")); applyPitchCorrectionAct->setStatusTip(tr("Re-render audio using the edited piano roll notes")); }
    if (waveformViewMenu) waveformViewMenu->setTitle(tr("Waveform view"));
    if (waveformPeaksAct) waveformPeaksAct->setText(tr("Wave peaks"));
    if (waveformSpectrogramAct) waveformSpectrogramAct->setText(tr("Spectrogram"));
    if (spectrogramSettingsAct) { spectrogramSettingsAct->setText(tr("Spectrogram display settings...")); spectrogramSettingsAct->setStatusTip(tr("Spectrogram options (window, bands, color)")); }
    if (pitchShiftSettingsAct) { pitchShiftSettingsAct->setText(tr("Pitch shift settings...")); pitchShiftSettingsAct->setStatusTip(tr("Granular pitch shift after stretch (Ctrl+T)")); }
    if (pitchGridAnalyzeButton) pitchGridAnalyzeButton->setText(tr("Analyze"));
    if (pitchGridAnalyzeProgress) pitchGridAnalyzeProgress->setFormat(tr("Analyzing... %p%"));
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        retranslateMenus();
    }
}

void MainWindow::readSettings()
{
    settings.beginGroup("MainWindow");

    // Восстановление геометрии окна
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    // Восстановление состояния окна (развернуто/свернуто/нормально)
    const QByteArray windowState = settings.value("windowState", QByteArray()).toByteArray();
    if (!windowState.isEmpty()) {
        restoreState(windowState);
    }

    // Восстанавливаем цветовую схему
    QString colorScheme = settings.value("colorScheme", "dark").toString();
    setColorScheme(colorScheme);

    // Восстанавливаем видимость питч-сетки (по умолчанию видна)
    isPitchGridVisible = settings.value("pitchGridVisible", true).toBool();

    // Восстанавливаем текущие тональности
    currentKey = settings.value("currentKey", QStringLiteral("C Major")).toString();
    setKey(currentKey);

    currentKey2 = settings.value("currentKey2", QString()).toString();
    setKey2(currentKey2);

    updatePitchGridLayout();

    if (togglePitchGridAct) {
        togglePitchGridAct->setText(isPitchGridVisible
            ? tr("Hide Pitch Grid")
            : tr("Show Pitch Grid"));
    }

    if (isPitchGridVisible) {
        syncPitchGridFromWaveform();
    }

    applyShortcuts();

    // Восстановление состояния сплиттера (только если питч-сетка видима)
    if (mainSplitter && isPitchGridVisible) {
        const QByteArray splitterState = settings.value("splitterState", QByteArray()).toByteArray();
        if (!splitterState.isEmpty()) {
            mainSplitter->restoreState(splitterState);
        }
    }

    settings.endGroup();
}

void MainWindow::writeSettings()
{
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    // Сохранение состояния сплиттера (только если питч-сетка видима)
    if (mainSplitter && isPitchGridVisible) {
        settings.setValue("splitterState", mainSplitter->saveState());
    }

    // Сохраняем видимость питч-сетки
    settings.setValue("pitchGridVisible", isPitchGridVisible);

    // Сохраняем текущие тональности
    settings.setValue("currentKey", currentKey);
    settings.setValue("currentKey2", currentKey2);

    settings.endGroup();
}

void MainWindow::setupConnections()
{
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::playAudio);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopAudio);
    connect(ui->bpmEdit, &QLineEdit::editingFinished, this, &MainWindow::updateBPM);
    connect(ui->barsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QVariant v = ui->barsCombo->currentData();
        int bpb = v.isValid() ? v.toInt() : 4;
        if (bpb < 1 || bpb > 32) bpb = 4;
        if (waveformView) {
            waveformView->setBeatsPerBar(bpb);
            waveformView->update();
        }
        if (pitchGridWidget) {
            pitchGridWidget->setBeatsPerBar(bpb);
            pitchGridWidget->update();
        }
        updateTimeLabel(mediaPlayer ? mediaPlayer->position() : currentPosition);
        QString text = ui->barsCombo->currentText();
        statusBar()->showMessage(tr("Time signature set to %1").arg(text), 2000);
    });
    // Временно закомментировано до пересборки UI
    // После пересборки проекта в Qt Creator раскомментировать эти строки:
    // connect(ui->bpmIncreaseButton, &QPushButton::clicked, this, &MainWindow::increaseBPM);
    // connect(ui->bpmDecreaseButton, &QPushButton::clicked, this, &MainWindow::decreaseBPM);
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, &MainWindow::updatePlaybackPosition);
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState && isPlaying) {
            // Воспроизведение завершилось
            isPlaying = false;
            ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
            playbackTimer->stop();

            // Останавливаем метроном
            if (metronomeController) {
                metronomeController->setPlaying(false);
                metronomeController->reset();
            }

            statusBar()->showMessage(tr("Playback completed"), 2000);
        }
    });

    // Связываем горизонтальный скроллбар с WaveformView и PitchGridWidget
    connect(horizontalScrollBar, &QScrollBar::valueChanged, this,
        [this](int value) {
            if (waveformView && horizontalScrollBar->maximum() > 0) {
                float offset = float(value) / float(horizontalScrollBar->maximum());
                offset = qBound(0.0f, offset, 1.0f);
                waveformView->setHorizontalOffset(offset);
            }
            if (pitchGridWidget && horizontalScrollBar->maximum() > 0) {
                float offset = float(value) / float(horizontalScrollBar->maximum());
                offset = qBound(0.0f, offset, 1.0f);
                pitchGridWidget->setHorizontalOffset(offset);
            }
        });



    // Связываем вертикальный скроллбар для PitchGridWidget
    connect(pitchGridVerticalScrollBar, &QScrollBar::valueChanged, this,
        [this](int value) {
            if (pitchGridWidget) {
                float offset = float(value) / float(pitchGridVerticalScrollBar->maximum());
                offset = qBound(0.0f, offset, 1.0f);
                pitchGridWidget->setVerticalOffset(offset);
            }
        });

    // Обновляем горизонтальный скроллбар при изменении масштаба
    connect(waveformView, &WaveformView::zoomChanged, this,
        [this](float zoom) {
            updateHorizontalScrollBar(zoom);
            if (pitchGridWidget) {
                pitchGridWidget->setZoomLevel(zoom);
                syncPitchGridTimelineWidth();
                updateScrollBarTransparency();
            }
        });

    // Обновляем горизонтальный скроллбар при изменении смещения
    connect(waveformView, &WaveformView::horizontalOffsetChanged, this,
        [this](float offset) {
            updateHorizontalScrollBarFromOffset(offset);
            if (pitchGridWidget) {
                pitchGridWidget->setHorizontalOffset(offset);
                syncPitchGridTimelineWidth();
                updateScrollBarTransparency();
            }
        });

    connect(waveformView, &WaveformView::gridStartChanged, this,
        [this](qint64 sample) {
            if (pitchGridWidget) {
                pitchGridWidget->setGridStartSample(sample);
            }
            updateTimeLabel(mediaPlayer ? mediaPlayer->position() : currentPosition);
        });

    connect(waveformView, &WaveformView::markerDragFinished, this,
        &MainWindow::scheduleMarkerPlaybackPreview);

    connect(waveformView, &WaveformView::markersChanged, this,
        [this]() {
            hasUnsavedChanges = true;
            if (pitchGridWidget && waveformView) {
                pitchGridWidget->setTimelineSampleCount(waveformView->displaySampleCount());
            }
            scheduleMarkerPlaybackPreview();
        });

    // Подключаем сигнал изменения позиции от WaveformView
    connect(waveformView, &WaveformView::positionChanged, this,
        [this](qint64 msPosition) {
            if (waveformView) {
                // Теперь WaveformView отправляет позицию в миллисекундах
                mediaPlayer->setPosition(msPosition);
                updateTimeLabel(msPosition);
                // Обновляем позицию каретки в PitchGridWidget
                if (pitchGridWidget) {
                    float cursorX = waveformView->getCursorXPosition();
                    pitchGridWidget->setCursorPosition(cursorX);
                }
                // Обновляем прозрачность скроллбара
                updateScrollBarTransparency();
            }
        });

    // Подключаем сигналы от PitchGridWidget
    if (pitchGridWidget) {
        // Синхронизация позиции воспроизведения
        connect(pitchGridWidget, &PitchGridWidget::positionChanged, this,
            [this](qint64 msPosition) {
                mediaPlayer->setPosition(msPosition);
                updateTimeLabel(msPosition);
                if (waveformView) {
                    waveformView->setPlaybackPosition(msPosition);
                }
            });

        connect(pitchGridWidget, &PitchGridWidget::horizontalOffsetChanged, this,
            [this](float offset) {
                if (waveformView && !qFuzzyCompare(waveformView->getHorizontalOffset(), offset)) {
                    waveformView->setHorizontalOffset(offset);
                }
            });

        connect(pitchGridWidget, &PitchGridWidget::verticalOffsetChanged, this,
            [this](float offset) {
                if (!pitchGridVerticalScrollBar) {
                    return;
                }
                const int maxValue = pitchGridVerticalScrollBar->maximum();
                if (maxValue <= 0) {
                    return;
                }
                pitchGridVerticalScrollBar->blockSignals(true);
                pitchGridVerticalScrollBar->setValue(qBound(0, int(offset * maxValue), maxValue));
                pitchGridVerticalScrollBar->blockSignals(false);
            });

        connect(pitchGridWidget, &PitchGridWidget::timelineZoomRequested, this,
            [this](int angleDeltaY, float timelinePixelX) {
                if (waveformView) {
                    waveformView->zoomAtPixelX(angleDeltaY, timelinePixelX);
                }
            });

        // Правка высоты ноты на пианоролле → undo-команда
        connect(pitchGridWidget, &PitchGridWidget::notePitchEdited,
                this, &MainWindow::onNotePitchEdited);

        // Зацикленное прослушивание удерживаемой ноты
        connect(pitchGridWidget, &PitchGridWidget::notePreviewRequested,
                this, &MainWindow::onNotePreviewRequested);
        connect(pitchGridWidget, &PitchGridWidget::notePreviewPitchChanged,
                this, &MainWindow::onNotePreviewPitchChanged);
        connect(pitchGridWidget, &PitchGridWidget::notePreviewStopped,
                this, &MainWindow::stopNotePreview);

        // Warp нот при изменении меток (drag меток stretch)
        connect(waveformView, &WaveformView::markersChanged,
                this, &MainWindow::refreshPitchGridNotes);
        connect(waveformView, &WaveformView::markerDragFinished,
                this, &MainWindow::refreshPitchGridNotes);

        // Синхронизация с WaveformView
        connect(waveformView, &WaveformView::positionChanged, this,
            [this](qint64 msPosition) {
                if (pitchGridWidget) {
                    pitchGridWidget->setPlaybackPosition(msPosition);
                    // Передаем позицию каретки в пикселях для точной синхронизации
                    float cursorX = waveformView->getCursorXPosition();
                    pitchGridWidget->setCursorPosition(cursorX);
                }
            });

    }

    // Добавляем кнопки загрузки и сохранения в тулбар
    ui->loadButton->setIcon(QIcon(":/icons/resources/icons/load.svg"));
    ui->saveButton->setIcon(QIcon(":/icons/resources/icons/save.svg"));
    connect(ui->loadButton, &QPushButton::clicked, this, &MainWindow::openAudioFile);
    connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::saveAudioFile);

    // Подключаем метроном и циклы
    connect(ui->metronomeButton, &QPushButton::clicked, this, &MainWindow::toggleMetronome);
    connect(ui->loopStartButton, &QPushButton::clicked, this, &MainWindow::setLoopStart);
    connect(ui->loopEndButton, &QPushButton::clicked, this, &MainWindow::setLoopEnd);
    connect(ui->loopButton, &QPushButton::clicked, this, &MainWindow::toggleLoop);

    // Авто-метки по транзиентам (Onset detection — пока заглушка)
    if (ui->onsetDetectButton) {
        ui->onsetDetectButton->setToolTip(tr("Auto-place markers from transients (LMMS onset detection)"));
        connect(ui->onsetDetectButton, &QPushButton::clicked,
                this, &MainWindow::createOnsetMarkersAuto);
    }

    // Авто-привязка всех меток к тактовой сетке (Beat Grid)
    if (ui->gridMarkersButton) {
        ui->gridMarkersButton->setToolTip(
            tr("Snap all markers to BPM grid (bar subdivisions)"));
        connect(ui->gridMarkersButton, &QPushButton::clicked,
                this, &MainWindow::snapAllMarkersToGrid);
    }
    if (ui->gridBackButton) {
        ui->gridBackButton->setToolTip(tr("Shift the bar grid one beat backward (Shift — with markers)\n"
                                          "Shift + LMB drag on waveform — fine grid adjustment"));
        connect(ui->gridBackButton, &QPushButton::clicked,
                this, &MainWindow::shiftBeatGridBackward);
    }
    if (ui->gridForwardButton) {
        ui->gridForwardButton->setToolTip(tr("Shift the bar grid one beat forward (Shift — with markers)\n"
                                             "Shift + LMB drag on waveform — fine grid adjustment"));
        connect(ui->gridForwardButton, &QPushButton::clicked,
                this, &MainWindow::shiftBeatGridForward);
    }

    // Подключаем правую кнопку мыши для удаления меток цикла
    ui->loopStartButton->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->loopEndButton->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->loopStartButton, &QPushButton::customContextMenuRequested, this, [this]() {
        clearLoopStart();
    });

    connect(ui->loopEndButton, &QPushButton::customContextMenuRequested, this, [this]() {
        clearLoopEnd();
    });

    // Добавляем подсказки для кнопок цикла
    ui->loopStartButton->setToolTip(tr("LMB: Set point A (loop start)\nRMB: Remove point A\nA: Set point A\nShift+A: Remove point A"));
    ui->loopEndButton->setToolTip(tr("LMB: Set point B (loop end)\nRMB: Remove point B\nB: Set point B\nShift+B: Remove point B"));

    connect(ui->loopStartButton, &QPushButton::pressed, this, [this]() {
        if (loopStartPosition > 0) {
            statusBar()->showMessage(tr("Point A: %1").arg(TimeUtils::formatTime(loopStartPosition)), 2000);
        }
    });

    connect(ui->loopEndButton, &QPushButton::pressed, this, [this]() {
        if (loopEndPosition > 0) {
            statusBar()->showMessage(tr("Point B: %1").arg(TimeUtils::formatTime(loopEndPosition)), 2000);
        }
    });

    // Инициализация метронома
    metronomeController = new MetronomeController(this);

    // Загружаем настройки громкости метронома
    metronomeController->setStrongBeatVolume(settings.value("Metronome/StrongBeatVolume", 100).toInt());
    metronomeController->setWeakBeatVolume(settings.value("Metronome/WeakBeatVolume", 90).toInt());

    // Устанавливаем начальный BPM
    float defaultBPM = 120.0f;
    metronomeController->setBPM(defaultBPM);

    // Инициализация циклов
    isLoopEnabled = false;
    loopStartPosition = 0;
    loopEndPosition = 0;

    // Подключаем сигналы стека отмены
    connect(undoStack, &QUndoStack::canUndoChanged, undoAct, &QAction::setEnabled);
    connect(undoStack, &QUndoStack::canRedoChanged, redoAct, &QAction::setEnabled);
    connect(undoStack, &QUndoStack::indexChanged, this, &MainWindow::onUndoStackChanged);

    // Подключаем контекстные меню для полей ввода тональности
    ui->keyInput->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->keyInput, &QLineEdit::customContextMenuRequested, this, &MainWindow::showKeyContextMenu);
    ui->keyInput->installEventFilter(this);

    ui->keyInput2->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->keyInput2, &QLineEdit::customContextMenuRequested, this, &MainWindow::showKeyContextMenu2);
    ui->keyInput2->installEventFilter(this);

    // Добавляем горячие клавиши
    setupShortcuts();

    // Принудительно обновляем размеры виджетов после настройки соединений
    QTimer::singleShot(200, this, [this]() {
        if (ui->waveformWidget) {
            ui->waveformWidget->updateGeometry();
            ui->waveformWidget->update();
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->updateGeometry();
            ui->pitchGridWidget->update();
        }
    });
}

void MainWindow::playAudio()
{
    if (!isPlaying) {
        isPlaying = true;
        ui->playButton->setIcon(QIcon(":/icons/resources/icons/pause.svg"));
        mediaPlayer->play();
        playbackTimer->start();
        statusBar()->showMessage(tr("Playing..."));

        // Обновляем состояние метронома
        if (metronomeController) {
            metronomeController->setPlaying(true);
        }
    } else {
        isPlaying = false;
        ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
        mediaPlayer->pause();
        playbackTimer->stop();
        statusBar()->showMessage(tr("Paused"));

        // Обновляем состояние метронома
        if (metronomeController) {
            metronomeController->setPlaying(false);
        }
    }
}

void MainWindow::stopAudio()
{
    isPlaying = false;
    // Останавливаем метроном при остановке воспроизведения
    if (metronomeController) {
        metronomeController->setPlaying(false);
        metronomeController->reset();
    }
    currentPosition = 0;
    ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
    mediaPlayer->stop();
    playbackTimer->stop();
    ui->timeLabel->setText(formatTimeAndBars(0));
    statusBar()->showMessage(tr("Stopped"));
}

void MainWindow::updateTime()
{
    if (isShuttingDown || !ui || !isPlaying) {
        return;
    }
    currentPosition = mediaPlayer->position();
    ui->timeLabel->setText(formatTimeAndBars(currentPosition));
}

void MainWindow::updateBPM()
{
    bool ok;
    float bpm = ui->bpmEdit->text().toFloat(&ok);
    if (ok && bpm > 0.0f && bpm <= 9999.99f) {
        waveformView->setBPM(bpm);
        // Синхронизируем BPM с PitchGridWidget
        if (pitchGridWidget) {
            pitchGridWidget->setBPM(bpm);
        }
        // Обновляем BPM в метрономе
        if (metronomeController) {
            metronomeController->setBPM(bpm);
        }
        // Принудительно обновляем отображение тактов
        waveformView->update();
        if (pitchGridWidget) {
            pitchGridWidget->update();
        }
        statusBar()->showMessage(tr("BPM set to: %1").arg(bpm), 2000);
    } else {
        ui->bpmEdit->setText("120.00");
        statusBar()->showMessage(tr("Invalid BPM value (valid range: 0.01 - 9999.99)"), 3000);
    }
}

void MainWindow::increaseBPM()
{
    bool ok;
    float currentBpm = ui->bpmEdit->text().toFloat(&ok);
    if (ok && currentBpm > 0.0f) {
        float newBpm = currentBpm + 1.0f;
        if (newBpm <= 9999.99f) {
            ui->bpmEdit->setText(QString::number(newBpm, 'f', 2));
            updateBPM();
        }
    }
}

void MainWindow::decreaseBPM()
{
    bool ok;
    float currentBpm = ui->bpmEdit->text().toFloat(&ok);
    if (ok && currentBpm > 0.0f) {
        float newBpm = currentBpm - 1.0f;
        if (newBpm >= 0.01f) {
            ui->bpmEdit->setText(QString::number(newBpm, 'f', 2));
            updateBPM();
        }
    }
}

void MainWindow::updateTimeLabel(qint64 msPosition)
{
    if (isShuttingDown || !ui) {
        return;
    }
    ui->timeLabel->setText(formatTimeAndBars(msPosition));

    // Показываем информацию о цикле в статусной строке
    if (isLoopEnabled && loopStartPosition > 0 && loopEndPosition > 0) {
        QString loopInfo = tr("Loop: %1 - %2").arg(TimeUtils::formatTime(loopStartPosition)).arg(TimeUtils::formatTime(loopEndPosition));
        if (msPosition >= loopStartPosition && msPosition <= loopEndPosition) {
            statusBar()->showMessage(loopInfo, 1000);
        }
    }
}

void MainWindow::updatePlaybackPosition(qint64 position)
{
    if (isShuttingDown || !ui) {
        return;
    }
    if (waveformView) {
        // Передаем позицию в миллисекундах, как ожидает WaveformView
        waveformView->setPlaybackPosition(position);
        updateTimeLabel(position);
        updateLoopPoints();

        // Показываем информацию об активном сегменте в статусной строке (если есть метки)
        if (!waveformView->getMarkers().isEmpty()) {
            WaveformView::ActiveSegmentInfo segmentInfo = waveformView->getActiveSegmentInfo();
            if (segmentInfo.isValid) {
                QString segmentText = tr("Segment: %1 - %2 | Ratio: %3")
                    .arg(segmentInfo.startMarkerTime)
                    .arg(segmentInfo.endMarkerTime)
                    .arg(segmentInfo.stretchFactor, 0, 'f', 3);
                statusBar()->showMessage(segmentText, 100);
            }
        }
    }

    // Обновляем позицию в PitchGridWidget
    if (pitchGridWidget) {
        pitchGridWidget->setPlaybackPosition(position);
        // Обновляем позицию каретки в пикселях для точной синхронизации
        if (waveformView) {
            float cursorX = waveformView->getCursorXPosition();
            pitchGridWidget->setCursorPosition(cursorX);
        }
    }
}

void MainWindow::createActions()
{
    // File actions
    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openAudioFile);

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveAudioFile);

    exitAct = new QAction(tr("&Exit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Theme actions
    defaultThemeAct = new QAction(tr("Reset to default"), this);
    defaultThemeAct->setStatusTip(tr("Use default theme"));
    connect(defaultThemeAct, &QAction::triggered, this, [this]() { setTheme("default"); });

    // Color scheme actions
    darkSchemeAct = new QAction(tr("Dark"), this);
    darkSchemeAct->setStatusTip(tr("Use dark theme"));
    connect(darkSchemeAct, &QAction::triggered, this, [this]() { setColorScheme("dark"); });

    lightSchemeAct = new QAction(tr("Light"), this);
    lightSchemeAct->setStatusTip(tr("Use light theme"));
    connect(lightSchemeAct, &QAction::triggered, this, [this]() { setColorScheme("light"); });

    // Settings actions
    metronomeSettingsAct = new QAction(tr("&Metronome Settings..."), this);
    metronomeSettingsAct->setStatusTip(tr("Metronome Settings"));
    connect(metronomeSettingsAct, &QAction::triggered, this, &MainWindow::showMetronomeSettings);

    keyboardShortcutsAct = new QAction(tr("&Hotkeys..."), this);
    keyboardShortcutsAct->setStatusTip(tr("Configure hotkeys"));
    connect(keyboardShortcutsAct, &QAction::triggered, this, &MainWindow::showKeyboardShortcuts);

    // Playback actions
    playPauseAct = new QAction(tr("Play/Pause"), this);
    playPauseAct->setShortcut(QKeySequence(Qt::Key_Space));
    connect(playPauseAct, &QAction::triggered, this, &MainWindow::playAudio);

    stopAct = new QAction(tr("Stop"), this);
    stopAct->setShortcut(QKeySequence(Qt::Key_S));
    connect(stopAct, &QAction::triggered, this, &MainWindow::stopAudio);

    // Metronome action
    metronomeAct = new QAction(tr("Metronome"), this);
    metronomeAct->setShortcut(QKeySequence(Qt::Key_T));
    metronomeAct->setCheckable(true);
    connect(metronomeAct, &QAction::triggered, this, &MainWindow::toggleMetronome);

    // Loop actions
    loopStartAct = new QAction(tr("Set loop start"), this);
    loopStartAct->setShortcut(QKeySequence(Qt::Key_A));
    connect(loopStartAct, &QAction::triggered, this, &MainWindow::setLoopStart);

    loopEndAct = new QAction(tr("Set loop end"), this);
    loopEndAct->setShortcut(QKeySequence(Qt::Key_B));
    connect(loopEndAct, &QAction::triggered, this, &MainWindow::setLoopEnd);

    // Pitch grid toggle (Ctrl+G)
    togglePitchGridAct = new QAction(tr("Hide Pitch Grid"), this);
    togglePitchGridAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    togglePitchGridAct->setStatusTip(tr("Toggle pitch grid visibility"));
    connect(togglePitchGridAct, &QAction::triggered, this, &MainWindow::togglePitchGrid);

    // Beat deviations toggle action
    toggleBeatWaveformAct = new QAction(tr("Beat Waveform"), this);
    toggleBeatWaveformAct->setCheckable(true);
    toggleBeatWaveformAct->setChecked(true); // Включено по умолчанию
    toggleBeatWaveformAct->setStatusTip(tr("Toggle beat waveform overlay"));
    connect(toggleBeatWaveformAct, &QAction::triggered, this, &MainWindow::toggleBeatWaveform);

    // Waveform view mode actions
    waveformPeaksAct = new QAction(tr("Wave peaks"), this);
    waveformPeaksAct->setCheckable(true);
    waveformPeaksAct->setChecked(true);
    connect(waveformPeaksAct, &QAction::triggered, this, [this]() {
        if (waveformView) {
            waveformView->setWaveformRenderMode(WaveformView::WaveformRenderMode::Peaks);
            waveformPeaksAct->setChecked(true);
            waveformSpectrogramAct->setChecked(false);
        }
    });

    waveformSpectrogramAct = new QAction(tr("Spectrogram"), this);
    waveformSpectrogramAct->setCheckable(true);
    connect(waveformSpectrogramAct, &QAction::triggered, this, [this]() {
        if (waveformView) {
            waveformView->setWaveformRenderMode(WaveformView::WaveformRenderMode::Spectrogram);
            waveformPeaksAct->setChecked(false);
            waveformSpectrogramAct->setChecked(true);
        }
    });

    // Edit actions - use QUndoStack's built-in actions
    undoAct = undoStack->createUndoAction(this);
    undoAct->setText(tr("&Undo"));
    undoAct->setShortcuts(QKeySequence::Undo);

    redoAct = undoStack->createRedoAction(this);
    redoAct->setText(tr("&Redo"));
    redoAct->setShortcuts(QKeySequence::Redo);

    // Language actions
    russianAction = new QAction(tr("Русский"), this);
    russianAction->setCheckable(true);
    russianAction->setChecked(true);
    connect(russianAction, &QAction::triggered, this, &MainWindow::setRussianLanguage);

    englishAction = new QAction(tr("English"), this);
    englishAction->setCheckable(true);
    connect(englishAction, &QAction::triggered, this, &MainWindow::setEnglishLanguage);

    // Spectrogram settings action
    spectrogramSettingsAct = new QAction(tr("Spectrogram display settings..."), this);
    spectrogramSettingsAct->setStatusTip(tr("Spectrogram options (window, bands, color)"));
    connect(spectrogramSettingsAct, &QAction::triggered, this, [this]() {
        if (!spectrogramSettingsDialog) {
            spectrogramSettingsDialog = new SpectrogramSettingsDialog(this);
            connect(spectrogramSettingsDialog, &SpectrogramSettingsDialog::settingsChanged,
                    this, [this](const WaveformView::SpectrogramSettings& s) {
                if (waveformView) {
                    waveformView->setSpectrogramSettings(s);
                }
            });
        }
        if (waveformView) {
            spectrogramSettingsDialog->setSettings(waveformView->getSpectrogramSettings());
        }
        spectrogramSettingsDialog->show();
        spectrogramSettingsDialog->raise();
        spectrogramSettingsDialog->activateWindow();
    });
    spectrogramSettingsDialog = nullptr;

    // Pitch shift settings action
    pitchShiftSettingsAct = new QAction(tr("Pitch shift settings..."), this);
    pitchShiftSettingsAct->setStatusTip(tr("Granular pitch shift after stretch (Ctrl+T)"));
    connect(pitchShiftSettingsAct, &QAction::triggered, this, [this]() {
        if (!pitchShiftSettingsDialog) {
            pitchShiftSettingsDialog = new PitchShiftSettingsDialog(this);
            connect(pitchShiftSettingsDialog, &PitchShiftSettingsDialog::paramsChanged,
                    this, [this](const GranularEngine::Params& p) {
                pitchShiftParams = p;
            });
        }
        pitchShiftSettingsDialog->setParams(pitchShiftParams);
        pitchShiftSettingsDialog->show();
        pitchShiftSettingsDialog->raise();
        pitchShiftSettingsDialog->activateWindow();
    });
    pitchShiftSettingsDialog = nullptr;

    // Time stretch action
    applyTimeStretchAct = new QAction(tr("Apply time stretch"), this);
    applyTimeStretchAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    applyTimeStretchAct->setStatusTip(tr("Apply time stretch with pitch compensation"));
    connect(applyTimeStretchAct, &QAction::triggered, this, &MainWindow::applyTimeStretch);

    applyPitchCorrectionAct = new QAction(tr("Apply note pitch correction"), this);
    applyPitchCorrectionAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    applyPitchCorrectionAct->setStatusTip(tr("Re-render audio using the edited piano roll notes"));
    connect(applyPitchCorrectionAct, &QAction::triggered, this, &MainWindow::applyPitchCorrection);
}

void MainWindow::createMenus()
{
    // File menu
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    // Edit menu
    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(applyTimeStretchAct);
    editMenu->addAction(applyPitchCorrectionAct);
    editMenu->addSeparator();

    // View menu
    viewMenu = menuBar()->addMenu(tr("&View"));

    // Themes submenu in View menu
    themesMenu = viewMenu->addMenu(tr("Themes"));
    themesMenu->addAction(defaultThemeAct);

    // Color scheme submenu in View menu
    colorSchemeMenu = viewMenu->addMenu(tr("&Color Scheme"));
    colorSchemeMenu->addAction(darkSchemeAct);
    colorSchemeMenu->addAction(lightSchemeAct);

    // Waveform view submenu
    waveformViewMenu = viewMenu->addMenu(tr("Waveform view"));
    waveformViewMenu->addAction(waveformPeaksAct);
    waveformViewMenu->addAction(waveformSpectrogramAct);

    // Add pitch grid toggle to View menu
    viewMenu->addSeparator();
    viewMenu->addAction(togglePitchGridAct);
    viewMenu->addSeparator();
    viewMenu->addAction(toggleBeatWaveformAct);

    // Settings menu (last)
    settingsMenu = menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(metronomeSettingsAct);
    settingsMenu->addAction(keyboardShortcutsAct);
    settingsMenu->addSeparator();
    settingsMenu->addAction(spectrogramSettingsAct);
    settingsMenu->addAction(pitchShiftSettingsAct);

    // Language submenu in Settings menu
    languageMenu = settingsMenu->addMenu(tr("Language"));
    languageMenu->addAction(russianAction);
    languageMenu->addAction(englishAction);
}

void MainWindow::setupShortcuts()
{
    // Дополнительная клавиша для воспроизведения/паузы (ключ задаётся в applyShortcuts)
    playShortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(playShortcut, &QShortcut::activated, this, &MainWindow::playAudio);

    shiftAShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_A), this);
    shiftAShortcut->setContext(Qt::ApplicationShortcut);
    connect(shiftAShortcut, &QShortcut::activated, this, &MainWindow::clearLoopStart);

    shiftBShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_B), this);
    shiftBShortcut->setContext(Qt::ApplicationShortcut);
    connect(shiftBShortcut, &QShortcut::activated, this, &MainWindow::clearLoopEnd);

    // Добавление метки растяжения только здесь (настраиваемая клавиша AddMarker), без дублирования в keyPressEvent / WaveformView
    markerShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    markerShortcut->setContext(Qt::ApplicationShortcut);
    connect(markerShortcut, &QShortcut::activated, this, [this]() {
        if (waveformView) {
            qint64 playbackPos = waveformView->getPlaybackPosition();
            qint64 samplePos = (playbackPos * waveformView->getSampleRate()) / 1000;
            waveformView->addMarker(samplePos);
            statusBar()->showMessage(tr("Marker added"), 2000);
        }
    });

    addAction(playPauseAct);
    addAction(stopAct);
    addAction(metronomeAct);
    addAction(loopStartAct);
    addAction(loopEndAct);
    addAction(togglePitchGridAct);

    applyShortcuts();
}

void MainWindow::applyShortcuts()
{
    settings.beginGroup("Shortcuts");

    auto key = [this](const QString& id, const QKeySequence& defaultSeq) -> QKeySequence {
        QString s = settings.value(id, QString()).toString();
        return s.isEmpty() ? defaultSeq : QKeySequence::fromString(s);
    };

    if (openAct)    openAct->setShortcut(key("Open", QKeySequence::Open));
    if (saveAct)    saveAct->setShortcut(key("Save", QKeySequence::Save));
    if (exitAct)    exitAct->setShortcut(key("Exit", QKeySequence::Quit));
    const QKeySequence playKey = key("Play", QKeySequence(Qt::Key_Space));
    if (playPauseAct) {
        playPauseAct->setShortcut(playKey);
        playPauseAct->setShortcutContext(Qt::ApplicationShortcut);
    }
    // playPauseAct уже обрабатывает Play; дублирование на playShortcut давало двойной toggle по Space
    if (playShortcut) {
        playShortcut->setKey(QKeySequence());
    }
    if (stopAct) {
        stopAct->setShortcut(key("Stop", QKeySequence(Qt::Key_S)));
        stopAct->setShortcutContext(Qt::ApplicationShortcut);
    }
    if (metronomeAct) metronomeAct->setShortcut(key("Metronome", QKeySequence(Qt::Key_T)));
    if (loopStartAct) loopStartAct->setShortcut(key("LoopStart", QKeySequence(Qt::Key_A)));
    if (loopEndAct)   loopEndAct->setShortcut(key("LoopEnd", QKeySequence(Qt::Key_B)));
    if (shiftAShortcut) shiftAShortcut->setKey(key("ClearLoopA", QKeySequence(Qt::SHIFT | Qt::Key_A)));
    if (shiftBShortcut) shiftBShortcut->setKey(key("ClearLoopB", QKeySequence(Qt::SHIFT | Qt::Key_B)));
    if (undoAct)    undoAct->setShortcut(key("Undo", QKeySequence::Undo));
    if (redoAct)    redoAct->setShortcut(key("Redo", QKeySequence::Redo));
    if (togglePitchGridAct) togglePitchGridAct->setShortcut(key("PitchGrid", QKeySequence(Qt::CTRL | Qt::Key_G)));
    if (markerShortcut) markerShortcut->setKey(key("AddMarker", QKeySequence(Qt::Key_M)));
    if (applyTimeStretchAct) applyTimeStretchAct->setShortcut(key("TimeStretch", QKeySequence(Qt::CTRL | Qt::Key_T)));
    if (applyPitchCorrectionAct) applyPitchCorrectionAct->setShortcut(key("PitchCorrection", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T)));

    settings.endGroup();
}

void MainWindow::showKeyboardShortcuts()
{
    ShortcutsDialog dialog(this);
    connect(&dialog, &ShortcutsDialog::shortcutsChanged, this, &MainWindow::applyShortcuts);
    dialog.loadFromSettings();
    dialog.exec();
}

void MainWindow::setLoopStart()
{
    if (waveformView && mediaPlayer) {
        loopStartPosition = mediaPlayer->position();
        waveformView->setLoopStart(loopStartPosition);

        // Визуально показываем, что точка A установлена
        ui->loopStartButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border: 2px solid #45a049; }");

        statusBar()->showMessage(tr("Point A (loop start) set: %1").arg(TimeUtils::formatTime(loopStartPosition)), 3000);

        // Если точка B уже установлена, показываем сообщение в статусбаре
        if (loopEndPosition > 0 && loopEndPosition > loopStartPosition) {
            statusBar()->showMessage(tr("Loop ready! Press Loop button to enable."), 3000);
        }
    }
}

void MainWindow::setLoopEnd()
{
    if (waveformView && mediaPlayer) {
        loopEndPosition = mediaPlayer->position();
        waveformView->setLoopEnd(loopEndPosition);

        // Проверяем, что точка B больше точки A
        if (loopEndPosition <= loopStartPosition) {
            statusBar()->showMessage(tr("Error: Point B must be greater than point A!"), 3000);
            loopEndPosition = 0;
            return;
        }

        // Визуально показываем, что точка B установлена
        ui->loopEndButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; border: 2px solid #da190b; }");

        statusBar()->showMessage(tr("Point B (loop end) set: %1").arg(TimeUtils::formatTime(loopEndPosition)), 3000);

        // Если точка A уже установлена, показываем сообщение в статусбаре
        if (loopStartPosition > 0) {
            statusBar()->showMessage(tr("Loop ready! Press Loop button to enable."), 3000);
        }
    }
}

void MainWindow::clearLoopStart()
{
    qDebug() << "clearLoopStart() called";
    if (waveformView) {
        loopStartPosition = 0;
        waveformView->setLoopStart(0);

        // Сбрасываем визуальное состояние кнопки
        ui->loopStartButton->setStyleSheet("");

        // Если цикл был включен, выключаем его
        if (isLoopEnabled) {
            isLoopEnabled = false;
            ui->loopButton->setChecked(false);
            ui->loopButton->setStyleSheet("");
        }

        statusBar()->showMessage(tr("Point A (loop start) removed"), 2000);
        qDebug() << "Loop start cleared successfully";
    } else {
        qDebug() << "waveformView is null";
    }
}

void MainWindow::clearLoopEnd()
{
    qDebug() << "clearLoopEnd() called";
    if (waveformView) {
        loopEndPosition = 0;
        waveformView->setLoopEnd(0);

        // Сбрасываем визуальное состояние кнопки
        ui->loopEndButton->setStyleSheet("");

        // Если цикл был включен, выключаем его
        if (isLoopEnabled) {
            isLoopEnabled = false;
            ui->loopButton->setChecked(false);
            ui->loopButton->setStyleSheet("");
        }

        statusBar()->showMessage(tr("Point B (loop end) removed"), 2000);
        qDebug() << "Loop end cleared successfully";
    } else {
        qDebug() << "waveformView is null";
    }
}

bool MainWindow::maybeSave()
{
    if (!hasUnsavedChanges)
        return true;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("DONTFLOAT"));
    msgBox.setText(tr("The audio file has unsaved changes.\n"
                     "Do you want to save the changes?"));
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton* saveBtn = msgBox.addButton(tr("Save"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Don't save"), QMessageBox::DestructiveRole);
    QPushButton* cancelBtn = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(saveBtn);
    msgBox.setEscapeButton(cancelBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == saveBtn)
        return doSaveAudioFile();
    if (msgBox.clickedButton() == cancelBtn)
        return false;
    return true;
}

void MainWindow::resetAudioState()
{
    // Останавливаем воспроизведение
    stopAudio();

    // Сбрасываем состояние метронома
    if (metronomeController && metronomeController->isEnabled()) {
        metronomeController->setEnabled(false);
        ui->metronomeButton->setChecked(false);
    }

    // Сбрасываем состояние цикла
    if (isLoopEnabled) {
        toggleLoop();
    }

    // Сбрасываем точки цикла
    loopStartPosition = 0;
    loopEndPosition = 0;

    // Сбрасываем визуальное оформление кнопок
    ui->loopStartButton->setStyleSheet("");
    ui->loopEndButton->setStyleSheet("");
    ui->loopButton->setStyleSheet("");
    ui->loopButton->setChecked(false);

    // Сбрасываем позицию воспроизведения
    currentPosition = 0;

    // Обновляем интерфейс
    ui->timeLabel->setText(formatTimeAndBars(0));
    waveformView->setPlaybackPosition(0);

    // Удаляем старые метки при загрузке нового трека
    if (waveformView) {
        waveformView->clearMarkers();
    }

    if (undoStack) {
        undoStack->clear();
    }

    // Сбрасываем результаты анализа нот прошлого файла
    stopNotePreview();
    abortPitchAnalysis();
    basePitchNotes.clear();
    if (pitchGridWidget) {
        pitchGridWidget->clearNotes();
    }

    hidePitchGridAnalyzeOverlay();

    // Сброс второй тональности — поле модуляции покажется только после анализа
    setKey2(QString());
    lastPerBarKey = KeyAnalyzer::PerBarKeyResult();
    if (keyModulationStrip) {
        keyModulationStrip->clearRegions();
    }
}

void MainWindow::openAudioFile()
{
    // Проверяем, есть ли несохраненные изменения
    if (!maybeSave())
        return;

    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open Audio File"), "",
        tr("Audio Files (*.wav *.mp3 *.flac);;All Files (*)"));

    if (!fileName.isEmpty()) {
        // Останавливаем воспроизведение и сбрасываем состояние
        resetAudioState();

        // Если есть текущий файл, освобождаем ресурсы
        if (!currentFileName.isEmpty()) {
            mediaPlayer->stop();
            mediaPlayer->setSource(QUrl());
        }

        // Загружаем новый файл
        currentFileName = fileName;
        updateWindowTitle();
        processAudioFile(fileName);
        mediaPlayer->setSource(QUrl::fromLocalFile(fileName));
        hasUnsavedChanges = false;
        statusBar()->showMessage(tr("File loaded: %1").arg(fileName), 2000);
    }
}

void MainWindow::processAudioFile(const QString& filePath)
{
    setEnabled(false);
    QApplication::processEvents();

    LoadFileDialog dialog(this, BPMAnalyzer::AnalysisResult());
    dialog.setWindowTitle(tr("Analysis and Beat Alignment"));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.updateProgress(tr("Loading audio..."), 10);
    dialog.show();
    dialog.raise();
    dialog.activateWindow();
    QApplication::processEvents();

    bool loadOk = false;
    const QVector<QVector<float>> audioData = loadAudioFile(filePath, &loadOk,
        [this, &dialog](int percent) {
            // Декодирование занимает «нижнюю» часть прогресс-бара (10..45 %).
            dialog.updateProgress(tr("Loading audio..."), 10 + (percent * 35) / 100);
        });
    if (!loadOk || audioData.isEmpty()) {
        dialog.close();
        setEnabled(true);
        statusBar()->showMessage(tr("File load error"), 3000);
        return;
    }

    // Параметры совпадают с BPMAnalyzer::AnalysisOptions по умолчанию (Mixxx, 60–200 BPM, δ 5 %)
    const BPMAnalyzer::AnalysisOptions analysisOptions;

    dialog.updateProgress(tr("Audio analysis..."), 50);
    const BPMAnalyzer::AnalysisResult analysis =
        BPMAnalyzer::analyzeBPM(audioData[0], waveformView->getSampleRate(), analysisOptions);

    dialog.updateProgress(tr("Analysis completed."), 100);
    dialog.showResult(analysis);
    dialog.setBeatsPerBar(4);

    updateUIAfterAnalysis(audioData, analysis, dialog.getBeatsPerBar());

    const bool accepted = (dialog.exec() == QDialog::Accepted);
    const int beatsPerBar = dialog.getBeatsPerBar();

    updateUIAfterAnalysis(audioData, analysis, beatsPerBar);

    if (accepted && dialog.shouldFixBeats()) {
        createDeviationMarkers(analysisOptions.tolerancePercent);
    } else if (dialog.keepMarkersOnSkip()) {
        createDeviationMarkers(analysisOptions.tolerancePercent, true);
    }

    alignWaveformViewToBarGrid(waveformView, analysis.bpm, beatsPerBar, analysis.gridStartSample);

    updateTimeLabel(0);
    updateHorizontalScrollBar(waveformView->getZoomLevel());
    resetLoopStateAfterNewFile();
    setEnabled(true);
    showPitchGridAnalyzeOverlay();
}

void MainWindow::resetLoopStateAfterNewFile()
{
    loopStartPosition = 0;
    loopEndPosition = 0;
    isLoopEnabled = false;
    ui->loopStartButton->setStyleSheet("");
    ui->loopEndButton->setStyleSheet("");
    ui->loopButton->setStyleSheet("");
    ui->loopButton->setChecked(false);
}

QVector<QVector<float>> MainWindow::loadAudioFile(const QString& filePath,
                                                  bool* ok,
                                                  const std::function<void(int)>& onProgress)
{
    if (ok)
        *ok = false;

    // Декодирование вынесено в AudioFileService (нативный формат, без ресемплинга).
    const AudioFileService::DecodeResult res = AudioFileService::decode(filePath, onProgress);

    if (!res.ok) {
        if (!res.error.isEmpty())
            statusBar()->showMessage(tr("Decode error: %1").arg(res.error), 3000);
        return {};
    }

    // Сохраняем нативную частоту дискретизации для всего пайплайна.
    if (waveformView && res.sampleRate > 0)
        waveformView->setSampleRate(res.sampleRate);

    if (ok)
        *ok = true;
    return res.channels;
}

void MainWindow::saveAudioFile()
{
    doSaveAudioFile();
}

bool MainWindow::doSaveAudioFile()
{
    const QString filterFloat = tr("WAV 32-bit float (*.wav)");
    const QString filterPcm24 = tr("WAV 24-bit PCM (*.wav)");
    const QString filterPcm16 = tr("WAV 16-bit PCM (*.wav)");
    const QString filterAll = tr("All files (*)");
    const QString filters = filterFloat + QStringLiteral(";;")
                          + filterPcm24 + QStringLiteral(";;")
                          + filterPcm16 + QStringLiteral(";;")
                          + filterAll;

    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save Audio File"), "",
        filters,
        &selectedFilter);

    if (fileName.isEmpty()) {
        return false;
    }
    if (!fileName.endsWith(".wav", Qt::CaseInsensitive)) {
        fileName += ".wav";
    }

    const QVector<QVector<float>>& audioData = waveformView->getAudioData();
    if (audioData.isEmpty()) {
        statusBar()->showMessage(tr("Error: nothing to save"), 2000);
        return false;
    }

    WavWriter::WriteOptions writeOptions;
    if (selectedFilter == filterFloat) {
        writeOptions.format = WavWriter::SampleFormat::Float32;
    } else if (selectedFilter == filterPcm24) {
        writeOptions.format = WavWriter::SampleFormat::Pcm24;
    } else {
        writeOptions.format = WavWriter::SampleFormat::Pcm16;
    }

    QString error;
    if (!WavWriter::writeFile(fileName, audioData, waveformView->getSampleRate(), &error, writeOptions)) {
        QMessageBox::warning(this, tr("DONTFLOAT"),
                           tr("Could not save %1:\n%2.")
                           .arg(QDir::toNativeSeparators(fileName), error));
        return false;
    }

    hasUnsavedChanges = false;
    statusBar()->showMessage(tr("Saved: %1").arg(fileName), 2000);
    return true;
}

void MainWindow::updateWindowTitle()
{
    QString title = "DONTFLOAT";
    if (!currentFileName.isEmpty()) {
        QFileInfo fileInfo(currentFileName);
        title += " - " + fileInfo.fileName();
    }
    setWindowTitle(title);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_A) {
        qDebug() << "A key pressed, modifiers:" << event->modifiers() << "ShiftModifier:" << Qt::ShiftModifier;
        if (event->modifiers() & Qt::ShiftModifier) {
            qDebug() << "Shift+A pressed - clearing loop start";
            clearLoopStart();
        } else {
            qDebug() << "A pressed - setting loop start";
            setLoopStart();
        }
    } else if (event->key() == Qt::Key_B) {
        qDebug() << "B key pressed, modifiers:" << event->modifiers() << "ShiftModifier:" << Qt::ShiftModifier;
        if (event->modifiers() & Qt::ShiftModifier) {
            qDebug() << "Shift+B pressed - clearing loop end";
            clearLoopEnd();
        } else {
            qDebug() << "B pressed - setting loop end";
            setLoopEnd();
        }
    } else if (event->key() == Qt::Key_Up && event->modifiers() == Qt::ControlModifier) {
        increaseBPM();
    } else if (event->key() == Qt::Key_Down && event->modifiers() == Qt::ControlModifier) {
        decreaseBPM();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls()) return;

    QList<QUrl> urls = mime->urls();
    if (urls.isEmpty()) return;

    QString fileName = urls.first().toLocalFile();
    if (fileName.isEmpty()) return;

    event->acceptProposedAction();

    if (!maybeSave())
        return;

    resetAudioState();
    if (!currentFileName.isEmpty()) {
        mediaPlayer->stop();
        mediaPlayer->setSource(QUrl());
    }

    currentFileName = fileName;
    updateWindowTitle();
    processAudioFile(fileName);
    mediaPlayer->setSource(QUrl::fromLocalFile(fileName));
    hasUnsavedChanges = false;
    statusBar()->showMessage(tr("File loaded: %1").arg(fileName), 2000);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == waveformView && event->type() == QEvent::Resize) {
        syncPitchGridTimelineWidth();
    }

    if (watched == pitchGridScrollContainer && event->type() == QEvent::Resize) {
        layoutPitchGridScrollOverlay();
        syncPitchGridTimelineWidth();
    }

    if (watched == ui->pitchGridWidget && event->type() == QEvent::Resize) {
        layoutPitchGridAnalyzeOverlay();
    }

    if (event->type() == QEvent::MouseButtonPress) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (watched == ui->keyInput) {
                showKeyContextMenu(mouseEvent->pos());
                return true;
            }
            if (watched == ui->keyInput2) {
                showKeyContextMenu2(mouseEvent->pos());
                return true;
            }
        }
    }

    if (watched == centralWidget()) {
        if (event->type() == QEvent::DragEnter) {
            dragEnterEvent(static_cast<QDragEnterEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::Drop) {
            dropEvent(static_cast<QDropEvent*>(event));
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::toggleMetronome()
{
    if (!metronomeController) {
        return;
    }

    bool wasEnabled = metronomeController->isEnabled();
    metronomeController->setEnabled(!wasEnabled);

    ui->metronomeButton->setChecked(metronomeController->isEnabled());

    if (metronomeController->isEnabled()) {
        float bpm = ui->bpmEdit->text().toFloat();
        metronomeController->setBPM(bpm);
        statusBar()->showMessage(tr("Metronome on"), 2000);
    } else {
        statusBar()->showMessage(tr("Metronome off"), 2000);
    }
}

void MainWindow::toggleLoop()
{
    // Проверяем, что точки A и B установлены
    if (loopStartPosition <= 0 || loopEndPosition <= 0) {
        statusBar()->showMessage(tr("Error: Set loop points A and B first!"), 3000);
        ui->loopButton->setChecked(false);
        return;
    }

    isLoopEnabled = !isLoopEnabled;
    ui->loopButton->setChecked(isLoopEnabled);

    if (isLoopEnabled) {
        // Включаем цикл
        ui->loopButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border: 2px solid #1976D2; }");
        statusBar()->showMessage(tr("Loop on: %1 - %2").arg(TimeUtils::formatTime(loopStartPosition)).arg(TimeUtils::formatTime(loopEndPosition)), 3000);
    } else {
        // Выключаем цикл
        ui->loopButton->setStyleSheet("");
        statusBar()->showMessage(tr("Loop off"), 2000);
    }
}

void MainWindow::updateLoopPoints()
{
    if (isLoopEnabled && isPlaying) {
        qint64 position = mediaPlayer->position();
        if (position >= loopEndPosition) {
            // Возвращаемся к началу цикла
            mediaPlayer->setPosition(loopStartPosition);

            // Обновляем позицию в визуализации
            if (waveformView) {
                waveformView->setPlaybackPosition(loopStartPosition);
            }
            if (pitchGridWidget) {
                pitchGridWidget->setPlaybackPosition(loopStartPosition);
            }

            // Показываем сообщение о цикле
            statusBar()->showMessage(tr("Loop: %1 - %2").arg(TimeUtils::formatTime(loopStartPosition)).arg(TimeUtils::formatTime(loopEndPosition)), 1000);
        }
    }
}



void MainWindow::showMetronomeSettings()
{
    // Передаем контроллер в диалог для прямой синхронизации
    MetronomeSettingsDialog dialog(this, metronomeController);
    if (dialog.exec() == QDialog::Accepted) {
        // Настройки уже применены к контроллеру в saveSettings() диалога
        statusBar()->showMessage(tr("Metronome settings updated"), 2000);
    }
}

QString MainWindow::loadStyleSheet(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось загрузить стиль:" << resourcePath;
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

void MainWindow::applyWidgetBackgrounds(const QString& scheme)
{
    // "system" — сброс пользовательских стилей (пустая строка), иначе цвет фона.
    QString waveBg, scrollBg;
    if (scheme == "light") {
        waveBg = QStringLiteral("#f5f5f5");
        scrollBg = QStringLiteral("#e0e0e0");
    } else if (scheme != "system") { // dark / default / прочее → тёмный фон
        waveBg = QStringLiteral("#2b2b2b");
        scrollBg = QStringLiteral("#404040");
    }

    auto sheetFor = [](const QString& color) {
        return color.isEmpty() ? QString()
                               : QStringLiteral("QWidget { background-color: %1; }").arg(color);
    };

    if (ui->waveformWidget)  ui->waveformWidget->setStyleSheet(sheetFor(waveBg));
    if (ui->pitchGridWidget) ui->pitchGridWidget->setStyleSheet(sheetFor(waveBg));
    if (ui->scrollBarWidget) ui->scrollBarWidget->setStyleSheet(sheetFor(scrollBg));
}

void MainWindow::applyScrollBarStyles(const QString& scheme)
{
    // "system" — сброс (пустая строка), light — светлая схема, иначе — тёмная.
    QString qss;
    if (scheme == "light") {
        qss = loadStyleSheet(QStringLiteral(":/styles/resources/styles/scrollbars_light.qss"));
    } else if (scheme != "system") {
        qss = loadStyleSheet(QStringLiteral(":/styles/resources/styles/scrollbars_dark.qss"));
    }

    if (horizontalScrollBar) {
        horizontalScrollBar->setStyleSheet(qss);
    }
    if (pitchGridVerticalScrollBar) {
        updateScrollBarTransparency();
    }
}

void MainWindow::setTheme(const QString& theme)
{
    if (theme == "default") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette defaultPalette;
        defaultPalette.setColor(QPalette::Window, QColor(49, 54, 59));
        defaultPalette.setColor(QPalette::WindowText, QColor(239, 240, 241));
        defaultPalette.setColor(QPalette::Base, QColor(35, 38, 41));
        defaultPalette.setColor(QPalette::AlternateBase, QColor(49, 54, 59));
        defaultPalette.setColor(QPalette::ToolTipBase, QColor(49, 54, 59));
        defaultPalette.setColor(QPalette::ToolTipText, QColor(239, 240, 241));
        defaultPalette.setColor(QPalette::Text, QColor(239, 240, 241));
        defaultPalette.setColor(QPalette::Button, QColor(49, 54, 59));
        defaultPalette.setColor(QPalette::ButtonText, QColor(239, 240, 241));
        defaultPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
        defaultPalette.setColor(QPalette::Link, QColor(41, 128, 185));
        defaultPalette.setColor(QPalette::Highlight, QColor(61, 174, 233));
        defaultPalette.setColor(QPalette::HighlightedText, QColor(239, 240, 241));
        qApp->setPalette(defaultPalette);

        // Тёмный фон виджетов; стили скроллбаров для "default" не трогаем (как и раньше).
        applyWidgetBackgrounds("default");
    } else {
        // System theme
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(QApplication::style()->standardPalette());

        // Сбрасываем пользовательские стили виджетов и скроллбаров.
        applyWidgetBackgrounds("system");
        applyScrollBarStyles("system");
    }

    settings.setValue("theme", theme);
    statusBar()->showMessage(tr("Theme: %1").arg(theme), 2000);
}

void MainWindow::setColorScheme(const QString& scheme)
{
    if (scheme == "dark") {
        // Установка тёмной темы для всего приложения
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
    } else if (scheme == "light") {
        // Установка светлой темы для всего приложения
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette lightPalette;
        lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::black);
        lightPalette.setColor(QPalette::ToolTipText, Qt::black);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(0, 120, 215));
        lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(lightPalette);
    }

    // Применяем схему к WaveformView
    if (waveformView) {
        waveformView->setColorScheme(scheme);
    }

    // Применяем схему к PitchGridWidget
    if (pitchGridWidget) {
        pitchGridWidget->setColorScheme(scheme);
    }

    // Фон виджетов и стили скроллбаров — единые helpers (см. :/styles/*.qss).
    applyWidgetBackgrounds(scheme);
    applyScrollBarStyles(scheme);
    applyPitchGridAnalyzeOverlayStyle(scheme);

    settings.setValue("colorScheme", scheme);
    statusBar()->showMessage(tr("Color scheme: %1").arg(scheme == "dark" ? "Тёмная" : "Светлая"), 2000);
}

void MainWindow::updateHorizontalScrollBar(float zoom)
{
    if (!horizontalScrollBar) return;

    // Если масштаб 1.0 или меньше, скроллбар не нужен
    if (zoom <= 1.0f) {
        horizontalScrollBar->setMaximum(0);
        horizontalScrollBar->setPageStep(100);
        horizontalScrollBar->setValue(0);
        return;
    }

    // Вычисляем максимальное значение для скроллбара
    int maxValue = static_cast<int>((zoom - 1.0f) * 1000.0f);
    int pageSize = static_cast<int>(1000.0f / zoom);

    horizontalScrollBar->setMaximum(maxValue);
    horizontalScrollBar->setPageStep(pageSize);
    horizontalScrollBar->setSingleStep(qMax(1, pageSize / 10));

    // Устанавливаем текущее значение, сохраняя относительную позицию
    int newValue = qBound(0, horizontalScrollBar->value(), maxValue);
    horizontalScrollBar->setValue(newValue);
}

void MainWindow::updateHorizontalScrollBarFromOffset(float offset)
{
    if (!horizontalScrollBar || !waveformView) return;

    float zoom = waveformView->getZoomLevel();

    // Если масштаб 1.0 или меньше, скроллбар не нужен
    if (zoom <= 1.0f) {
        return;
    }

    // Вычисляем максимальное значение для скроллбара
    int maxValue = static_cast<int>((zoom - 1.0f) * 1000.0f);

    // Конвертируем offset (0.0-1.0) в значение скроллбара
    int scrollValue = static_cast<int>(offset * maxValue);
    scrollValue = qBound(0, scrollValue, maxValue);

    // Обновляем значение скроллбара без эмитирования сигнала
    horizontalScrollBar->blockSignals(true);
    horizontalScrollBar->setValue(scrollValue);
    horizontalScrollBar->blockSignals(false);
}

void MainWindow::constrainWindowSize()
{
    // Полностью убираем ограничения размера окна
    // Позволяем Qt самому управлять окном для разворачивания и минимизации
    setMaximumHeight(16777215); // Максимальное значение для размера
    setMaximumWidth(16777215);  // Максимальное значение для размера

    // Убираем все ограничения, чтобы окно могло разворачиваться на весь экран
}

void MainWindow::snapAllMarkersToGrid()
{
    if (!waveformView) {
        statusBar()->showMessage(tr("Waveform not initialized."), 3000);
        return;
    }

    const QVector<QVector<float>>& data = waveformView->getAudioData();
    if (data.isEmpty() || data[0].isEmpty()) {
        statusBar()->showMessage(tr("No audio loaded."), 3000);
        return;
    }

    const float bpm = waveformView->getBPM();
    if (bpm <= 0.0f || waveformView->getBeatInfo().isEmpty()) {
        statusBar()->showMessage(tr("No beat grid to snap to (BPM or beats not detected)."), 4000);
        return;
    }

    QVector<Marker> markers = waveformView->getMarkers();
    if (markers.size() < 2) {
        statusBar()->showMessage(tr("No markers to snap to the grid."), 3000);
        return;
    }

    QVector<Marker> snapped = waveformView->snapMarkersToGrid(markers);
    if (snapped.isEmpty() || snapped.size() != markers.size()) {
        statusBar()->showMessage(tr("Could not snap markers to the grid."), 3000);
        return;
    }

    int movedCount = 0;
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].position != snapped[i].position) {
            ++movedCount;
        }
    }

    waveformView->setMarkers(snapped);

    if (movedCount > 0) {
        statusBar()->showMessage(
            tr("Markers snapped to the grid (%1)").arg(movedCount),
            4000);
    } else {
        statusBar()->showMessage(tr("All markers are already on the grid."), 3000);
    }
}

void MainWindow::shiftBeatGridBackward()
{
    shiftBeatGridByBeats(-1);
}

void MainWindow::shiftBeatGridForward()
{
    shiftBeatGridByBeats(1);
}

void MainWindow::shiftBeatGridByBeats(int beatDelta)
{
    if (!waveformView) {
        statusBar()->showMessage(tr("Waveform not initialized."), 3000);
        return;
    }

    const QVector<QVector<float>>& data = waveformView->getAudioData();
    if (data.isEmpty() || data[0].isEmpty()) {
        statusBar()->showMessage(tr("No audio loaded."), 3000);
        return;
    }

    const float bpm = waveformView->getBPM();
    const int sampleRate = waveformView->getSampleRate();
    if (bpm <= 0.0f || sampleRate <= 0) {
        statusBar()->showMessage(tr("No beat grid to shift (BPM not detected)."), 4000);
        return;
    }

    const qint64 beatSamples = qMax<qint64>(1, qRound((60.0f * sampleRate) / bpm));
    const bool moveMarkers = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    const qint64 maxGridStart = qMax<qint64>(0, data[0].size() - 1);
    const qint64 oldGridStart = waveformView->getGridStartSample();
    const qint64 newGridStart = qBound<qint64>(
        0,
        oldGridStart + beatDelta * beatSamples,
        maxGridStart);

    if (newGridStart == oldGridStart) {
        statusBar()->showMessage(tr("Beat grid is already at the file boundary."), 3000);
        return;
    }

    waveformView->shiftGridBySamples(newGridStart - oldGridStart, moveMarkers);
    updateTimeLabel(currentPosition);

    const QString direction = beatDelta < 0 ? tr("back") : tr("forward");
    const QString markersNote = moveMarkers ? tr(" (markers moved)") : QString();
    statusBar()->showMessage(
        tr("Beat grid shifted one beat %1%2").arg(direction, markersNote),
        3000);
}

void MainWindow::createDeviationMarkers(float tolerancePercent, bool neutralMarkers)
{
    if (!waveformView) {
        return;
    }

    // Получаем информацию о долях из WaveformView
    QVector<BPMAnalyzer::BeatInfo> beats = waveformView->getBeatInfo();
    if (beats.isEmpty()) {
        statusBar()->showMessage(tr("No beat information"), 3000);
        return;
    }

    // Получаем BPM и sampleRate
    float bpm = waveformView->getBPM();
    int sampleRate = waveformView->getSampleRate();

    if (bpm <= 0 || sampleRate <= 0) {
        statusBar()->showMessage(tr("Invalid BPM or sample rate"), 3000);
        return;
    }

    // Вычисляем отклонения для всех долей
    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    // Находим неровные доли
    float deviationThreshold = tolerancePercent / 100.0f; // Преобразуем проценты в доли
    QVector<int> unalignedIndices = BPMAnalyzer::findUnalignedBeats(beats, deviationThreshold);

    if (unalignedIndices.isEmpty()) {
        statusBar()->showMessage(tr("No irregular beats found"), 3000);
        return;
    }

    int markersCreated = 0;
    QSet<qint64> addedPositions; // Избегаем дубликатов при последовательных неровных долях

    // Создаём метки для интервалов между долями
    // Для каждой неровной доли создаем пару меток:
    // - начало интервала (предыдущая доля)
    // - конец интервала (текущая неровная доля)
    for (int idx : unalignedIndices) {
        if (idx == 0) {
            // Пропускаем первую долю - нет предыдущей
            continue;
        }

        const BPMAnalyzer::BeatInfo& prevBeat = beats[idx - 1];
        const BPMAnalyzer::BeatInfo& currentBeat = beats[idx];

        // Метки коррекции: position = фактическая доля.
        // Если neutralMarkers — originalPosition = position (сегменты не сжимаются/не растягиваются).
        // Иначе originalPosition = ожидаемая по сетке (коэффициент сегмента (actual/expected) != 1).
        if (!addedPositions.contains(prevBeat.position)) {
            Marker startMarker(prevBeat.position, sampleRate);
            startMarker.originalPosition = neutralMarkers ? prevBeat.position : prevBeat.expectedPosition;
            startMarker.originalTimeMs = TimeUtils::samplesToMs(startMarker.originalPosition, sampleRate);
            waveformView->addMarker(startMarker);
            addedPositions.insert(prevBeat.position);
            markersCreated++;
        }
        if (!addedPositions.contains(currentBeat.position)) {
            Marker endMarker(currentBeat.position, sampleRate);
            endMarker.originalPosition = neutralMarkers ? currentBeat.position : currentBeat.expectedPosition;
            endMarker.originalTimeMs = TimeUtils::samplesToMs(endMarker.originalPosition, sampleRate);
            waveformView->addMarker(endMarker);
            addedPositions.insert(currentBeat.position);
            markersCreated++;
        }
    }

    // Сортируем метки по позиции
    waveformView->sortMarkers();

    statusBar()->showMessage(
        tr("Created %1 correction markers for %2 irregular beats")
            .arg(markersCreated)
            .arg(unalignedIndices.size()),
        5000);

    waveformView->update();
}

QString MainWindow::formatTimeAndBars(qint64 msPosition)
{
    if (isShuttingDown || !ui) {
        return TimeUtils::formatTime(msPosition);
    }
    float bpm = ui->bpmEdit->text().toFloat();
    int bpb = 4;
    if (waveformView) {
        bpb = qMax(1, waveformView->getBeatsPerBar());
    } else {
        QVariant v = ui->barsCombo->currentData();
        if (v.isValid()) {
            int num = v.toInt();
            if (num >= 1 && num <= 32) bpb = num;
        }
    }
    int sampleRate = waveformView ? waveformView->getSampleRate() : 44100;
    qint64 gridStart = waveformView ? waveformView->getGridStartSample() : 0;
    return TimeUtils::formatTimeAndBars(msPosition, bpm, bpb, sampleRate, gridStart);
}

void MainWindow::togglePitchGrid()
{
    isPitchGridVisible = !isPitchGridVisible;
    updatePitchGridLayout();

    if (togglePitchGridAct) {
        togglePitchGridAct->setText(isPitchGridVisible
            ? tr("Hide Pitch Grid")
            : tr("Show Pitch Grid"));
    }

    if (isPitchGridVisible) {
        syncPitchGridFromWaveform();
        if (pitchGridAnalyzePending) {
            layoutPitchGridAnalyzeOverlay();
            if (pitchGridAnalyzeOverlay) {
                pitchGridAnalyzeOverlay->raise();
            }
        }
    }

    settings.setValue("pitchGridVisible", isPitchGridVisible);
}

void MainWindow::onPitchGridAnalyzeClicked()
{
    startPitchAnalysis();
}

void MainWindow::abortPitchAnalysis()
{
    ++pitchAnalysisEpoch;
    pitchAnalysisRunning = false;
    if (pitchAnalysisProgressTimer) {
        pitchAnalysisProgressTimer->stop();
    }
    setPitchAnalysisUiRunning(false);
}

void MainWindow::setPitchAnalysisUiRunning(bool running)
{
    if (pitchGridAnalyzeButton) {
        pitchGridAnalyzeButton->setVisible(!running);
    }
    if (pitchGridAnalyzeProgress) {
        pitchGridAnalyzeProgress->setValue(0);
        pitchGridAnalyzeProgress->setVisible(running);
    }
}

void MainWindow::startPitchAnalysis()
{
    if (pitchAnalysisRunning) {
        return;
    }
    if (!waveformView || waveformView->getAudioData().isEmpty()) {
        statusBar()->showMessage(tr("Load an audio file first"), 3000);
        return;
    }

    // Анализируем исходные данные: warp по меткам применяется к нотам отдельно
    const QVector<float> mono = AudioFileService::toMono(waveformView->getSourceAudioData());
    const int sampleRate = waveformView->getSampleRate();
    if (mono.isEmpty() || sampleRate <= 0) {
        statusBar()->showMessage(tr("No audio to analyze"), 3000);
        return;
    }

    // Тактовая сетка для потактового анализа модуляции (снимаем в UI-потоке).
    KeyAnalyzer::BarGrid barGrid;
    barGrid.bpm = waveformView->getBPM();
    barGrid.beatsPerBar = waveformView->getBeatsPerBar();
    barGrid.gridStartSample = waveformView->getGridStartSample();

    const qint64 epoch = ++pitchAnalysisEpoch;
    pitchAnalysisRunning = true;
    setPitchAnalysisUiRunning(true);
    statusBar()->showMessage(tr("Analyzing key and notes..."), 0);

    pitchAnalysisProgressValue = std::make_shared<std::atomic<int>>(0);
    if (!pitchAnalysisProgressTimer) {
        pitchAnalysisProgressTimer = new QTimer(this);
        pitchAnalysisProgressTimer->setInterval(100);
        connect(pitchAnalysisProgressTimer, &QTimer::timeout, this, [this]() {
            if (pitchGridAnalyzeProgress && pitchAnalysisProgressValue) {
                pitchGridAnalyzeProgress->setValue(pitchAnalysisProgressValue->load());
            }
        });
    }
    pitchAnalysisProgressTimer->start();

    auto pending = std::make_shared<PitchAnalysisOutcome>();
    std::shared_ptr<std::atomic<int>> progress = pitchAnalysisProgressValue;
    // QPointer: окно могли закрыть, пока крутится пул потоков.
    const QPointer<MainWindow> self(this);

    (void)QtConcurrent::run([self, epoch, mono, sampleRate, progress, barGrid, pending]() {
        bool ok = false;
        try {
            progress->store(2);
            pending->perBarKey = KeyAnalyzer::analyzeKeyPerBar(mono, sampleRate, barGrid);
            progress->store(12);
            pending->key.primaryKey = pending->perBarKey.primaryKey;
            pending->key.overallConfidence = pending->perBarKey.primaryKey.confidence;
            pending->key.hasKeyChange = pending->perBarKey.hasModulation;
            if (pending->perBarKey.hasModulation) {
                pending->key.secondaryKey =
                    KeyAnalyzer::dominantModulationKey(
                        pending->perBarKey, pending->perBarKey.primaryKey.key);
            }
            progress->store(15);
            pending->notes = PitchDetector::detectNotes(
                mono, sampleRate, PitchDetector::Options(),
                [progress](int pct) { progress->store(15 + pct * 85 / 100); });
            progress->store(100);
            ok = true;
        } catch (const std::exception& e) {
            qWarning() << "Pitch analysis failed:" << e.what();
        } catch (...) {
            qWarning() << "Pitch analysis failed with unknown exception";
        }

        if (!self) {
            return;
        }
        // Без QFutureWatcher::result() — только QueuedConnection в UI-поток.
        QMetaObject::invokeMethod(self, [self, epoch, ok, pending]() {
            if (!self) {
                return;
            }
            self->finishPitchAnalysis(epoch, ok, pending);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::finishPitchAnalysis(qint64 epoch, bool ok,
                                     const std::shared_ptr<PitchAnalysisOutcome>& pending)
{
    if (epoch != pitchAnalysisEpoch) {
        return;
    }

    pitchAnalysisRunning = false;
    if (pitchAnalysisProgressTimer) {
        pitchAnalysisProgressTimer->stop();
    }

    if (!ok || !pending) {
        setPitchAnalysisUiRunning(false);
        statusBar()->showMessage(tr("Analysis failed"), 3000);
        return;
    }

    applyPerBarKeyResult(pending->perBarKey, pending->key);

    QString keysText = currentKey;
    if (!currentKey2.isEmpty()) {
        keysText += QStringLiteral(" / ") + currentKey2;
    }

    basePitchNotes = pending->notes;
    refreshPitchGridNotes();

    setPitchAnalysisUiRunning(false);
    hidePitchGridAnalyzeOverlay();
    statusBar()->showMessage(
        tr("Analysis finished: %1, notes found: %2")
            .arg(keysText)
            .arg(basePitchNotes.size()),
        5000);
}

void MainWindow::refreshPitchGridNotes()
{
    if (!pitchGridWidget) {
        return;
    }
    if (basePitchNotes.isEmpty() || !waveformView) {
        pitchGridWidget->clearNotes();
        return;
    }
    pitchGridWidget->setNotes(
        warpNotesThroughMarkers(basePitchNotes, waveformView->getMarkers()));
}

void MainWindow::onNotePitchEdited(int noteIndex, float oldPitch, float newPitch)
{
    if (noteIndex < 0 || noteIndex >= basePitchNotes.size() || !undoStack) {
        return;
    }
    // Callback вызывается при каждом redo/undo: помечаем правку нот и
    // запускаем фоновый пересчёт звука (см. updatePlaybackAfterMarkerDrag)
    undoStack->push(new PitchNoteEditCommand(
        pitchGridWidget, &basePitchNotes, noteIndex, oldPitch, newPitch,
        tr("Change note pitch"),
        [this]() {
            noteEditCommandActive = true;
            scheduleMarkerPlaybackPreview();
        }));
}

void MainWindow::onNotePreviewRequested(int noteIndex)
{
    if (!waveformView || noteIndex < 0 || noteIndex >= basePitchNotes.size()) {
        return;
    }

    // Пауза основного воспроизведения, чтобы цикл ноты был слышен отдельно
    if (isPlaying) {
        playAudio();
    }

    // Сегмент берём из исходного аудио (без превью-коррекций): он всегда
    // звучит на detectedPitch, поэтому сдвиг = midiPitch - detectedPitch
    const QVector<QVector<float>>& source = waveformView->getSourceAudioData();
    const int sampleRate = waveformView->getSampleRate();
    if (source.isEmpty() || sampleRate <= 0) {
        return;
    }

    const PitchDetector::PitchNote& note = basePitchNotes[noteIndex];
    const qint64 total = source[0].size();
    const qint64 start = qBound<qint64>(0, note.startSample, total);
    const qint64 end = qBound<qint64>(start, note.endSample, total);
    if (end - start < 256) {
        return;
    }

    QVector<float> segment(int(end - start), 0.0f);
    for (const QVector<float>& channel : source) {
        const int offset = int(start);
        for (int i = 0; i < segment.size(); ++i) {
            segment[i] += channel[offset + i];
        }
    }
    const float norm = 1.0f / float(source.size());
    for (float& s : segment) {
        s *= norm;
    }

    if (!notePreviewPlayer) {
        notePreviewPlayer = new NotePreviewPlayer(this);
    }
    notePreviewPlayer->start(segment, sampleRate, note.midiPitch - note.detectedPitch);
}

void MainWindow::onNotePreviewPitchChanged(int noteIndex, float midiPitch)
{
    if (!notePreviewPlayer || !notePreviewPlayer->isActive()
        || noteIndex < 0 || noteIndex >= basePitchNotes.size()) {
        return;
    }
    notePreviewPlayer->setSemitoneOffset(midiPitch - basePitchNotes[noteIndex].detectedPitch);
}

void MainWindow::stopNotePreview()
{
    if (notePreviewPlayer) {
        notePreviewPlayer->stop();
    }
}

void MainWindow::applyPitchCorrection()
{
    if (!waveformView) {
        return;
    }
    if (basePitchNotes.isEmpty()) {
        statusBar()->showMessage(
            tr("Run note analysis first (the \"Analyze\" button)"), 4000);
        return;
    }
    if (!PitchCorrection::hasPendingEdits(basePitchNotes)) {
        statusBar()->showMessage(
            tr("No edited notes — drag notes on the piano roll"), 4000);
        return;
    }
    if (waveformView->hasTimelineStretch()) {
        QMessageBox::information(this, tr("Note pitch correction"),
            tr("Apply time-stretch first (Ctrl+T), "
               "then note pitch correction."));
        return;
    }

    // База — исходные данные без превью-коррекции (отображаемое аудио может
    // уже содержать фоновую коррекцию, повторное применение сдвоило бы сдвиг)
    const QVector<QVector<float>> baseData = waveformView->getSourceAudioData();
    const QVector<QVector<float>> oldData = waveformView->getAudioData();
    if (baseData.isEmpty() || oldData.isEmpty()) {
        return;
    }
    const int sampleRate = waveformView->getSampleRate();
    const QVector<PitchDetector::PitchNote> notes =
        warpNotesThroughMarkers(basePitchNotes, waveformView->getMarkers());

    statusBar()->showMessage(tr("Applying note pitch correction..."), 0);
    setEnabled(false);

    // Не возвращаем аудио через QFuture::result() — на MSVC Debug это AV в QList::at.
    auto newDataBox = std::make_shared<QVector<QVector<float>>>();
    const QPointer<MainWindow> self(this);

    (void)QtConcurrent::run([self, baseData, notes, sampleRate, newDataBox, oldData]() {
        *newDataBox = PitchCorrection::apply(baseData, notes, sampleRate);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, newDataBox, oldData]() {
            if (!self) {
                return;
            }
            self->setEnabled(true);
            const QVector<QVector<float>>& newData = *newDataBox;
            if (newData.isEmpty() || newData[0].isEmpty()) {
                self->statusBar()->showMessage(
                    self->tr("Error while applying note pitch correction"), 4000);
                return;
            }

            const QVector<Marker> markers = self->waveformView->getMarkers();
            self->undoStack->push(new TimeStretchCommand(
                self->waveformView, oldData, newData, markers, markers,
                self->tr("Apply note pitch correction")));

            for (PitchDetector::PitchNote& note : self->basePitchNotes) {
                note.detectedPitch = note.midiPitch;
            }
            self->refreshPitchGridNotes();

            self->statusBar()->showMessage(
                self->tr("Note pitch correction applied"), 5000);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::analyzeKey()
{
    if (!waveformView || waveformView->getAudioData().isEmpty()) {
        statusBar()->showMessage(tr("Load an audio file first"), 3000);
        return;
    }

    const QVector<QVector<float>>& audioData = waveformView->getAudioData();
    if (audioData.isEmpty()) {
        statusBar()->showMessage(tr("No audio to analyze"), 3000);
        return;
    }

    const QVector<float> samples = audioData[0];
    const int sampleRate = waveformView->getSampleRate();

    KeyAnalyzer::BarGrid barGrid;
    barGrid.bpm = waveformView->getBPM();
    barGrid.beatsPerBar = waveformView->getBeatsPerBar();
    barGrid.gridStartSample = waveformView->getGridStartSample();

    statusBar()->showMessage(tr("Key analysis..."), 0);
    setEnabled(false);

    using KeyOutcome = QPair<KeyAnalyzer::AnalysisResult, KeyAnalyzer::PerBarKeyResult>;
    auto* watcher = new QFutureWatcher<KeyOutcome>(this);
    connect(watcher, &QFutureWatcher<KeyOutcome>::finished, this,
            [this, watcher]() {
        const KeyOutcome outcome = watcher->result();
        const KeyAnalyzer::AnalysisResult result = outcome.first;
        const KeyAnalyzer::PerBarKeyResult perBar = outcome.second;
        setEnabled(true);

        applyPerBarKeyResult(perBar, result);

        QString keysText = currentKey;
        if (!currentKey2.isEmpty()) {
            keysText += QStringLiteral(" / ") + currentKey2;
        }

        statusBar()->showMessage(tr("Key: %1").arg(keysText), 3000);
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([samples, sampleRate, barGrid]() {
        return qMakePair(KeyAnalyzer::analyzeKey(samples, sampleRate),
                         KeyAnalyzer::analyzeKeyPerBar(samples, sampleRate, barGrid));
    }));
}

void MainWindow::showKeyContextMenu(const QPoint& pos)
{
    if (keyMenu)
        keyMenu->popup(ui->keyInput, pos);
}

void MainWindow::showKeyContextMenu2(const QPoint& pos)
{
    if (keyMenu2)
        keyMenu2->popup(ui->keyInput2, pos);
}

void MainWindow::setupKeyModulationStrip()
{
    if (!ui->keyInputContainer) {
        return;
    }

    // Статические dual-поля оставляем скрытыми: тональность/модуляция
    // отображаются потактово на полосе над пианороллом.
    if (ui->keyInput) {
        ui->keyInput->hide();
    }
    if (ui->keyInput2) {
        ui->keyInput2->hide();
    }
    if (ui->keyInputSpacer) {
        ui->keyInputSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    if (!keyModulationStrip) {
        keyModulationStrip = new KeyModulationStrip(ui->keyInputContainer);
        if (auto* layout = ui->keyInputLayout) {
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->insertWidget(0, keyModulationStrip, 1);
        } else {
            keyModulationStrip->setParent(ui->keyInputContainer);
            keyModulationStrip->setGeometry(ui->keyInputContainer->rect());
        }
        connect(keyModulationStrip, &KeyModulationStrip::fieldMenuRequested,
                this, &MainWindow::onKeyModulationFieldMenu);
    }
    syncKeyModulationStripFromWaveform();
}

void MainWindow::syncKeyModulationStripFromWaveform()
{
    if (!keyModulationStrip || !waveformView) {
        return;
    }
    keyModulationStrip->setTimelineSampleCount(waveformView->displaySampleCount());
    keyModulationStrip->setTimelineReferenceWidth(waveformView->width());
    keyModulationStrip->setZoomLevel(waveformView->getZoomLevel());
    keyModulationStrip->setHorizontalOffset(waveformView->getHorizontalOffset());
}

void MainWindow::applyPerBarKeyResult(const KeyAnalyzer::PerBarKeyResult& perBar,
                                      const KeyAnalyzer::AnalysisResult& trackKey)
{
    lastPerBarKey = perBar;

    QString detectedKey = trackKey.primaryKey.keyName;
    KeyAnalyzer::Key primaryEnum = trackKey.primaryKey.key;
    if (detectedKey.isEmpty() || primaryEnum == KeyAnalyzer::UNKNOWN_KEY) {
        if (perBar.primaryKey.key != KeyAnalyzer::UNKNOWN_KEY) {
            detectedKey = perBar.primaryKey.keyName;
            primaryEnum = perBar.primaryKey.key;
        } else {
            detectedKey = QStringLiteral("C Major");
            primaryEnum = KeyAnalyzer::stringToKey(detectedKey);
        }
    }
    setKey(detectedKey);

    const KeyAnalyzer::KeyInfo modKey =
        KeyAnalyzer::dominantModulationKey(perBar, primaryEnum);
    if (modKey.key != KeyAnalyzer::UNKNOWN_KEY && !modKey.keyName.isEmpty()) {
        setKey2(modKey.keyName);
    } else if (trackKey.hasKeyChange
        && trackKey.secondaryKey.key != KeyAnalyzer::UNKNOWN_KEY
        && !trackKey.secondaryKey.keyName.isEmpty()) {
        setKey2(trackKey.secondaryKey.keyName);
    } else {
        setKey2(QString());
    }

    // Поля над пианороллом — по регионам тактов (Melodyne-style).
    QVector<KeyAnalyzer::KeyRegion> regions = perBar.regions;
    if (regions.isEmpty() && !detectedKey.isEmpty() && waveformView) {
        KeyAnalyzer::KeyRegion whole;
        whole.startBar = 0;
        whole.endBar = 0;
        whole.startSample = waveformView->getGridStartSample();
        whole.endSample = qMax<qint64>(whole.startSample + 1, waveformView->displaySampleCount());
        whole.key.keyName = detectedKey;
        whole.key.key = primaryEnum;
        regions.push_back(whole);
    }
    lastPerBarKey.regions = regions;

    if (keyModulationStrip) {
        keyModulationStrip->setRegions(regions);
        syncKeyModulationStripFromWaveform();
    }
}

void MainWindow::onKeyModulationFieldMenu(int regionIndex, QWidget* anchor, const QPoint& localPos)
{
    if (!keyRegionMenu || !anchor || regionIndex < 0) {
        return;
    }
    editingKeyRegionIndex = regionIndex;
    disconnect(keyRegionMenu, &KeySelectionMenu::keySelected, this, nullptr);
    connect(keyRegionMenu, &KeySelectionMenu::keySelected, this,
            [this](const QString& key) {
                applyKeyModulationRegion(editingKeyRegionIndex, key);
            },
            static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
    keyRegionMenu->popup(anchor, localPos);
}

void MainWindow::applyKeyModulationRegion(int regionIndex, const QString& key)
{
    if (!keyModulationStrip || regionIndex < 0
        || regionIndex >= lastPerBarKey.regions.size()) {
        return;
    }

    lastPerBarKey.regions[regionIndex].key.keyName = key;
    lastPerBarKey.regions[regionIndex].key.key =
        key.isEmpty() ? KeyAnalyzer::UNKNOWN_KEY : KeyAnalyzer::stringToKey(key);
    keyModulationStrip->setRegionKey(regionIndex, key);

    // Обновляем потактовые записи внутри региона.
    const KeyAnalyzer::KeyRegion& region = lastPerBarKey.regions[regionIndex];
    for (KeyAnalyzer::BarKey& bar : lastPerBarKey.bars) {
        if (bar.barIndex >= region.startBar && bar.barIndex <= region.endBar) {
            bar.key = lastPerBarKey.regions[regionIndex].key;
        }
    }

    // Пересобираем primary/modulation для легенды пианоролла.
    if (regionIndex == 0 || currentKey.isEmpty()) {
        setKey(key);
    }
    const KeyAnalyzer::KeyInfo modKey =
        KeyAnalyzer::dominantModulationKey(lastPerBarKey,
            KeyAnalyzer::stringToKey(currentKey));
    if (modKey.key != KeyAnalyzer::UNKNOWN_KEY) {
        setKey2(modKey.keyName);
    } else if (regionIndex > 0 && !key.isEmpty()) {
        setKey2(key);
    } else {
        setKey2(QString());
    }
}

void MainWindow::setKey(const QString& key)
{
    currentKey = key;
    // Пустое значение показываем через placeholder («Не определена»)
    ui->keyInput->setText(key);

    // Обновляем стиль поля ввода в зависимости от того, определена ли тональность
    if (key.isEmpty()) {
        ui->keyInput->setStyleSheet(
            "QLineEdit {"
            "    background-color: #2b2b2b;"
            "    border: 1px solid #555;"
            "    border-radius: 3px;"
            "    padding: 2px 5px;"
            "    color: #666;"
            "    font-size: 12px;"
            "}"
        );
    } else {
        ui->keyInput->setStyleSheet(
            "QLineEdit {"
            "    background-color: #2b2b2b;"
            "    border: 1px solid #42a5f5;"
            "    border-radius: 3px;"
            "    padding: 2px 5px;"
            "    color: white;"
            "    font-size: 12px;"
            "}"
        );
    }

    // Сохраняем настройку
    settings.setValue("currentKey", currentKey);

    if (pitchGridWidget) {
        pitchGridWidget->setPrimaryKey(currentKey);
    }
}

void MainWindow::setKey2(const QString& key)
{
    currentKey2 = key;
    // Dual-поле скрыто: модуляция видна на потактовой полосе.
    // Оставляем скрытый QLineEdit синхронизированным для совместимости.
    ui->keyInput2->setVisible(false);
    ui->keyInput2->setText(key);

    // Обновляем стиль поля ввода в зависимости от того, определена ли тональность
    if (key.isEmpty()) {
        ui->keyInput2->setStyleSheet(
            "QLineEdit {"
            "    background-color: #2b2b2b;"
            "    border: 1px solid #555;"
            "    border-radius: 2px;"
            "    padding: 2px 6px;"
            "    color: #666;"
            "    font-size: 11px;"
            "    min-width: 120px;"
            "}"
        );
    } else {
        ui->keyInput2->setStyleSheet(
            "QLineEdit {"
            "    background-color: #2b2b2b;"
            "    border: 1px solid #42a5f5;"
            "    border-radius: 2px;"
            "    padding: 2px 6px;"
            "    color: white;"
            "    font-size: 11px;"
            "    min-width: 120px;"
            "}"
        );
    }

    // Сохраняем настройку
    settings.setValue("currentKey2", currentKey2);

    if (pitchGridWidget) {
        pitchGridWidget->setSecondaryKey(currentKey2);
    }
}

void MainWindow::layoutPitchGridScrollOverlay()
{
    if (!pitchGridScrollContainer || !pitchGridWidget || !pitchGridVerticalScrollBar) {
        return;
    }

    const QRect area = pitchGridScrollContainer->rect();
    pitchGridWidget->setGeometry(area);
    pitchGridVerticalScrollBar->setGeometry(
        0, 0, UiConstants::kScrollBarWidthPx, area.height());
    pitchGridVerticalScrollBar->raise();
    layoutPitchGridAnalyzeOverlay();
}

void MainWindow::setupPitchGridAnalyzeOverlay()
{
    if (!ui->pitchGridWidget) {
        return;
    }

    pitchGridAnalyzeOverlay = new QWidget(ui->pitchGridWidget);
    pitchGridAnalyzeOverlay->setObjectName(QStringLiteral("pitchGridAnalyzeOverlay"));
    pitchGridAnalyzeOverlay->setAttribute(Qt::WA_StyledBackground, true);

    auto* overlayLayout = new QVBoxLayout(pitchGridAnalyzeOverlay);
    overlayLayout->setContentsMargins(12, 12, 12, 12);
    overlayLayout->addStretch();

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    pitchGridAnalyzeButton = new QPushButton(tr("Analyze"), pitchGridAnalyzeOverlay);
    pitchGridAnalyzeButton->setCursor(Qt::PointingHandCursor);
    pitchGridAnalyzeButton->setMinimumWidth(140);
    pitchGridAnalyzeButton->setMinimumHeight(30);
    buttonRow->addWidget(pitchGridAnalyzeButton);
    buttonRow->addStretch();
    overlayLayout->addLayout(buttonRow);

    auto* progressRow = new QHBoxLayout();
    progressRow->addStretch();
    pitchGridAnalyzeProgress = new QProgressBar(pitchGridAnalyzeOverlay);
    pitchGridAnalyzeProgress->setRange(0, 100);
    pitchGridAnalyzeProgress->setValue(0);
    pitchGridAnalyzeProgress->setTextVisible(true);
    pitchGridAnalyzeProgress->setFormat(tr("Analyzing... %p%"));
    pitchGridAnalyzeProgress->setFixedWidth(240);
    pitchGridAnalyzeProgress->setMinimumHeight(22);
    pitchGridAnalyzeProgress->hide();
    progressRow->addWidget(pitchGridAnalyzeProgress);
    progressRow->addStretch();
    overlayLayout->addLayout(progressRow);
    overlayLayout->addStretch();

    connect(pitchGridAnalyzeButton, &QPushButton::clicked,
            this, &MainWindow::onPitchGridAnalyzeClicked);

    applyPitchGridAnalyzeOverlayStyle(settings.value("colorScheme", "dark").toString());
    ui->pitchGridWidget->installEventFilter(this);
    pitchGridAnalyzeOverlay->hide();
}

void MainWindow::showPitchGridAnalyzeOverlay()
{
    if (!pitchGridAnalyzeOverlay || !ui->pitchGridWidget) {
        return;
    }

    pitchGridAnalyzePending = true;
    layoutPitchGridAnalyzeOverlay();
    pitchGridAnalyzeOverlay->show();
    pitchGridAnalyzeOverlay->raise();
}

void MainWindow::hidePitchGridAnalyzeOverlay()
{
    pitchGridAnalyzePending = false;
    if (pitchGridAnalyzeOverlay) {
        pitchGridAnalyzeOverlay->hide();
    }
}

void MainWindow::layoutPitchGridAnalyzeOverlay()
{
    if (!pitchGridAnalyzeOverlay || !ui->pitchGridWidget) {
        return;
    }

    pitchGridAnalyzeOverlay->setGeometry(ui->pitchGridWidget->rect());
    if (pitchGridAnalyzePending) {
        pitchGridAnalyzeOverlay->raise();
    }
}

void MainWindow::applyPitchGridAnalyzeOverlayStyle(const QString& scheme)
{
    if (!pitchGridAnalyzeOverlay || !pitchGridAnalyzeButton) {
        return;
    }

    if (scheme == QStringLiteral("light")) {
        pitchGridAnalyzeOverlay->setStyleSheet(
            QStringLiteral("#pitchGridAnalyzeOverlay { background-color: rgba(170, 170, 170, 165); }"));
        pitchGridAnalyzeButton->setStyleSheet(
            QStringLiteral(
                "QPushButton {"
                "  background-color: rgba(255, 255, 255, 220);"
                "  color: #222;"
                "  border: 1px solid #999;"
                "  border-radius: 4px;"
                "  padding: 6px 16px;"
                "  font-size: 12px;"
                "  font-weight: bold;"
                "}"
                "QPushButton:hover { background-color: rgba(255, 255, 255, 245); }"
                "QPushButton:pressed { background-color: rgba(230, 230, 230, 245); }"));
        if (pitchGridAnalyzeProgress) {
            pitchGridAnalyzeProgress->setStyleSheet(
                QStringLiteral(
                    "QProgressBar {"
                    "  background-color: rgba(255, 255, 255, 200);"
                    "  color: #222;"
                    "  border: 1px solid #999;"
                    "  border-radius: 4px;"
                    "  text-align: center;"
                    "}"
                    "QProgressBar::chunk { background-color: #42a5f5; border-radius: 3px; }"));
        }
    } else {
        pitchGridAnalyzeOverlay->setStyleSheet(
            QStringLiteral("#pitchGridAnalyzeOverlay { background-color: rgba(70, 70, 70, 175); }"));
        pitchGridAnalyzeButton->setStyleSheet(
            QStringLiteral(
                "QPushButton {"
                "  background-color: rgba(55, 55, 55, 230);"
                "  color: #eee;"
                "  border: 1px solid #888;"
                "  border-radius: 4px;"
                "  padding: 6px 16px;"
                "  font-size: 12px;"
                "  font-weight: bold;"
                "}"
                "QPushButton:hover { background-color: rgba(75, 75, 75, 240); }"
                "QPushButton:pressed { background-color: rgba(45, 45, 45, 240); }"));
        if (pitchGridAnalyzeProgress) {
            pitchGridAnalyzeProgress->setStyleSheet(
                QStringLiteral(
                    "QProgressBar {"
                    "  background-color: rgba(40, 40, 40, 220);"
                    "  color: #eee;"
                    "  border: 1px solid #888;"
                    "  border-radius: 4px;"
                    "  text-align: center;"
                    "}"
                    "QProgressBar::chunk { background-color: #1976d2; border-radius: 3px; }"));
        }
    }
}

void MainWindow::applyPitchGridVerticalScrollBarAlpha(int alpha)
{
    if (!pitchGridVerticalScrollBar) {
        return;
    }

    alpha = qBound(UiConstants::kPitchGridScrollBarMinAlpha, alpha,
                   UiConstants::kPitchGridScrollBarIdleAlpha);
    const int handleAlpha = qMin(255, alpha + 50);
    const int hoverAlpha = qMin(255, alpha + 70);

    const QString currentScheme = settings.value("colorScheme", "dark").toString();
    if (currentScheme == "dark") {
        pitchGridVerticalScrollBar->setStyleSheet(
            QStringLiteral(
                "QScrollBar:vertical {"
                "    background: rgba(64, 64, 64, %1);"
                "    width: %4px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(96, 96, 96, %2);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(112, 112, 112, %3);"
                "}"
                "QScrollBar::handle:vertical:pressed {"
                "    background: rgba(160, 160, 160, 255);"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                "    background: none;"
                "    border: none;"
                "}"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
                "    background: none;"
                "    border: none;"
                "}")
                .arg(alpha)
                .arg(handleAlpha)
                .arg(hoverAlpha)
                .arg(UiConstants::kScrollBarWidthPx));
    } else {
        pitchGridVerticalScrollBar->setStyleSheet(
            QStringLiteral(
                "QScrollBar:vertical {"
                "    background: rgba(224, 224, 224, %1);"
                "    width: %4px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(192, 192, 192, %2);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(160, 160, 160, %3);"
                "}"
                "QScrollBar::handle:vertical:pressed {"
                "    background: rgba(128, 128, 128, 255);"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                "    background: none;"
                "    border: none;"
                "}"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
                "    background: none;"
                "    border: none;"
                "}")
                .arg(alpha)
                .arg(handleAlpha)
                .arg(hoverAlpha)
                .arg(UiConstants::kScrollBarWidthPx));
    }
}

void MainWindow::updateScrollBarTransparency()
{
    if (!pitchGridVerticalScrollBar || !pitchGridWidget) {
        return;
    }

    const float cursorContentX = pitchGridWidget->playbackCursorContentX();
    const float distanceRightOfScrollBar =
        cursorContentX - float(UiConstants::kScrollBarWidthPx);

    int alpha = UiConstants::kPitchGridScrollBarIdleAlpha;
    if (distanceRightOfScrollBar <= 0.0f) {
        alpha = UiConstants::kPitchGridScrollBarMinAlpha;
    } else if (distanceRightOfScrollBar < float(UiConstants::kPitchGridScrollBarFadeRangePx)) {
        const float t = distanceRightOfScrollBar / float(UiConstants::kPitchGridScrollBarFadeRangePx);
        alpha = UiConstants::kPitchGridScrollBarMinAlpha
            + int(t * float(UiConstants::kPitchGridScrollBarIdleAlpha
                             - UiConstants::kPitchGridScrollBarMinAlpha));
    }

    applyPitchGridVerticalScrollBarAlpha(alpha);
}

void MainWindow::setRussianLanguage()
{
    if (settings.value("language").toString() == "ru_RU")
        return;
    qApp->removeTranslator(m_appTranslator);
    if (!loadTranslation(m_appTranslator, "ru_RU")) {
        // Restore previous translator and revert checkboxes
        if (loadTranslation(m_appTranslator, "en_US"))
            qApp->installTranslator(m_appTranslator);
        russianAction->setChecked(false);
        englishAction->setChecked(true);
        statusBar()->showMessage(tr("Translation load error"), 3000);
        return;
    }
    qApp->installTranslator(m_appTranslator);
    settings.setValue("language", "ru_RU");
    russianAction->setChecked(true);
    englishAction->setChecked(false);
    ui->retranslateUi(this);
    retranslateMenus();
    statusBar()->showMessage(tr("Language: Russian"), 2000);
}

void MainWindow::setEnglishLanguage()
{
    if (settings.value("language").toString() == "en_US")
        return;
    qApp->removeTranslator(m_appTranslator);
    // Source strings are English: identity en_US.qm is optional.
    if (loadTranslation(m_appTranslator, "en_US"))
        qApp->installTranslator(m_appTranslator);
    settings.setValue("language", "en_US");
    englishAction->setChecked(true);
    russianAction->setChecked(false);
    ui->retranslateUi(this);
    retranslateMenus();
    statusBar()->showMessage(tr("Language: English"), 2000);
}

void MainWindow::applyTimeStretch()
{
    if (!waveformView) {
        statusBar()->showMessage(tr("Error: WaveformView not initialized"), 3000);
        return;
    }

    // Получаем текущие метки
    QVector<Marker> currentMarkers = waveformView->getMarkers();

    if (currentMarkers.size() < 2) {
        QMessageBox::warning(this,
                            tr("Not enough markers"),
                            tr("At least 2 markers are required to apply stretch.\n"
                                            "Press M to add markers."));
        return;
    }

    // Получаем исходные данные
    const QVector<QVector<float>>& oldData = waveformView->getAudioData();

    if (oldData.isEmpty()) {
        statusBar()->showMessage(tr("Error: no audio loaded"), 3000);
        return;
    }

    // Применяем сжатие-растяжение (теперь возвращает структуру с данными и метками)
    TimeStretchProcessor::StretchResult stretchResult = waveformView->applyTimeStretch(currentMarkers);
    QVector<QVector<float>> newData = stretchResult.audioData;

    // Применяем гранулярный питч-шифт (если включён)
    if (pitchShiftParams.enabled && !newData.isEmpty()) {
        newData = GranularEngine::applyPitchShiftQt(newData, waveformView->getSampleRate(), pitchShiftParams);
    }

    // Конвертируем MarkerData → Marker для WaveformView
    QVector<Marker> newMarkers = MarkerUtils::toMarkers(stretchResult.newMarkers);

    if (newData.isEmpty() || newData[0].isEmpty()) {
        statusBar()->showMessage(tr("Audio processing error"), 3000);
        return;
    }

    // Метки уже обновлены в applyTimeStretch на основе реальной длины обработанных сегментов
    // Проверяем, что все метки правильно обновлены
    int sampleRate = waveformView->getSampleRate();
    for (Marker& marker : newMarkers) {
        marker.updateTimeFromSamples(sampleRate);
    }

    // Убеждаемся, что есть конечная метка
    bool hasEndMarker = false;
    for (const Marker& m : newMarkers) {
        if (m.isEndMarker) {
            hasEndMarker = true;
            break;
        }
    }

    if (!hasEndMarker && !newMarkers.isEmpty()) {
        qint64 newSize = newData[0].size();
        Marker endMarker(newSize - 1, false, true, sampleRate);
        endMarker.originalPosition = newSize - 1;
        newMarkers.append(endMarker);
    }

    // Создаем команду для undo/redo
    TimeStretchCommand* command = new TimeStretchCommand(
        waveformView,
        oldData,
        newData,
        currentMarkers,
        newMarkers,
        tr("Apply time stretch")
    );

    // Применяем команду (push автоматически вызывает redo())
    // redo() уже обновит originalAudioData через updateOriginalData()
    undoStack->push(command);

    // Метки сбрасываются (originalPosition = position), поэтому запекаем warp
    // в базовые координаты нот: originalPosition старых меток → position новых.
    if (!basePitchNotes.isEmpty()) {
        QVector<Marker> mapping = currentMarkers;
        std::sort(mapping.begin(), mapping.end(),
                  [](const Marker& a, const Marker& b) {
                      return a.originalPosition < b.originalPosition;
                  });
        const int pairCount = qMin(mapping.size(), int(stretchResult.newMarkers.size()));
        mapping.resize(pairCount);
        for (int i = 0; i < pairCount; ++i) {
            mapping[i].position = stretchResult.newMarkers[i].position;
        }
        basePitchNotes = warpNotesThroughMarkers(basePitchNotes, mapping);
        refreshPitchGridNotes();
    }

    // Явно обновляем визуализацию после применения эффекта
    if (waveformView) {
        waveformView->update();
    }

    statusBar()->showMessage(tr("Stretch applied. Length: %1 → %2 samples")
                             .arg(oldData.isEmpty() ? 0 : oldData[0].size())
                             .arg(newData.isEmpty() ? 0 : newData[0].size()), 5000);
}

void MainWindow::onUndoStackChanged()
{
    if (!undoStack) {
        return;
    }

    // Правка высоты ноты: не переключаем источник на «сырое» аудио —
    // скорректированный звук придёт из фонового превью
    if (noteEditCommandActive) {
        noteEditCommandActive = false;
        hasUnsavedChanges = undoStack->index() > 0;
        if (isPitchGridVisible) {
            syncPitchGridFromWaveform();
        }
        return;
    }

    if (undoStack->index() > 0) {
        syncPlaybackWithWaveform();
        hasUnsavedChanges = true;
    } else {
        hasUnsavedChanges = false;
        if (!currentFileName.isEmpty() && mediaPlayer) {
            mediaPlayer->setSource(QUrl::fromLocalFile(currentFileName));
            mediaPlayer->setPosition(0);
            currentPosition = 0;
            if (waveformView) {
                waveformView->setPlaybackPosition(0);
            }
            updateTimeLabel(0);
            if (isPlaying) {
                mediaPlayer->stop();
                isPlaying = false;
                if (playbackTimer) {
                    playbackTimer->stop();
                }
                if (ui->playButton) {
                    ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
                }
            }
        }
    }

    if (waveformView) {
        waveformView->update();
    }
    if (isPitchGridVisible) {
        syncPitchGridFromWaveform();
    }
}

void MainWindow::syncPlaybackWithWaveform()
{
    if (!waveformView || !mediaPlayer) {
        return;
    }

    const QVector<QVector<float>>& data = waveformView->getAudioData();
    if (data.isEmpty() || data[0].isEmpty()) {
        return;
    }

    const int sampleRate = waveformView->getSampleRate();
    const QString tempWavPath = WavWriter::writeTempProcessedFile(data, sampleRate);
    if (tempWavPath.isEmpty()) {
        return;
    }

    mediaPlayer->setSource(QUrl::fromLocalFile(tempWavPath));
    mediaPlayer->setPosition(0);
    currentPosition = 0;
    waveformView->setPlaybackPosition(0);
    updateTimeLabel(0);

    if (isPlaying) {
        mediaPlayer->stop();
        isPlaying = false;
        if (playbackTimer) {
            playbackTimer->stop();
        }
        if (ui->playButton) {
            ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
        }
    }
}

void MainWindow::scheduleMarkerPlaybackPreview()
{
    if (isShuttingDown || !waveformView || !markerPreviewTimer) {
        return;
    }
    // Пересчёт нужен либо при метках stretch, либо при изменённых нотах
    if (waveformView->getMarkers().size() < 2
        && !PitchCorrection::hasPendingEdits(basePitchNotes)) {
        return;
    }

    markerPlaybackPreviewPending = true;
    markerPreviewTimer->start();
}

void MainWindow::updatePlaybackAfterMarkerDrag()
{
    if (isShuttingDown || !waveformView || !mediaPlayer) {
        return;
    }

    markerPlaybackPreviewPending = false;

    const bool hasStretch = waveformView->getMarkers().size() >= 2
        && waveformView->hasTimelineStretch();
    const bool hasNoteEdits = PitchCorrection::hasPendingEdits(basePitchNotes);

    if (!hasStretch && !hasNoteEdits) {
        // Нет ни растяжения, ни правок нот — возвращаем исходный файл
        if (!currentFileName.isEmpty()) {
            const QUrl originalUrl = QUrl::fromLocalFile(currentFileName);
            if (mediaPlayer->source() != originalUrl) {
                previewRestorePosition = mediaPlayer->position();
                previewOldDuration = mediaPlayer->duration();
                previewWasPlaying = (mediaPlayer->playbackState() == QMediaPlayer::PlayingState);
                if (waveformView) {
                    // Восстанавливаем волну без коррекции
                    waveformView->applyStretchedPreview(waveformView->getSourceAudioData());
                }
                applyMarkerPreviewMediaSource(currentFileName);
            }
        }
        return;
    }

    // Предыдущий пересчёт ещё идёт — повторим после завершения
    if (markerPreviewRunning && markerPreviewRunning->load()) {
        markerPlaybackPreviewPending = true;
        markerPreviewTimer->start();
        return;
    }

    const QVector<MarkerData> markerData = MarkerUtils::toMarkerData(waveformView->getMarkers());
    const QVector<QVector<float>> sourceData = waveformView->getSourceAudioData();
    const int sampleRate = waveformView->getSampleRate();

    if (sourceData.isEmpty() || sourceData[0].isEmpty()) {
        return;
    }

    // Ноты в координатах целевого (растянутого) таймлайна
    const QVector<PitchDetector::PitchNote> notesForRender = hasNoteEdits
        ? warpNotesThroughMarkers(basePitchNotes, waveformView->getMarkers())
        : QVector<PitchDetector::PitchNote>();

    previewRestorePosition = mediaPlayer->position();
    previewOldDuration = mediaPlayer->duration();
    previewWasPlaying = (mediaPlayer->playbackState() == QMediaPlayer::PlayingState);

    const qint64 epoch = ++markerPreviewEpoch;
    markerPreviewRunning->store(true);

    auto pending = std::make_shared<QPair<QString, QVector<QVector<float>>>>();
    const QPointer<MainWindow> self(this);
    auto running = markerPreviewRunning;

    (void)QtConcurrent::run(
        [self, epoch, running, sourceData, markerData, sampleRate, hasStretch, notesForRender, pending]() {
            bool ok = false;
            try {
                QVector<QVector<float>> processed = sourceData;
                bool failed = false;

                if (hasStretch) {
                    const TimeStretchProcessor::StretchResult result =
                        TimeStretchProcessor::applyMarkerStretch(sourceData, markerData, sampleRate, true);
                    if (result.audioData.isEmpty() || result.audioData[0].isEmpty()) {
                        failed = true;
                    } else {
                        processed = result.audioData;
                    }
                }

                if (!failed && !notesForRender.isEmpty()) {
                    processed = PitchCorrection::apply(processed, notesForRender, sampleRate);
                }

                if (failed || processed.isEmpty() || processed[0].isEmpty()) {
                    *pending = {};
                    ok = false;
                } else {
                    const QString path = WavWriter::writeTempProcessedFile(processed, sampleRate);
                    if (path.isEmpty()) {
                        qWarning() << "updatePlaybackAfterMarkerDrag: failed to save processed audio";
                    }
                    *pending = qMakePair(path, processed);
                    ok = true;
                }
            } catch (const std::exception& e) {
                qWarning() << "Marker preview render failed:" << e.what();
                *pending = {};
                ok = false;
            } catch (...) {
                qWarning() << "Marker preview render failed with unknown exception";
                *pending = {};
                ok = false;
            }

            if (running) {
                running->store(false);
            }
            QMetaObject::invokeMethod(self, [self, epoch, ok, pending]() {
                if (!self) {
                    return;
                }
                self->finishMarkerPreview(epoch, ok, pending);
            }, Qt::QueuedConnection);
        });

    qDebug() << "updatePlaybackAfterMarkerDrag: background render started,"
             << sourceData[0].size() << "samples," << markerData.size() << "markers,"
             << notesForRender.size() << "notes";
}

void MainWindow::applyMarkerPreviewMediaSource(const QString& path)
{
    if (isShuttingDown || !mediaPlayer || path.isEmpty()) {
        return;
    }

    const QUrl url = QUrl::fromLocalFile(path);
    const auto isLoaded = [this, url]() {
        const QMediaPlayer::MediaStatus status = mediaPlayer->mediaStatus();
        return mediaPlayer->source() == url
            && (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia);
    };

    if (isLoaded()) {
        restorePlaybackPositionAfterSourceChange();
        if (markerPlaybackPreviewPending) {
            scheduleMarkerPlaybackPreview();
        }
        return;
    }

    const auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
        [this, url, connection](QMediaPlayer::MediaStatus status) {
            if (status != QMediaPlayer::LoadedMedia && status != QMediaPlayer::BufferedMedia) {
                return;
            }
            if (mediaPlayer->source() != url) {
                return;
            }
            disconnect(*connection);
            restorePlaybackPositionAfterSourceChange();
            if (markerPlaybackPreviewPending) {
                scheduleMarkerPlaybackPreview();
            }
        });

    mediaPlayer->setSource(url);
}

void MainWindow::finishMarkerPreview(qint64 epoch, bool ok,
    const std::shared_ptr<QPair<QString, QVector<QVector<float>>>>& preview)
{
    if (isShuttingDown || !ui || !mediaPlayer) {
        return;
    }
    if (epoch != markerPreviewEpoch) {
        return;
    }
    Q_UNUSED(ok);

    if (!preview || (preview->first.isEmpty() && preview->second.isEmpty())) {
        if (markerPlaybackPreviewPending) {
            scheduleMarkerPlaybackPreview();
        }
        return;
    }

    if (!preview->second.isEmpty() && waveformView) {
        waveformView->applyStretchedPreview(preview->second);
    }

    if (preview->first.isEmpty()) {
        if (markerPlaybackPreviewPending) {
            scheduleMarkerPlaybackPreview();
        }
        return;
    }

    applyMarkerPreviewMediaSource(preview->first);
}

void MainWindow::createOnsetMarkersAuto()
{
    const QString dialogTitle = tr("Auto markers on transients");

    if (!waveformView) {
        QMessageBox::warning(this, dialogTitle, tr("Waveform not initialized."));
        return;
    }

    const QVector<QVector<float>>& data = waveformView->getAudioData();
    if (data.isEmpty() || data[0].isEmpty()) {
        QMessageBox::warning(this, dialogTitle, tr("No audio loaded."));
        return;
    }

    const int sampleRate = waveformView->getSampleRate();
    if (sampleRate <= 0) {
        QMessageBox::warning(this, dialogTitle, tr("Invalid sample rate."));
        return;
    }

    // --- Подготовка моно-сигнала ---
    const int numCh = data.size();
    const int numSamples = data[0].size();
    QVector<float> mono;
    mono.resize(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        double sum = 0.0;
        for (int ch = 0; ch < numCh; ++ch) {
            if (i < data[ch].size())
                sum += data[ch][i];
        }
        mono[i] = static_cast<float>(sum / qMax(1, numCh));
    }

    // --- Оценка огибающей и её производной (простая onset-функция) ---
    QVector<float> env;
    env.resize(numSamples);
    const float alpha = 0.99f; // экспоненциальное сглаживание
    env[0] = std::fabs(mono[0]);
    for (int i = 1; i < numSamples; ++i) {
        const float x = std::fabs(mono[i]);
        env[i] = qMax(x, env[i - 1] * alpha);
    }

    QVector<float> diff;
    diff.resize(numSamples);
    diff[0] = 0.0f;
    float maxDiff = 0.0f;
    for (int i = 1; i < numSamples; ++i) {
        float d = env[i] - env[i - 1];
        if (d < 0.0f) d = 0.0f;
        diff[i] = d;
        if (d > maxDiff) maxDiff = d;
    }

    if (maxDiff <= 0.0f) {
        QMessageBox::information(this, dialogTitle,
                                 tr("No transients found."));
        return;
    }

    const float threshold = maxDiff * UiConstants::kOnsetDetectionThresholdRatio;
    const int minDistanceSamples = sampleRate / UiConstants::kOnsetMinDistanceSampleRateDivisor;

    QVector<qint64> onsetSamples;
    onsetSamples.reserve(256);
    int lastOnsetIdx = -minDistanceSamples;

    for (int i = 1; i < numSamples - 1; ++i) {
        if (diff[i] < threshold)
            continue;

        // простой локальный максимум
        if (diff[i] < diff[i - 1] || diff[i] <= diff[i + 1])
            continue;

        if (i - lastOnsetIdx < minDistanceSamples)
            continue;

        onsetSamples.append(i);
        lastOnsetIdx = i;
    }

    if (onsetSamples.isEmpty()) {
        QMessageBox::information(this, dialogTitle, tr("No suitable transients found."));
        return;
    }

    // --- Создаём метки по найденным транзиентам ---
    waveformView->clearMarkers();

    for (qint64 s : onsetSamples) {
        Marker m(s, sampleRate);
        waveformView->addMarker(m);
    }

    waveformView->sortMarkers();
    waveformView->update();

    statusBar()->showMessage(
        tr("Created %1 transient markers").arg(onsetSamples.size()),
        4000);
}

void MainWindow::toggleBeatWaveform()
{
    if (waveformView) {
        bool currentState = waveformView->getShowBeatWaveform();
        waveformView->setShowBeatWaveform(!currentState);
        toggleBeatWaveformAct->setChecked(!currentState);

        if (!currentState) {
            statusBar()->showMessage(tr("Beat waveform enabled"), 2000);
        } else {
            statusBar()->showMessage(tr("Beat waveform disabled"), 2000);
        }
    }
}

// ============================================================================
// Вспомогательные методы для рефакторинга
// ============================================================================

void MainWindow::setBPMAndBeatsPerBar(float bpm, int beatsPerBar)
{
    ui->bpmEdit->setText(QString::number(bpm, 'f', 2));
    for (int i = 0; i < ui->barsCombo->count(); ++i) {
        if (ui->barsCombo->itemData(i).toInt() == beatsPerBar) {
            ui->barsCombo->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::updateUIAfterAnalysis(const QVector<QVector<float>>& audioData,
                                       const BPMAnalyzer::AnalysisResult& analysis,
                                       int beatsPerBar)
{
    if (!waveformView) return;

    waveformView->setAudioData(audioData);
    waveformView->setBeatInfo(analysis.beats);
    waveformView->setGridStartSample(analysis.gridStartSample);
    waveformView->setBPM(analysis.bpm);
    waveformView->setBeatsAligned(false);
    waveformView->setBeatsPerBar(beatsPerBar);
    waveformView->update();

    setBPMAndBeatsPerBar(analysis.bpm, beatsPerBar);

    if (pitchGridWidget) {
        pitchGridWidget->setAudioData(audioData);
        pitchGridWidget->setSampleRate(waveformView->getSampleRate());
        pitchGridWidget->setBPM(analysis.bpm);
        pitchGridWidget->setBeatsPerBar(beatsPerBar);
        pitchGridWidget->setGridStartSample(analysis.gridStartSample);
        pitchGridWidget->update();
    }

    if (metronomeController) {
        metronomeController->setBPM(analysis.bpm);
    }

    updateHorizontalScrollBar(waveformView->getZoomLevel());
}

void MainWindow::updateUIAfterBeatFix(const QVector<QVector<float>>& fixedData,
                                      const BPMAnalyzer::AnalysisResult& analysis,
                                      int beatsPerBar)
{
    if (!waveformView) return;

    waveformView->setAudioData(fixedData);
    waveformView->setGridStartSample(analysis.gridStartSample);
    waveformView->setBPM(analysis.bpm);
    waveformView->setBeatsAligned(true);
    waveformView->setBeatsPerBar(beatsPerBar);
    waveformView->update();

    setBPMAndBeatsPerBar(analysis.bpm, beatsPerBar);

    if (pitchGridWidget) {
        pitchGridWidget->setAudioData(fixedData);
        pitchGridWidget->setSampleRate(waveformView->getSampleRate());
        pitchGridWidget->setBPM(analysis.bpm);
        pitchGridWidget->setBeatsPerBar(beatsPerBar);
        pitchGridWidget->setGridStartSample(analysis.gridStartSample);
        pitchGridWidget->update();
    }

    if (metronomeController) {
        metronomeController->setBPM(analysis.bpm);
    }

    // Настройка зума и смещения для выровненного аудио
    waveformView->setZoomLevel(1.0f);
    const int sampleRate = waveformView->getSampleRate();
    float samplesPerBeat = (60.0f * sampleRate) / analysis.bpm;
    float barLengthInQuarters = (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    float offset = float(analysis.gridStartSample) / samplesPerBar;
    offset = offset - floor(offset);
    waveformView->setHorizontalOffset(offset);
    updateHorizontalScrollBar(waveformView->getZoomLevel());
    waveformView->update();

    if (pitchGridWidget) {
        pitchGridWidget->setZoomLevel(waveformView->getZoomLevel());
        pitchGridWidget->setHorizontalOffset(waveformView->getHorizontalOffset());
        syncPitchGridTimelineWidth();
        pitchGridWidget->update();
    }
}

QVector<Marker> MainWindow::createAlignedBeatMarkers(const QVector<BPMAnalyzer::BeatInfo>& alignedBeats,
                                                      qint64 totalSamples, int sampleRate)
{
    QVector<Marker> markers;
    if (alignedBeats.isEmpty() || totalSamples <= 0 || sampleRate <= 0) return markers;

    const qint64 minSegmentSamples = (sampleRate * 50) / 1000; // 50 мс между метками

    markers.append(Marker(0, true, sampleRate));
    qint64 lastPos = 0;

    for (const BPMAnalyzer::BeatInfo& beat : alignedBeats) {
        qint64 pos = beat.position;
        if (pos <= 0) continue;
        if (pos >= totalSamples) break;
        if (pos - lastPos >= minSegmentSamples) {
            Marker m(pos, sampleRate);
            m.originalPosition = pos;
            m.originalTimeMs = TimeUtils::samplesToMs(pos, sampleRate);
            m.updateTimeFromSamples(sampleRate);
            markers.append(m);
            lastPos = pos;
        }
    }

    qint64 endPos = totalSamples - 1;
    if (endPos > lastPos) {
        Marker endMarker(endPos, false, true, sampleRate);
        endMarker.originalPosition = endPos;
        endMarker.originalTimeMs = TimeUtils::samplesToMs(endPos, sampleRate);
        endMarker.updateTimeFromSamples(sampleRate);
        markers.append(endMarker);
    }

    return markers;
}

QVector<BPMAnalyzer::BeatInfo> MainWindow::createAlignedBeatGrid(float bpm, qint64 gridStartSample,
                                                                  qint64 totalSamples, int sampleRate,
                                                                  const QVector<QVector<float>>& audioData)
{
    QVector<BPMAnalyzer::BeatInfo> alignedBeats;
    if (bpm <= 0 || totalSamples <= 0 || sampleRate <= 0) return alignedBeats;

    const float beatInterval = (60.0f * sampleRate) / bpm;
    qint64 pos = gridStartSample;

    while (pos < totalSamples) {
        BPMAnalyzer::BeatInfo beat;
        beat.position = pos;
        beat.expectedPosition = pos;
        beat.confidence = 1.0f;
        beat.deviation = 0.0f;
        beat.energy = (pos >= 0 && pos < totalSamples && !audioData.isEmpty() && !audioData[0].isEmpty())
            ? std::abs(audioData[0][static_cast<int>(pos)]) : 0.0f;
        alignedBeats.append(beat);
        pos += static_cast<qint64>(beatInterval);
    }

    return alignedBeats;
}

void MainWindow::applyBeatFixToWaveform(const QVector<QVector<float>>& originalData,
                                        const QVector<QVector<float>>& fixedData,
                                        const BPMAnalyzer::AnalysisResult& analysis,
                                        int beatsPerBar)
{
    if (!waveformView) return;

    // Создаём выровненную сетку битов
    const int sampleRate = waveformView->getSampleRate();
    const qint64 totalSamples = fixedData.isEmpty() ? 0 : fixedData[0].size();
    QVector<BPMAnalyzer::BeatInfo> alignedBeats = createAlignedBeatGrid(
        analysis.bpm, analysis.gridStartSample, totalSamples, sampleRate, fixedData);

    // Создаём команду отмены
    BeatFixCommand* command = new BeatFixCommand(
        waveformView, originalData, fixedData, analysis.bpm, alignedBeats, analysis.gridStartSample);
    undoStack->push(command);

    // Применяем выровненные данные к волне
    waveformView->setAudioData(fixedData);
    waveformView->setBeatInfo(alignedBeats);
    waveformView->setGridStartSample(analysis.gridStartSample);
    waveformView->setBPM(analysis.bpm);
    waveformView->setBeatsAligned(true);
    waveformView->setBeatsPerBar(beatsPerBar);

    // Метки по каждой доле выровненной сетки (как при «Пропустить» + оставить метки)
    waveformView->clearMarkers();
    QVector<Marker> gridMarkers = createAlignedBeatMarkers(alignedBeats, totalSamples, sampleRate);
    if (gridMarkers.size() >= 2) {
        waveformView->setMarkers(gridMarkers);
    }

    // Обновляем остальной UI (BPM поле, комбобокс, питч-сетка, метроном, зум)
    updateUIAfterBeatFix(fixedData, analysis, beatsPerBar);
}

void MainWindow::restorePlaybackPositionAfterSourceChange()
{
    if (isShuttingDown || !mediaPlayer) {
        return;
    }

    const qint64 newDuration = mediaPlayer->duration();

    if (newDuration > 0 && previewOldDuration > 0 && previewRestorePosition > 0) {
        const float ratio = float(newDuration) / float(previewOldDuration);
        qint64 newPosition = qint64(previewRestorePosition * ratio);
        newPosition = qBound(qint64(0), newPosition, newDuration);
        mediaPlayer->setPosition(newPosition);
    } else if (newDuration > 0 && previewRestorePosition > 0) {
        mediaPlayer->setPosition(qBound(qint64(0), previewRestorePosition, newDuration));
    } else {
        mediaPlayer->setPosition(0);
    }

    if (previewWasPlaying) {
        mediaPlayer->play();
    }
}
