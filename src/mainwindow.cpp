#include "../include/mainwindow.h"
#include "../include/beatfixcommand.h"
#include "../include/timestretchcommand.h"
#include "../include/timestretchprocessor.h"
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
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QMessageBox>
#include <QtCore/QDir>
#include <QtCore/QtGlobal>
#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QtMath>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QInputDialog>
#include "../include/bpmanalyzer.h"
#include "../include/loadfiledialog.h"
#include "../include/metronomesettingsdialog.h"
#include "../include/markerengine.h"
#include "../include/timeutils.h"
#include <QUndoStack>
#include <QtGui/QShortcut>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QValidator>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <QtCore/QtAlgorithms>
#include <QtCore/QTranslator>
#include <QtCore/QLocale>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , waveformView(nullptr)
    , pitchGridWidget(nullptr)
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
    , isPitchGridVisible(false) // По умолчанию питч-сетка скрыта
    , currentKey("")
    , currentKey2("")
    , keyContextMenu(nullptr)
    , keyContextMenu2(nullptr)
{
    // Initialize all pointers to nullptr first
    openAct = nullptr;
    saveAct = nullptr;
    exitAct = nullptr;
    undoAct = nullptr;
    redoAct = nullptr;
    defaultThemeAct = nullptr;
    darkSchemeAct = nullptr;
    lightSchemeAct = nullptr;
    metronomeSettingsAct = nullptr;
    keyboardShortcutsAct = nullptr;
    playPauseAct = nullptr;
    stopAct = nullptr;
    metronomeAct = nullptr;
    loopStartAct = nullptr;
    loopEndAct = nullptr;
    togglePitchGridAct = nullptr;
    undoStack = new QUndoStack(this);

    // Initialize key actions arrays
    for (int i = 0; i < 28; ++i) {
        keyActions[i] = nullptr;
        keyActions2[i] = nullptr;
    }

    // Load and install translator before setupUi (language from settings or system)
    m_appTranslator = new QTranslator(this);
    QString lang = settings.value("language", QString()).toString();
    if (lang.isEmpty()) {
        QString sys = QLocale::system().name();
        if (sys.startsWith("en")) lang = "en_US";
        else if (sys.startsWith("ru")) lang = "ru_RU";
        else lang = "en_US";
    }
    const QString trPath = QCoreApplication::applicationDirPath();
    if (m_appTranslator->load("DONTFLOAT_" + lang, trPath) || m_appTranslator->load("DONTFLOAT_" + lang, trPath + "/../")) {
        qApp->installTranslator(m_appTranslator);
    }

    createActions();  // Create actions first
    ui->setupUi(this);  // Then setup UI
    createKeyContextMenu();  // Create key context menu
    createKeyContextMenu2();  // Create second key context menu

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

    // Настраиваем валидатор для поля BPM
    // QValidator* bpmValidator = new QRegularExpressionValidator(
    //     QRegularExpression("^\\d{1,4}(\\.\\d{1,2})?$"), this);
    // ui->bpmEdit->setValidator(bpmValidator);

    // Настраиваем валидатор для поля Размер такта (целое 1..32)
    // QValidator* barsValidator = new QRegularExpressionValidator(
    //     QRegularExpression("^(?:[1-9]|[12]\\d|3[0-2])$"), this);
    // if (ui->barsCombo) ... (размер такта задаётся комбобоксом)

    // Create and setup WaveformView
    waveformView = new WaveformView(this);
    if (!ui->waveformWidget->layout()) {
        ui->waveformWidget->setLayout(new QVBoxLayout());
    }
    ui->waveformWidget->layout()->addWidget(waveformView);

    // Create and setup PitchGridWidget
    pitchGridWidget = new PitchGridWidget(this);
    if (!ui->pitchGridTableContainer->layout()) {
        ui->pitchGridTableContainer->setLayout(new QVBoxLayout());
    }
    ui->pitchGridTableContainer->layout()->addWidget(pitchGridWidget);

    // Initialize main splitter
    mainSplitter = ui->mainSplitter;
    if (mainSplitter) {
        // Set initial splitter sizes (75% for waveform, 25% for pitch grid)
        QList<int> sizes;
        sizes << 450 << 150; // 75% and 25% of 600px total height
        mainSplitter->setSizes(sizes);

        // Set minimum sizes
        mainSplitter->setChildrenCollapsible(false);

        // Set minimum sizes programmatically for better control
        QWidget* waveformContainer = mainSplitter->widget(0);
        QWidget* pitchGridContainer = mainSplitter->widget(1);

        if (waveformContainer) {
            waveformContainer->setMinimumHeight(150);
        }
        if (pitchGridContainer) {
            pitchGridContainer->setMinimumHeight(80);
            // По умолчанию скрываем питч-сетку
            pitchGridContainer->setVisible(false);
            mainSplitter->setChildrenCollapsible(true);
            // Устанавливаем размеры так, чтобы волна заняла всё пространство
            QList<int> sizes;
            int totalHeight = mainSplitter->height();
            if (totalHeight > 0) {
                sizes << totalHeight << 0;
            } else {
                sizes << 450 << 0; // Временные размеры, если высота еще не установлена
            }
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
    horizontalScrollBar->setFixedHeight(20);
    if (!ui->scrollBarWidget->layout()) {
        ui->scrollBarWidget->setLayout(new QHBoxLayout());
    }
    ui->scrollBarWidget->layout()->addWidget(horizontalScrollBar);

    // Create and setup vertical scrollbar for pitch grid
    pitchGridVerticalScrollBar = new QScrollBar(Qt::Vertical, this);
    pitchGridVerticalScrollBar->setMinimum(0);
    pitchGridVerticalScrollBar->setMaximum(1000);
    pitchGridVerticalScrollBar->setSingleStep(10);  // Small step
    pitchGridVerticalScrollBar->setPageStep(100);   // Large step (for PageUp/PageDown)

    // Создаем отдельный контейнер для вертикального скроллбара питч-сетки
    QWidget* pitchGridScrollContainer = new QWidget(this);
    QHBoxLayout* pitchGridScrollLayout = new QHBoxLayout(pitchGridScrollContainer);
    pitchGridScrollLayout->setContentsMargins(0, 0, 0, 0);
    pitchGridScrollLayout->setSpacing(0);
    pitchGridScrollLayout->addWidget(pitchGridWidget);
    pitchGridScrollLayout->addWidget(pitchGridVerticalScrollBar);

    // Заменяем содержимое pitchGridWidget
    QLayout* oldPitchLayout = ui->pitchGridWidget->layout();
    delete oldPitchLayout;
    ui->pitchGridWidget->setLayout(new QVBoxLayout());
    ui->pitchGridWidget->layout()->addWidget(pitchGridScrollContainer);

    // Устанавливаем ширину вертикального скроллбара
    int scrollBarWidth = 16; // Стандартная ширина скроллбара
    pitchGridVerticalScrollBar->setFixedWidth(scrollBarWidth);

    // Настраиваем цвета скроллбаров
    QString currentScheme = settings.value("colorScheme", "dark").toString();
    if (currentScheme == "dark") {
        horizontalScrollBar->setStyleSheet(
            "QScrollBar:horizontal {"
            "    background: #404040;"
            "    height: 20px;"
            "    border: none;"
            "}"
            "QScrollBar::handle:horizontal {"
            "    background: #606060;"
            "    min-width: 20px;"
            "    border-radius: 0px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "    background: #707070;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "    background: none;"
            "    border: none;"
            "}"
        );



        pitchGridVerticalScrollBar->setStyleSheet(
            "QScrollBar:vertical {"
            "    background: rgba(64, 64, 64, 128);"
            "    width: 16px;"
            "    border: none;"
            "    border-radius: 8px;"
            "}"
            "QScrollBar::handle:vertical {"
            "    background: rgba(96, 96, 96, 180);"
            "    min-height: 20px;"
            "    border-radius: 8px;"
            "    margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "    background: rgba(112, 112, 112, 200);"
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
            "}"
        );
    }

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

    // Применяем сохраненную цветовую схему или темную по умолчанию
    currentScheme = settings.value("colorScheme", "dark").toString();
    if (currentScheme == "dark") {
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("QWidget { background-color: #404040; }");
        }
        // rulerWidget больше не существует в новой структуре UI
    } else if (currentScheme == "light") {
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("QWidget { background-color: #f5f5f5; }");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("QWidget { background-color: #f5f5f5; }");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("QWidget { background-color: #e0e0e0; }");
        }
        // rulerWidget больше не существует в новой структуре UI

        // Применяем светлые стили к скроллбарам для светлой темы
        if (horizontalScrollBar) {
            horizontalScrollBar->setStyleSheet(
                "QScrollBar:horizontal {"
                "    background: #e0e0e0;"
                "    height: 20px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:horizontal {"
                "    background: #c0c0c0;"
                "    min-width: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:horizontal:hover {"
                "    background: #a0a0a0;"
                "}"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                "    background: none;"
                "    border: none;"
                "}"
            );
        }
        if (pitchGridVerticalScrollBar) {
            pitchGridVerticalScrollBar->setStyleSheet(
                "QScrollBar:vertical {"
                "    background: rgba(224, 224, 224, 128);"
                "    width: 16px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(192, 192, 192, 180);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(160, 160, 160, 200);"
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
                "}"
            );
        }
    } else {
        // По умолчанию - тёмная схема
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("QWidget { background-color: #404040; }");
        }
        // rulerWidget больше не существует в новой структуре UI

        // Применяем тёмные стили к скроллбарам для темы по умолчанию
        if (horizontalScrollBar) {
            horizontalScrollBar->setStyleSheet(
                "QScrollBar:horizontal {"
                "    background: #404040;"
                "    height: 16px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:horizontal {"
                "    background: #606060;"
                "    min-width: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:horizontal:hover {"
                "    background: #707070;"
                "}"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                "    background: none;"
                "    border: none;"
                "}"
            );
        }
        if (pitchGridVerticalScrollBar) {
            pitchGridVerticalScrollBar->setStyleSheet(
                "QScrollBar:vertical {"
                "    background: rgba(64, 64, 64, 128);"
                "    width: 16px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(96, 96, 96, 180);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(112, 112, 112, 200);"
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
                "}"
            );
        }
    }

    // Initialize timer for time updates
    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    playbackTimer->setInterval(33); // ~30 fps

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

MainWindow::~MainWindow()
{
    // Освобождаем UI
    delete ui;

    // Освобождаем таймеры
    if (playbackTimer) {
        playbackTimer->stop();
        delete playbackTimer;
    }
    if (metronomeController) {
        delete metronomeController;
    }

    // Освобождаем аудио компоненты
    if (mediaPlayer) {
        mediaPlayer->stop();
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

        // Обновляем размеры сплиттера в зависимости от видимости питч-сетки
        if (mainSplitter) {
            QWidget* pitchGridContainer = mainSplitter->widget(1);

            if (pitchGridContainer) {
                if (!isPitchGridVisible) {
                    // Если питч-сетка скрыта, убеждаемся что волна занимает всё пространство
                    QList<int> sizes;
                    int totalHeight = mainSplitter->height();
                    sizes << totalHeight << 0;
                    mainSplitter->setSizes(sizes);
                }
            }
        }
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

        // Обновляем размеры сплиттера в зависимости от видимости питч-сетки
        if (mainSplitter) {
            QWidget* pitchGridContainer = mainSplitter->widget(1);

            if (pitchGridContainer) {
                if (!isPitchGridVisible) {
                    // Если питч-сетка скрыта, убеждаемся что волна занимает всё пространство
                    QList<int> sizes;
                    int totalHeight = mainSplitter->height();
                    sizes << totalHeight << 0;
                    mainSplitter->setSizes(sizes);
                }
            }
        }
    });
}

void MainWindow::retranslateMenus()
{
    if (fileMenu) fileMenu->setTitle(tr("&Файл"));
    if (editMenu) editMenu->setTitle(tr("&Правка"));
    if (viewMenu) viewMenu->setTitle(tr("&Вид"));
    if (themesMenu) themesMenu->setTitle(tr("Темы"));
    if (colorSchemeMenu) colorSchemeMenu->setTitle(tr("&Цветовая схема"));
    if (settingsMenu) settingsMenu->setTitle(tr("&Настройки"));
    if (languageMenu) languageMenu->setTitle(tr("Language"));
    if (openAct) openAct->setText(tr("&Открыть..."));
    if (saveAct) saveAct->setText(tr("&Сохранить"));
    if (exitAct) exitAct->setText(tr("&Выход"));
    if (defaultThemeAct) { defaultThemeAct->setText(tr("По умолчанию")); defaultThemeAct->setStatusTip(tr("Использовать тему по умолчанию")); }
    if (darkSchemeAct) { darkSchemeAct->setText(tr("Тёмная")); darkSchemeAct->setStatusTip(tr("Использовать тёмную тему")); }
    if (lightSchemeAct) { lightSchemeAct->setText(tr("Светлая")); lightSchemeAct->setStatusTip(tr("Использовать светлую тему")); }
    if (metronomeSettingsAct) { metronomeSettingsAct->setText(tr("Настройки &метронома...")); metronomeSettingsAct->setStatusTip(tr("Настройки метронома")); }
    if (keyboardShortcutsAct) { keyboardShortcutsAct->setText(tr("&Горячие клавиши...")); keyboardShortcutsAct->setStatusTip(tr("Просмотр горячих клавиш")); }
    if (playPauseAct) playPauseAct->setText(tr("Воспроизведение/Пауза"));
    if (stopAct) stopAct->setText(tr("Стоп"));
    if (metronomeAct) metronomeAct->setText(tr("Метроном"));
    if (loopStartAct) loopStartAct->setText(tr("Установить начало цикла"));
    if (loopEndAct) loopEndAct->setText(tr("Установить конец цикла"));
    if (togglePitchGridAct) { togglePitchGridAct->setStatusTip(tr("Переключить видимость питч-сетки")); if (isPitchGridVisible) togglePitchGridAct->setText(tr("Убрать питч-сетку")); else togglePitchGridAct->setText(tr("Показать питч-сетку")); }
    if (toggleBeatWaveformAct) { toggleBeatWaveformAct->setText(tr("Силуэт ударных")); toggleBeatWaveformAct->setStatusTip(tr("Переключить отображение силуэта ударных поверх волны")); }
    if (undoAct) undoAct->setText(tr("&Отменить"));
    if (redoAct) redoAct->setText(tr("&Повторить"));
    if (russianAction) russianAction->setText(tr("Русский"));
    if (englishAction) englishAction->setText(tr("English"));
    if (applyTimeStretchAct) { applyTimeStretchAct->setText(tr("Применить сжатие-растяжение")); applyTimeStretchAct->setStatusTip(tr("Применить сжатие-растяжение аудио по меткам с тонкомпенсацией")); }
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

    // Восстанавливаем видимость питч-сетки (по умолчанию скрыта)
    isPitchGridVisible = settings.value("pitchGridVisible", false).toBool();

    // Восстанавливаем текущие тональности
    currentKey = settings.value("currentKey", "").toString();
    setKey(currentKey);

    currentKey2 = settings.value("currentKey2", "").toString();
    setKey2(currentKey2);

    // Применяем состояние к сплиттеру
    if (mainSplitter) {
        QWidget* pitchGridContainer = mainSplitter->widget(1);

        if (pitchGridContainer) {
            if (isPitchGridVisible) {
                // Показываем питч-сетку
                pitchGridContainer->setVisible(true);
                mainSplitter->setChildrenCollapsible(false);
            } else {
                // Скрываем питч-сетку
                pitchGridContainer->setVisible(false);
                mainSplitter->setChildrenCollapsible(true);

                // Устанавливаем размеры так, чтобы волна заняла всё пространство
                QList<int> sizes;
                int totalHeight = mainSplitter->height();
                sizes << totalHeight << 0;
                mainSplitter->setSizes(sizes);
            }
        }
    }

    // Обновляем текст действия и состояние (заблокирован/разблокирован)
    if (togglePitchGridAct) {
        if (isPitchGridVisible) {
            togglePitchGridAct->setText(QString::fromUtf8("Убрать питч-сетку"));
            togglePitchGridAct->setEnabled(true); // Разблокируем, если видима
        } else {
            togglePitchGridAct->setText(QString::fromUtf8("Показать питч-сетку"));
            togglePitchGridAct->setEnabled(false); // Блокируем, если скрыта
        }
    }

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
        statusBar()->showMessage(tr("Размер такта изменен на %1").arg(text), 2000);
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

            statusBar()->showMessage(tr("Воспроизведение завершено"), 2000);
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
            // Обновляем позицию каретки в PitchGridWidget при изменении масштаба
            if (pitchGridWidget) {
                float cursorX = waveformView->getCursorXPosition();
                pitchGridWidget->setCursorPosition(cursorX);
                // Обновляем прозрачность скроллбара
                updateScrollBarTransparency();
            }
        });

    // Обновляем горизонтальный скроллбар при изменении смещения
    connect(waveformView, &WaveformView::horizontalOffsetChanged, this,
        [this](float offset) {
            updateHorizontalScrollBarFromOffset(offset);
            // Обновляем позицию каретки в PitchGridWidget при изменении смещения
            if (pitchGridWidget) {
                float cursorX = waveformView->getCursorXPosition();
                pitchGridWidget->setCursorPosition(cursorX);
                // Обновляем прозрачность скроллбара
                updateScrollBarTransparency();
            }
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

        // Синхронизация масштаба и смещения
        connect(waveformView, &WaveformView::zoomChanged, this,
            [this](float zoom) {
                if (pitchGridWidget) {
                    pitchGridWidget->setZoomLevel(zoom);
                    // Обновляем позицию каретки при изменении масштаба
                    float cursorX = waveformView->getCursorXPosition();
                    pitchGridWidget->setCursorPosition(cursorX);
                    // Обновляем прозрачность скроллбара
                    updateScrollBarTransparency();
                }
            });

        // Обновление воспроизведения при завершении перетаскивания метки
        connect(waveformView, &WaveformView::markerDragFinished, this,
            [this]() {
                updatePlaybackAfterMarkerDrag();
            });

    // Любое изменение меток помечает состояние как несохраненное
    connect(waveformView, &WaveformView::markersChanged, this,
        [this]() {
            hasUnsavedChanges = true;
        });

        // Синхронизация горизонтального смещения
        connect(horizontalScrollBar, &QScrollBar::valueChanged, this,
            [this](int value) {
                if (pitchGridWidget) {
                    float maxOffset = waveformView->getZoomLevel() > 1.0f ? 1.0f : 0.0f;
                    float offset = float(value) / float(horizontalScrollBar->maximum());
                    offset = qBound(0.0f, offset, maxOffset);
                    pitchGridWidget->setHorizontalOffset(offset);
                    // Обновляем позицию каретки при изменении смещения
                    float cursorX = waveformView->getCursorXPosition();
                    pitchGridWidget->setCursorPosition(cursorX);
                    // Обновляем прозрачность скроллбара
                    updateScrollBarTransparency();
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
    ui->loopStartButton->setToolTip(tr("ЛКМ: Установить точку A (начало цикла)\nПКМ: Удалить точку A\nA: Установить точку A\nShift+A: Удалить точку A"));
    ui->loopEndButton->setToolTip(tr("ЛКМ: Установить точку B (конец цикла)\nПКМ: Удалить точку B\nB: Установить точку B\nShift+B: Удалить точку B"));

    connect(ui->loopStartButton, &QPushButton::pressed, this, [this]() {
        if (loopStartPosition > 0) {
            statusBar()->showMessage(tr("Точка A: %1").arg(formatTime(loopStartPosition)), 2000);
        }
    });

    connect(ui->loopEndButton, &QPushButton::pressed, this, [this]() {
        if (loopEndPosition > 0) {
            statusBar()->showMessage(tr("Точка B: %1").arg(formatTime(loopEndPosition)), 2000);
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

    // Подключаем контекстные меню для полей ввода тональности
    ui->keyInput->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->keyInput, &QLineEdit::customContextMenuRequested, this, &MainWindow::showKeyContextMenu);

    ui->keyInput2->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->keyInput2, &QLineEdit::customContextMenuRequested, this, &MainWindow::showKeyContextMenu2);

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
        statusBar()->showMessage(tr("Воспроизведение..."));

        // Обновляем состояние метронома
        if (metronomeController) {
            metronomeController->setPlaying(true);
        }
    } else {
        isPlaying = false;
        ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
        mediaPlayer->pause();
        playbackTimer->stop();
        statusBar()->showMessage(tr("Пауза"));

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
    statusBar()->showMessage(tr("Остановлено"));
}

void MainWindow::updateTime()
{
    if (isPlaying) {
        currentPosition = mediaPlayer->position();
        ui->timeLabel->setText(formatTimeAndBars(currentPosition));
    }
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
        statusBar()->showMessage(tr("BPM установлен: %1").arg(bpm), 2000);
    } else {
        ui->bpmEdit->setText("120.00");
        statusBar()->showMessage(tr("Неверное значение BPM (допустимый диапазон: 0.01 - 9999.99)"), 3000);
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
    ui->timeLabel->setText(formatTimeAndBars(msPosition));

    // Показываем информацию о цикле в статусной строке
    if (isLoopEnabled && loopStartPosition > 0 && loopEndPosition > 0) {
        QString loopInfo = tr("Цикл: %1 - %2").arg(formatTime(loopStartPosition)).arg(formatTime(loopEndPosition));
        if (msPosition >= loopStartPosition && msPosition <= loopEndPosition) {
            statusBar()->showMessage(loopInfo, 1000);
        }
    }
}

void MainWindow::updatePlaybackPosition(qint64 position)
{
    if (waveformView) {
        // Передаем позицию в миллисекундах, как ожидает WaveformView
        waveformView->setPlaybackPosition(position);
        updateTimeLabel(position);
        updateLoopPoints();

        // Показываем информацию об активном сегменте в статусной строке (если есть метки)
        if (!waveformView->getMarkers().isEmpty()) {
            WaveformView::ActiveSegmentInfo segmentInfo = waveformView->getActiveSegmentInfo();
            if (segmentInfo.isValid) {
                QString segmentText = QString::fromUtf8("Сегмент: %1 - %2 | Коэффициент: %3")
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
    openAct = new QAction(QString::fromUtf8("&Открыть..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openAudioFile);

    saveAct = new QAction(QString::fromUtf8("&Сохранить"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveAudioFile);

    exitAct = new QAction(QString::fromUtf8("&Выход"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Theme actions
    defaultThemeAct = new QAction(QString::fromUtf8("По умолчанию"), this);
    defaultThemeAct->setStatusTip(QString::fromUtf8("Использовать тему по умолчанию"));
    connect(defaultThemeAct, &QAction::triggered, this, [this]() { setTheme("default"); });

    // Color scheme actions
    darkSchemeAct = new QAction(QString::fromUtf8("Тёмная"), this);
    darkSchemeAct->setStatusTip(QString::fromUtf8("Использовать тёмную тему"));
    connect(darkSchemeAct, &QAction::triggered, this, [this]() { setColorScheme("dark"); });

    lightSchemeAct = new QAction(QString::fromUtf8("Светлая"), this);
    lightSchemeAct->setStatusTip(QString::fromUtf8("Использовать светлую тему"));
    connect(lightSchemeAct, &QAction::triggered, this, [this]() { setColorScheme("light"); });

    // Settings actions
    metronomeSettingsAct = new QAction(tr("Настройки &метронома..."), this);
    metronomeSettingsAct->setStatusTip(tr("Настройки метронома"));
    connect(metronomeSettingsAct, &QAction::triggered, this, &MainWindow::showMetronomeSettings);

    keyboardShortcutsAct = new QAction(tr("&Горячие клавиши..."), this);
    keyboardShortcutsAct->setStatusTip(tr("Просмотр горячих клавиш"));
    connect(keyboardShortcutsAct, &QAction::triggered, this, &MainWindow::showKeyboardShortcuts);

    // Playback actions
    playPauseAct = new QAction(tr("Воспроизведение/Пауза"), this);
    playPauseAct->setShortcut(QKeySequence(Qt::Key_Space));
    connect(playPauseAct, &QAction::triggered, this, &MainWindow::playAudio);

    stopAct = new QAction(tr("Стоп"), this);
    stopAct->setShortcut(QKeySequence(Qt::Key_S));
    connect(stopAct, &QAction::triggered, this, &MainWindow::stopAudio);

    // Metronome action
    metronomeAct = new QAction(tr("Метроном"), this);
    metronomeAct->setShortcut(QKeySequence(Qt::Key_T));
    metronomeAct->setCheckable(true);
    connect(metronomeAct, &QAction::triggered, this, &MainWindow::toggleMetronome);

    // Loop actions
    loopStartAct = new QAction(tr("Установить начало цикла"), this);
    loopStartAct->setShortcut(QKeySequence(Qt::Key_A));
    connect(loopStartAct, &QAction::triggered, this, &MainWindow::setLoopStart);

    loopEndAct = new QAction(tr("Установить конец цикла"), this);
    loopEndAct->setShortcut(QKeySequence(Qt::Key_B));
    connect(loopEndAct, &QAction::triggered, this, &MainWindow::setLoopEnd);

    // Pitch grid toggle action (по умолчанию заблокирован и показывает "Показать питч-сетку")
    togglePitchGridAct = new QAction(tr("Показать питч-сетку"), this);
    togglePitchGridAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    togglePitchGridAct->setStatusTip(tr("Переключить видимость питч-сетки"));
    togglePitchGridAct->setEnabled(false); // Заблокирован по умолчанию
    connect(togglePitchGridAct, &QAction::triggered, this, &MainWindow::togglePitchGrid);

    // Beat deviations toggle action
    toggleBeatWaveformAct = new QAction(tr("Силуэт ударных"), this);
    toggleBeatWaveformAct->setCheckable(true);
    toggleBeatWaveformAct->setChecked(true); // Включено по умолчанию
    toggleBeatWaveformAct->setStatusTip(tr("Переключить отображение силуэта ударных поверх волны"));
    connect(toggleBeatWaveformAct, &QAction::triggered, this, &MainWindow::toggleBeatWaveform);

    // Edit actions - use QUndoStack's built-in actions
    undoAct = undoStack->createUndoAction(this);
    undoAct->setText(tr("&Отменить"));
    undoAct->setShortcuts(QKeySequence::Undo);

    redoAct = undoStack->createRedoAction(this);
    redoAct->setText(tr("&Повторить"));
    redoAct->setShortcuts(QKeySequence::Redo);

    // Language actions
    russianAction = new QAction(tr("Русский"), this);
    russianAction->setCheckable(true);
    russianAction->setChecked(true);
    connect(russianAction, &QAction::triggered, this, &MainWindow::setRussianLanguage);

    englishAction = new QAction(tr("English"), this);
    englishAction->setCheckable(true);
    connect(englishAction, &QAction::triggered, this, &MainWindow::setEnglishLanguage);

    // Time stretch action
    applyTimeStretchAct = new QAction(tr("Применить сжатие-растяжение"), this);
    applyTimeStretchAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    applyTimeStretchAct->setStatusTip(tr("Применить сжатие-растяжение аудио по меткам с тонкомпенсацией"));
    connect(applyTimeStretchAct, &QAction::triggered, this, &MainWindow::applyTimeStretch);
}

void MainWindow::createMenus()
{
    // File menu
    fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    // Edit menu
    editMenu = menuBar()->addMenu(tr("&Правка"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(applyTimeStretchAct);
    editMenu->addSeparator();

    // View menu
    viewMenu = menuBar()->addMenu(tr("&Вид"));

    // Themes submenu in View menu
    themesMenu = viewMenu->addMenu(tr("Темы"));
    themesMenu->addAction(defaultThemeAct);

    // Color scheme submenu in View menu
    colorSchemeMenu = viewMenu->addMenu(tr("&Цветовая схема"));
    colorSchemeMenu->addAction(darkSchemeAct);
    colorSchemeMenu->addAction(lightSchemeAct);

    // Add pitch grid toggle to View menu
    viewMenu->addSeparator();
    viewMenu->addAction(togglePitchGridAct);
    viewMenu->addSeparator();
    viewMenu->addAction(toggleBeatWaveformAct);

    // Settings menu (last)
    settingsMenu = menuBar()->addMenu(tr("&Настройки"));
    settingsMenu->addAction(metronomeSettingsAct);
    settingsMenu->addAction(keyboardShortcutsAct);

    // Language submenu in Settings menu
    languageMenu = settingsMenu->addMenu(tr("Language"));
    languageMenu->addAction(russianAction);
    languageMenu->addAction(englishAction);
}

void MainWindow::setupShortcuts()
{
    // Добавляем дополнительную клавишу P для воспроизведения/паузы
    QShortcut* playShortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(playShortcut, &QShortcut::activated, this, &MainWindow::playAudio);

    // Добавляем QShortcut для Shift+A и Shift+B
    QShortcut* shiftAShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_A), this);
    shiftAShortcut->setContext(Qt::ApplicationShortcut);
    connect(shiftAShortcut, &QShortcut::activated, this, [this]() {
        qDebug() << "QShortcut Shift+A activated";
        clearLoopStart();
    });

    QShortcut* shiftBShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_B), this);
    shiftBShortcut->setContext(Qt::ApplicationShortcut);
    connect(shiftBShortcut, &QShortcut::activated, this, [this]() {
        qDebug() << "QShortcut Shift+B activated";
        clearLoopEnd();
    });

    // Добавляем QShortcut для клавиши M (метки)
    QShortcut* markerShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    markerShortcut->setContext(Qt::ApplicationShortcut);
    connect(markerShortcut, &QShortcut::activated, this, [this]() {
        qDebug() << "QShortcut M activated - adding marker";
        if (waveformView) {
            qint64 playbackPos = waveformView->getPlaybackPosition();
            qint64 samplePos = (playbackPos * waveformView->getSampleRate()) / 1000;
            qDebug() << "Adding marker at playbackPos:" << playbackPos << "ms, samplePos:" << samplePos;
            waveformView->addMarker(samplePos);
            qDebug() << "Marker added, total markers:" << waveformView->getMarkers().size();
            statusBar()->showMessage(tr("Метка добавлена"), 2000);
        } else {
            qDebug() << "waveformView is null!";
        }
    });

    // Добавляем действия в окно для обработки клавиш
    addAction(playPauseAct);
    addAction(stopAct);
    addAction(metronomeAct);
    addAction(loopStartAct);
    addAction(loopEndAct);
    addAction(togglePitchGridAct);
}

void MainWindow::showKeyboardShortcuts()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("Горячие клавиши"));
    dialog.setMinimumWidth(400);

    auto layout = new QVBoxLayout(&dialog);
    auto label = new QLabel(&dialog);
    label->setTextFormat(Qt::RichText);
    label->setText(QString::fromUtf8(
        "<h3>Горячие клавиши</h3>"
        "<p><b>Пробел или P</b> - Воспроизведение/Пауза</p>"
        "<p><b>S</b> - Стоп</p>"
        "<p><b>T</b> - Включить/выключить метроном</p>"
        "<p><b>A</b> - Установить точку A (начало цикла)</p>"
        "<p><b>B</b> - Установить точку B (конец цикла)</p>"
        "<p><b>Shift+A</b> - Удалить точку A</p>"
        "<p><b>Shift+B</b> - Удалить точку B</p>"
        "<p><b>Ctrl+Z</b> - Отменить</p>"
        "<p><b>Ctrl+Y</b> - Повторить</p>"
        "<p><b>Ctrl+O</b> - Открыть файл</p>"
        "<p><b>Ctrl+S</b> - Сохранить файл</p>"
        "<p><b>Ctrl+G</b> - Переключить питч-сетку</p>"
        "<p><b>M</b> - Добавить метку в текущей позиции</p>"
    ));

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    layout->addWidget(label);
    layout->addWidget(buttonBox);

    dialog.exec();
}

void MainWindow::setLoopStart()
{
    if (waveformView && mediaPlayer) {
        loopStartPosition = mediaPlayer->position();
        waveformView->setLoopStart(loopStartPosition);

        // Визуально показываем, что точка A установлена
        ui->loopStartButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border: 2px solid #45a049; }");

        statusBar()->showMessage(QString::fromUtf8("Установлена точка A (начало цикла): %1").arg(formatTime(loopStartPosition)), 3000);

        // Если точка B уже установлена, показываем сообщение в статусбаре
        if (loopEndPosition > 0 && loopEndPosition > loopStartPosition) {
            statusBar()->showMessage(tr("Цикл готов! Нажмите кнопку Цикл для включения."), 3000);
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
            statusBar()->showMessage(tr("Ошибка: Точка B должна быть больше точки A!"), 3000);
            loopEndPosition = 0;
            return;
        }

        // Визуально показываем, что точка B установлена
        ui->loopEndButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; border: 2px solid #da190b; }");

        statusBar()->showMessage(QString::fromUtf8("Установлена точка B (конец цикла): %1").arg(formatTime(loopEndPosition)), 3000);

        // Если точка A уже установлена, показываем сообщение в статусбаре
        if (loopStartPosition > 0) {
            statusBar()->showMessage(tr("Цикл готов! Нажмите кнопку Цикл для включения."), 3000);
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

        statusBar()->showMessage(tr("Точка A (начало цикла) удалена"), 2000);
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

        statusBar()->showMessage(tr("Точка B (конец цикла) удалена"), 2000);
        qDebug() << "Loop end cleared successfully";
    } else {
        qDebug() << "waveformView is null";
    }
}

bool MainWindow::maybeSave()
{
    if (!hasUnsavedChanges)
        return true;

    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("DONTFLOAT"),
                             tr("В аудиофайле есть несохраненные изменения.\n"
                                "Хотите сохранить изменения?"),
                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save)
        return doSaveAudioFile();
    else if (ret == QMessageBox::Cancel)
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
}

void MainWindow::openAudioFile()
{
    // Проверяем, есть ли несохраненные изменения
    if (!maybeSave())
        return;

    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Открыть аудиофайл"), "",
        tr("Аудиофайлы (*.wav *.mp3 *.flac);;Все файлы (*)"));

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
        statusBar()->showMessage(tr("Файл загружен: %1").arg(fileName), 2000);
    }
}

void MainWindow::processAudioFile(const QString& filePath)
{
    // Блокируем главное окно ДО загрузки файла
    setEnabled(false);
    QApplication::processEvents(); // Обрабатываем события для обновления UI

    // Создаем диалог сразу, до загрузки файла
    LoadFileDialog dialog(this, BPMAnalyzer::AnalysisResult());
    dialog.setWindowTitle(QString::fromUtf8("Анализ и выравнивание долей"));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.updateProgress(QString::fromUtf8("Загрузка аудиофайла..."), 10);
    dialog.show();
    dialog.raise();
    dialog.activateWindow();
    QApplication::processEvents(); // Принудительно обрабатываем события для показа диалога

    // Загружаем файл
    QVector<QVector<float>> audioData = loadAudioFile(filePath);
    if (!audioData.isEmpty()) {

        // Настраиваем опции анализа
        BPMAnalyzer::AnalysisOptions options;
        options.assumeFixedTempo = true;
        options.fastAnalysis = false;
        options.minBPM = 60.0f;
        options.maxBPM = 200.0f;
        options.tolerancePercent = 5.0f;
        options.useMixxxAlgorithm = true; // Используем алгоритм Mixxx для лучшей точности

        // Анализируем BPM с отображением прогресса
        dialog.updateProgress(QString::fromUtf8("Анализ аудио..."), 50);
        BPMAnalyzer::AnalysisResult analysis = BPMAnalyzer::analyzeBPM(audioData[0], waveformView->getSampleRate(), options);

        dialog.updateProgress(QString::fromUtf8("Анализ завершен"), 100);
        dialog.showResult(analysis);
        dialog.setBeatsPerBar(4);  // по умолчанию 4/4, пользователь может изменить в поле

        // Сразу показываем загруженное аудио и обнаруженные биты, чтобы пользователь видел файл и сетку до выбора «Выровнять»/«Пропустить»
        int bpbInitial = dialog.getBeatsPerBar();
        updateUIAfterAnalysis(audioData, analysis, bpbInitial);

        if (dialog.exec() == QDialog::Accepted && dialog.shouldFixBeats()) {
            // То же, что при «Пропустить» + «Оставить метки»: только обновить UI и расставить метки по долям
            int bpb = dialog.getBeatsPerBar();
            updateUIAfterAnalysis(audioData, analysis, bpb);
            createDeviationMarkers(options.tolerancePercent);

            // Настройка зума и смещения (как в ветке «Пропустить»)
            waveformView->setZoomLevel(1.0f);
            const int sampleRate = waveformView->getSampleRate();
            float samplesPerBeat = (60.0f * sampleRate) / analysis.bpm;
            float barLengthInQuarters = (bpb == 6) ? 3.f : (bpb == 12) ? 6.f : float(qMax(1, bpb));
            float samplesPerBar = barLengthInQuarters * samplesPerBeat;
            float offset = float(analysis.gridStartSample) / samplesPerBar;
            offset = offset - floor(offset);
            waveformView->setHorizontalOffset(offset);
        } else {
            // Если пользователь отказался от выравнивания
            int bpb = dialog.getBeatsPerBar();
            updateUIAfterAnalysis(audioData, analysis, bpb);

            if (dialog.keepMarkersOnSkip()) {
                createDeviationMarkers(options.tolerancePercent);
            }

            // Настройка зума и смещения
            waveformView->setZoomLevel(1.0f);
            const int sampleRate = waveformView->getSampleRate();
            float samplesPerBeat = (60.0f * sampleRate) / analysis.bpm;
            float barLengthInQuarters = (bpb == 6) ? 3.f : (bpb == 12) ? 6.f : float(qMax(1, bpb));
            float samplesPerBar = barLengthInQuarters * samplesPerBeat;
            float offset = float(analysis.gridStartSample) / samplesPerBar;
            offset = offset - floor(offset);
            waveformView->setHorizontalOffset(offset);
        }

        // Обновляем подпись времени/тактов под выбранный размер такта
        updateTimeLabel(0);

        // Обновляем скроллбар
        updateHorizontalScrollBar(waveformView->getZoomLevel());

        // Сбрасываем точки цикла при загрузке нового файла
        loopStartPosition = 0;
        loopEndPosition = 0;
        isLoopEnabled = false;
        ui->loopStartButton->setStyleSheet("");
        ui->loopEndButton->setStyleSheet("");
        ui->loopButton->setStyleSheet("");
        ui->loopButton->setChecked(false);

        // Разблокируем главное окно
        setEnabled(true);
    } else {
        // Если файл не загружен, разблокируем окно
        dialog.close();
        setEnabled(true);
        statusBar()->showMessage(tr("Ошибка загрузки файла"), 3000);
    }
}

QVector<QVector<float>> MainWindow::loadAudioFile(const QString& filePath)
{
    QVector<QVector<float>> channels;
    QAudioDecoder decoder;

    // Настраиваем формат аудио
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(format);

    decoder.setSource(QUrl::fromLocalFile(filePath));

    // Проверяем, поддерживается ли формат
    if (!decoder.audioFormat().isValid()) {
        statusBar()->showMessage(tr("Ошибка: неподдерживаемый формат аудио"), 3000);
        return channels;
    }

    QEventLoop loop;
    QVector<float> leftChannel;
    QVector<float> rightChannel;

    // Обработка ошибок декодера
    connect(&decoder, static_cast<void(QAudioDecoder::*)(QAudioDecoder::Error)>(&QAudioDecoder::error),
        [&](QAudioDecoder::Error error) {
            QString errorStr = tr("Ошибка декодирования: ");
            switch (error) {
                case QAudioDecoder::ResourceError:
                    errorStr += tr("файл не найден или недоступен");
                    break;
                case QAudioDecoder::FormatError:
                    errorStr += tr("неподдерживаемый формат");
                    break;
                case QAudioDecoder::AccessDeniedError:
                    errorStr += tr("нет доступа к файлу");
                    break;
                default:
                    errorStr += tr("неизвестная ошибка");
            }
            statusBar()->showMessage(errorStr, 3000);
            loop.quit();
        });

    connect(&decoder, &QAudioDecoder::bufferReady, this,
        [&]() {
            QAudioBuffer buffer = decoder.read();
            if (!buffer.isValid() || buffer.frameCount() == 0) {
                statusBar()->showMessage(tr("Ошибка: некорректные данные в буфере"), 3000);
                return;
            }

            if (buffer.format().sampleFormat() != QAudioFormat::Float) {
                statusBar()->showMessage(tr("Ошибка: неподдерживаемый формат данных"), 3000);
                return;
            }

            const float* data = buffer.constData<float>();
            int frameCount = buffer.frameCount();
            int channelCount = buffer.format().channelCount();

            // Устанавливаем частоту дискретизации для визуализации
            if (waveformView && buffer.format().sampleRate() > 0) {
                waveformView->setSampleRate(buffer.format().sampleRate());
            }

            for (int i = 0; i < frameCount; ++i) {
                leftChannel.append(data[i * channelCount]);
                if (channelCount > 1) {
                    rightChannel.append(data[i * channelCount + 1]);
                }
            }
        });

    connect(&decoder, &QAudioDecoder::finished, &loop, &QEventLoop::quit);

    decoder.start();
    loop.exec();

    if (!leftChannel.isEmpty()) {
        channels.append(leftChannel);
        if (!rightChannel.isEmpty()) {
            channels.append(rightChannel);
        }
    }

    return channels;
}

void MainWindow::saveAudioFile()
{
    doSaveAudioFile();
}

bool MainWindow::doSaveAudioFile()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Сохранить аудиофайл"), "",
        tr("WAV файлы (*.wav);;Все файлы (*)"));

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".wav", Qt::CaseInsensitive)) {
            fileName += ".wav";
        }

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            // Получаем данные из WaveformView
            const QVector<QVector<float>>& audioData = waveformView->getAudioData();
            if (!audioData.isEmpty()) {
                // Заголовок WAV файла
                QDataStream out(&file);
                out.setByteOrder(QDataStream::LittleEndian);

                // RIFF заголовок
                out.writeRawData("RIFF", 4);
                qint32 fileSize = 36 + audioData[0].size() * 2 * audioData.size(); // 2 байта на сэмпл
                out << fileSize;
                out.writeRawData("WAVE", 4);

                // fmt подчанк
                out.writeRawData("fmt ", 4);
                qint32 fmtSize = 16;
                out << fmtSize;
                qint16 audioFormat = 1; // PCM
                out << audioFormat;
                qint16 numChannels = audioData.size();
                out << numChannels;
                qint32 sampleRate = waveformView->getSampleRate();
                out << sampleRate;
                qint32 byteRate = sampleRate * numChannels * 2;
                out << byteRate;
                qint16 blockAlign = numChannels * 2;
                out << blockAlign;
                qint16 bitsPerSample = 16;
                out << bitsPerSample;

                // data подчанк
                out.writeRawData("data", 4);
                qint32 dataSize = audioData[0].size() * 2 * numChannels;
                out << dataSize;

                // Записываем сэмплы
                for (int i = 0; i < audioData[0].size(); ++i) {
                    for (int channel = 0; channel < numChannels; ++channel) {
                        float sample = audioData[channel][i];
                        qint16 pcmSample = qint16(sample * 32767.0f);
                        out << pcmSample;
                    }
                }

                file.close();
                hasUnsavedChanges = false;
                statusBar()->showMessage(tr("Файл сохранен: %1").arg(fileName), 2000);
                return true;
            } else {
                statusBar()->showMessage(tr("Ошибка: нет данных для сохранения"), 2000);
                return false;
            }
        } else {
            QMessageBox::warning(this, tr("DONTFLOAT"),
                               tr("Не удалось сохранить файл %1:\n%2.")
                               .arg(QDir::toNativeSeparators(fileName),
                                   file.errorString()));
            return false;
        }
    }
    return false;
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
    if (event->key() == Qt::Key_Space) {
        playAudio();
    } else if (event->key() == Qt::Key_S) {
        stopAudio();
    } else if (event->key() == Qt::Key_A) {
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
    } else if (event->key() == Qt::Key_M) {
        // Добавляем метку в текущей позиции воспроизведения
        if (waveformView) {
            qint64 playbackPos = waveformView->getPlaybackPosition();
            qint64 samplePos = (playbackPos * waveformView->getSampleRate()) / 1000;
            qDebug() << "Adding marker at playbackPos:" << playbackPos << "ms, samplePos:" << samplePos;
            waveformView->addMarker(samplePos);
            qDebug() << "Marker added, total markers:" << waveformView->getMarkers().size();
            statusBar()->showMessage(tr("Метка добавлена"), 2000);
        }
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
    statusBar()->showMessage(tr("Файл загружен: %1").arg(fileName), 2000);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
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
        statusBar()->showMessage(tr("Метроном включен"), 2000);
    } else {
        statusBar()->showMessage(tr("Метроном выключен"), 2000);
    }
}

void MainWindow::toggleLoop()
{
    // Проверяем, что точки A и B установлены
    if (loopStartPosition <= 0 || loopEndPosition <= 0) {
        statusBar()->showMessage(tr("Ошибка: Сначала установите точки A и B для цикла!"), 3000);
        ui->loopButton->setChecked(false);
        return;
    }

    isLoopEnabled = !isLoopEnabled;
    ui->loopButton->setChecked(isLoopEnabled);

    if (isLoopEnabled) {
        // Включаем цикл
        ui->loopButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border: 2px solid #1976D2; }");
        statusBar()->showMessage(tr("Цикл включен: %1 - %2").arg(formatTime(loopStartPosition)).arg(formatTime(loopEndPosition)), 3000);
    } else {
        // Выключаем цикл
        ui->loopButton->setStyleSheet("");
        statusBar()->showMessage(tr("Цикл выключен"), 2000);
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
            statusBar()->showMessage(tr("Цикл: %1 - %2").arg(formatTime(loopStartPosition)).arg(formatTime(loopEndPosition)), 1000);
        }
    }
}



void MainWindow::showMetronomeSettings()
{
    // Передаем контроллер в диалог для прямой синхронизации
    MetronomeSettingsDialog dialog(this, metronomeController);
    if (dialog.exec() == QDialog::Accepted) {
        // Настройки уже применены к контроллеру в saveSettings() диалога
        statusBar()->showMessage(tr("Настройки метронома обновлены"), 2000);
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

        // Применяем цвета по умолчанию к виджетам
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("QWidget { background-color: #404040; }");
        }
        // rulerWidget больше не существует в новой структуре UI
    } else {
        // System theme
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(QApplication::style()->standardPalette());

        // Сбрасываем стили виджетов для системной темы
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("");
        }
        // rulerWidget больше не существует в новой структуре UI

        // Сбрасываем стили скроллбаров для системной темы
        if (horizontalScrollBar) {
            horizontalScrollBar->setStyleSheet("");
        }

        if (pitchGridVerticalScrollBar) {
            pitchGridVerticalScrollBar->setStyleSheet("");
        }
    }

    settings.setValue("theme", theme);
    statusBar()->showMessage(tr("Тема изменена: %1").arg(theme), 2000);
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

        // Применяем тёмные цвета к виджетам
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("QWidget { background-color: #2b2b2b; }");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("QWidget { background-color: #404040; }");
        }
        // rulerWidget больше не существует в новой структуре UI

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

        // Применяем светлые цвета к виджетам
        if (ui->waveformWidget) {
            ui->waveformWidget->setStyleSheet("QWidget { background-color: #f5f5f5; }");
        }
        if (ui->pitchGridWidget) {
            ui->pitchGridWidget->setStyleSheet("QWidget { background-color: #f5f5f5; }");
        }
        if (ui->scrollBarWidget) {
            ui->scrollBarWidget->setStyleSheet("QWidget { background-color: #e0e0e0; }");
        }
        // rulerWidget больше не существует в новой структуре UI
    }

    // Применяем схему к WaveformView
    if (waveformView) {
        waveformView->setColorScheme(scheme);
    }

    // Применяем схему к PitchGridWidget
    if (pitchGridWidget) {
        pitchGridWidget->setColorScheme(scheme);
    }

    // Обновляем стили скроллбаров
    if (scheme == "dark") {
        if (horizontalScrollBar) {
            horizontalScrollBar->setStyleSheet(
                "QScrollBar:horizontal {"
                "    background: #404040;"
                "    height: 20px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:horizontal {"
                "    background: #606060;"
                "    min-width: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:horizontal:hover {"
                "    background: #707070;"
                "}"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                "    background: none;"
                "    border: none;"
                "}"
            );
        }
        if (pitchGridVerticalScrollBar) {
            pitchGridVerticalScrollBar->setStyleSheet(
                "QScrollBar:vertical {"
                "    background: rgba(64, 64, 64, 128);"
                "    width: 16px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(96, 96, 96, 180);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(112, 112, 112, 200);"
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
                "}"
            );
        }
    } else if (scheme == "light") {
        if (horizontalScrollBar) {
            horizontalScrollBar->setStyleSheet(
                "QScrollBar:horizontal {"
                "    background: #e0e0e0;"
                "    height: 20px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:horizontal {"
                "    background: #c0c0c0;"
                "    min-width: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:horizontal:hover {"
                "    background: #a0a0a0;"
                "}"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                "    background: none;"
                "    border: none;"
                "}"
            );
        }

        if (pitchGridVerticalScrollBar) {
            pitchGridVerticalScrollBar->setStyleSheet(
                "QScrollBar:vertical {"
                "    background: rgba(224, 224, 224, 128);"
                "    width: 16px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(192, 192, 192, 180);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(160, 160, 160, 200);"
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
                "}"
            );
        }
    } else {
        // По умолчанию - тёмная схема
        if (horizontalScrollBar) {
            horizontalScrollBar->setStyleSheet(
                "QScrollBar:horizontal {"
                "    background: #404040;"
                "    height: 20px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:horizontal {"
                "    background: #606060;"
                "    min-width: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:horizontal:hover {"
                "    background: #707070;"
                "}"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                "    background: none;"
                "    border: none;"
                "}"
            );
        }

        if (pitchGridVerticalScrollBar) {
            pitchGridVerticalScrollBar->setStyleSheet(
                "QScrollBar:vertical {"
                "    background: rgba(64, 64, 64, 128);"
                "    width: 16px;"
                "    border: none;"
                "    border-radius: 8px;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: rgba(96, 96, 96, 180);"
                "    min-height: 20px;"
                "    border-radius: 8px;"
                "    margin: 2px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: rgba(112, 112, 112, 200);"
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
                "}"
            );
        }
    }

    settings.setValue("colorScheme", scheme);
    statusBar()->showMessage(tr("Цветовая схема изменена: %1").arg(scheme == "dark" ? "Тёмная" : "Светлая"), 2000);
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

void MainWindow::createDeviationMarkers(float tolerancePercent)
{
    if (!waveformView) {
        return;
    }

    // Получаем информацию о долях из WaveformView
    QVector<BPMAnalyzer::BeatInfo> beats = waveformView->getBeatInfo();
    if (beats.isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("Информация о долях отсутствует"), 3000);
        return;
    }

    // Получаем BPM и sampleRate
    float bpm = waveformView->getBPM();
    int sampleRate = waveformView->getSampleRate();

    if (bpm <= 0 || sampleRate <= 0) {
        statusBar()->showMessage(QString::fromUtf8("Некорректные параметры BPM или sampleRate"), 3000);
        return;
    }

    // Вычисляем отклонения для всех долей
    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    // Находим неровные доли
    float deviationThreshold = tolerancePercent / 100.0f; // Преобразуем проценты в доли
    QVector<int> unalignedIndices = BPMAnalyzer::findUnalignedBeats(beats, deviationThreshold);

    if (unalignedIndices.isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("Неровные доли не найдены"), 3000);
        return;
    }

    int markersCreated = 0;

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

        // Метки коррекции: position = фактическая доля, originalPosition = ожидаемая по сетке,
        // чтобы коэффициент сегмента был (actual_interval / expected_interval) != 1.
        Marker startMarker(prevBeat.position, sampleRate);
        startMarker.originalPosition = prevBeat.expectedPosition;
        startMarker.originalTimeMs = TimeUtils::samplesToMs(prevBeat.expectedPosition, sampleRate);
        waveformView->addMarker(startMarker);

        Marker endMarker(currentBeat.position, sampleRate);
        endMarker.originalPosition = currentBeat.expectedPosition;
        endMarker.originalTimeMs = TimeUtils::samplesToMs(currentBeat.expectedPosition, sampleRate);
        waveformView->addMarker(endMarker);

        markersCreated++;
    }

    // Сортируем метки по позиции
    waveformView->sortMarkers();

    statusBar()->showMessage(
        QString::fromUtf8("Создано %1 пар меток коррекции для %2 неровных долей")
            .arg(markersCreated)
            .arg(unalignedIndices.size()),
        5000);

    waveformView->update();
}

QString MainWindow::formatTime(qint64 msPosition)
{
    int minutes = (msPosition / 60000);
    int seconds = (msPosition / 1000) % 60;
    int milliseconds = msPosition % 1000;
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds / 100);
}

QString MainWindow::formatTimeAndBars(qint64 msPosition)
{
    int minutes = (msPosition / 60000);
    int seconds = (msPosition / 1000) % 60;
    int milliseconds = msPosition % 1000;
    QString timeStr = QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0'));
    float bpm = ui->bpmEdit->text().toFloat();
    int bpb = 4;
    if (waveformView) {
        bpb = qMax(1, waveformView->getBeatsPerBar());
    } else {
        // До загрузки файла берём размер такта из комбобокса
        QVariant v = ui->barsCombo->currentData();
        if (v.isValid()) {
            int num = v.toInt();
            if (num >= 1 && num <= 32) bpb = num;
        }
    }
    if (bpm <= 0.0f) {
        return timeStr + " | --.--.--";
    }
    int sampleRate = waveformView ? waveformView->getSampleRate() : 44100;
    qint64 gridStart = waveformView ? waveformView->getGridStartSample() : 0;
    qint64 samplePos = (msPosition * qint64(sampleRate)) / 1000;
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    float barLengthInQuarters = (bpb == 6) ? 3.f : (bpb == 12) ? 6.f : float(qMax(1, bpb));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    // Доли в такте по знаменателю: X/4 → 4, X/8 → 8
    int subdivisionsPerBar = (bpb == 6 || bpb == 12) ? 8 : 4;
    float samplesPerSubdivision = samplesPerBar / float(subdivisionsPerBar);
    float subsFromStart;
    if (gridStart > 0 && waveformView) {
        subsFromStart = float(samplePos - gridStart) / samplesPerSubdivision;
        if (subsFromStart < 0.0f) subsFromStart = 0.0f;
    } else {
        subsFromStart = float(samplePos) / samplesPerSubdivision;
    }
    int bar = int(subsFromStart / subdivisionsPerBar) + 1;
    int beat = int(subsFromStart) % subdivisionsPerBar + 1;
    float subBeat = (subsFromStart - float(int(subsFromStart))) * float(subdivisionsPerBar);
    int sub = qBound(1, int(subBeat + 1), subdivisionsPerBar);
    QString barsStr = QString("%1.%2.%3").arg(bar).arg(beat).arg(sub);
    return timeStr + " | " + barsStr;
}

void MainWindow::togglePitchGrid()
{
    isPitchGridVisible = !isPitchGridVisible;

    if (mainSplitter) {
        QWidget* pitchGridContainer = mainSplitter->widget(1); // pitchGridContainer - второй виджет в сплиттере

        if (pitchGridContainer) {
            if (isPitchGridVisible) {
                // Показываем питч-сетку и восстанавливаем сплиттер
                pitchGridContainer->setVisible(true);
                mainSplitter->setChildrenCollapsible(false);

                // Восстанавливаем пропорции сплиттера (75% для волны, 25% для питч-сетки)
                QList<int> sizes;
                int totalHeight = mainSplitter->height();
                sizes << static_cast<int>(totalHeight * 0.75) << static_cast<int>(totalHeight * 0.25);
                mainSplitter->setSizes(sizes);
            } else {
                // Скрываем питч-сетку и делаем волну на весь экран
                pitchGridContainer->setVisible(false);
                mainSplitter->setChildrenCollapsible(true);

                // Устанавливаем размеры так, чтобы волна заняла всё пространство
                QList<int> sizes;
                int totalHeight = mainSplitter->height();
                sizes << totalHeight << 0; // Вся высота для волны, 0 для питч-сетки
                mainSplitter->setSizes(sizes);
            }
        }
    }

    // Обновляем текст действия и состояние (заблокирован/разблокирован)
    if (isPitchGridVisible) {
        togglePitchGridAct->setText(QString::fromUtf8("Убрать питч-сетку"));
        togglePitchGridAct->setEnabled(true); // Разблокируем, если видима
    } else {
        togglePitchGridAct->setText(QString::fromUtf8("Показать питч-сетку"));
        togglePitchGridAct->setEnabled(false); // Блокируем, если скрыта
    }

    // Сохраняем настройку
    settings.setValue("pitchGridVisible", isPitchGridVisible);
}

void MainWindow::createKeyContextMenu()
{
    keyContextMenu = new QMenu(this);

    // Создаем действия для всех тональностей
    QStringList majorKeys = {
        "C Major", "C# Major", "D Major", "D# Major", "E Major", "F Major",
        "F# Major", "G Major", "G# Major", "A Major", "A# Major", "B Major"
    };

    QStringList minorKeys = {
        "C Minor", "C# Minor", "D Minor", "D# Minor", "E Minor", "F Minor",
        "F# Minor", "G Minor", "G# Minor", "A Minor", "A# Minor", "B Minor"
    };

    // Добавляем мажорные тональности
    QMenu* majorMenu = keyContextMenu->addMenu("Мажорные");
    for (int i = 0; i < majorKeys.size(); ++i) {
        keyActions[i] = new QAction(majorKeys[i], this);
        keyActions[i]->setData(majorKeys[i]);
        connect(keyActions[i], &QAction::triggered, this, [this, majorKeys, i]() {
            setKey(majorKeys[i]);
        });
        majorMenu->addAction(keyActions[i]);
    }

    // Добавляем минорные тональности
    QMenu* minorMenu = keyContextMenu->addMenu("Минорные");
    for (int i = 0; i < minorKeys.size(); ++i) {
        keyActions[i + 12] = new QAction(minorKeys[i], this);
        keyActions[i + 12]->setData(minorKeys[i]);
        connect(keyActions[i + 12], &QAction::triggered, this, [this, minorKeys, i]() {
            setKey(minorKeys[i]);
        });
        minorMenu->addAction(keyActions[i + 12]);
    }

    // Добавляем разделитель и опцию "Не определена"
    keyContextMenu->addSeparator();
    QAction* unknownAction = new QAction("Не определена", this);
    connect(unknownAction, &QAction::triggered, this, [this]() {
        setKey("");
    });
    keyContextMenu->addAction(unknownAction);
}

void MainWindow::createKeyContextMenu2()
{
    keyContextMenu2 = new QMenu(this);

    // Создаем действия для всех тональностей
    QStringList majorKeys = {
        "C Major", "C# Major", "D Major", "D# Major", "E Major", "F Major",
        "F# Major", "G Major", "G# Major", "A Major", "A# Major", "B Major"
    };

    QStringList minorKeys = {
        "C Minor", "C# Minor", "D Minor", "D# Minor", "E Minor", "F Minor",
        "F# Minor", "G Minor", "G# Minor", "A Minor", "A# Minor", "B Minor"
    };

    // Добавляем мажорные тональности
    QMenu* majorMenu = keyContextMenu2->addMenu("Мажорные");
    for (int i = 0; i < majorKeys.size(); ++i) {
        keyActions2[i] = new QAction(majorKeys[i], this);
        keyActions2[i]->setData(majorKeys[i]);
        connect(keyActions2[i], &QAction::triggered, this, [this, majorKeys, i]() {
            setKey2(majorKeys[i]);
        });
        majorMenu->addAction(keyActions2[i]);
    }

    // Добавляем минорные тональности
    QMenu* minorMenu = keyContextMenu2->addMenu("Минорные");
    for (int i = 0; i < minorKeys.size(); ++i) {
        keyActions2[i + 12] = new QAction(minorKeys[i], this);
        keyActions2[i + 12]->setData(minorKeys[i]);
        connect(keyActions2[i + 12], &QAction::triggered, this, [this, minorKeys, i]() {
            setKey2(minorKeys[i]);
        });
        minorMenu->addAction(keyActions2[i + 12]);
    }

    // Добавляем разделитель и опцию "Не определена"
    keyContextMenu2->addSeparator();
    QAction* unknownAction = new QAction("Не определена", this);
    connect(unknownAction, &QAction::triggered, this, [this]() {
        setKey2("");
    });
    keyContextMenu2->addAction(unknownAction);
}

void MainWindow::analyzeKey()
{
    if (!waveformView || waveformView->getAudioData().isEmpty()) {
        statusBar()->showMessage(tr("Сначала загрузите аудиофайл"), 3000);
        return;
    }

    // Получаем аудиоданные
    const QVector<QVector<float>>& audioData = waveformView->getAudioData();
    if (audioData.isEmpty()) {
        statusBar()->showMessage(tr("Нет аудиоданных для анализа"), 3000);
        return;
    }

    // Берем первый канал для анализа
    const QVector<float>& samples = audioData[0];
    int sampleRate = waveformView->getSampleRate();

    // Показываем сообщение о начале анализа
    statusBar()->showMessage(tr("Анализ тональности..."), 0);

    // Выполняем анализ в отдельном потоке (упрощенная версия)
    QTimer::singleShot(100, this, [this, samples, sampleRate]() {
        // Здесь должен быть вызов KeyAnalyzer::analyzeKey
        // Пока что используем заглушку
        QString detectedKey = "C Major"; // Заглушка

        setKey(detectedKey);
        statusBar()->showMessage(tr("Тональность определена: %1").arg(detectedKey), 3000);
    });
}

void MainWindow::showKeyContextMenu(const QPoint& pos)
{
    if (keyContextMenu) {
        keyContextMenu->exec(ui->keyInput->mapToGlobal(pos));
    }
}

void MainWindow::showKeyContextMenu2(const QPoint& pos)
{
    if (keyContextMenu2) {
        keyContextMenu2->exec(ui->keyInput2->mapToGlobal(pos));
    }
}

void MainWindow::setKey(const QString& key)
{
    currentKey = key;
    ui->keyInput->setText(key.isEmpty() ? "Не определена" : key);

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
}

void MainWindow::setKey2(const QString& key)
{
    currentKey2 = key;
    ui->keyInput2->setText(key.isEmpty() ? "Модуляция" : key);

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
}

void MainWindow::updateScrollBarTransparency()
{
    if (!pitchGridVerticalScrollBar || !waveformView) return;

    // Получаем позицию каретки воспроизведения
    qint64 playbackPos = waveformView->getPlaybackPosition();
    if (playbackPos < 0) return;

    // Вычисляем позицию каретки в пикселях относительно скроллбара
    // QRect scrollBarRect = pitchGridVerticalScrollBar->geometry(); // Не используется
    QRect pitchGridRect = pitchGridWidget->geometry();

    // Используем позицию каретки напрямую от WaveformView
    float cursorX = waveformView->getCursorXPosition();

    // Вычисляем расстояние от каретки до скроллбара
    float distanceToScrollBar = qAbs(cursorX - pitchGridRect.width());

    // Определяем прозрачность на основе расстояния
    int alpha = 128; // Базовая прозрачность (50%)

    if (distanceToScrollBar < 50) { // Если каретка близко к скроллбару
        alpha = 13; // 5% прозрачности (255 * 0.05 ≈ 13)
    } else if (distanceToScrollBar < 100) { // Если каретка в средней зоне
        alpha = 64; // 25% прозрачности
    }

    // Применяем новую прозрачность к скроллбару
    QString currentScheme = settings.value("colorScheme", "dark").toString();
    if (currentScheme == "dark") {
        pitchGridVerticalScrollBar->setStyleSheet(
            QString("QScrollBar:vertical {"
                "    background: rgba(64, 64, 64, %1);"
                "    width: 16px;"
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
            .arg(qMin(255, alpha + 50))
            .arg(qMin(255, alpha + 70))
        );
    } else {
        pitchGridVerticalScrollBar->setStyleSheet(
            QString("QScrollBar:vertical {"
                "    background: rgba(224, 224, 224, %1);"
                "    width: 16px;"
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
            .arg(qMin(255, alpha + 50))
            .arg(qMin(255, alpha + 70))
        );
    }
}

void MainWindow::setRussianLanguage()
{
    if (russianAction->isChecked() && englishAction->isChecked() == false)
        return;
    const QString path = QCoreApplication::applicationDirPath();
    qApp->removeTranslator(m_appTranslator);
    if (!m_appTranslator->load("DONTFLOAT_ru_RU", path) && !m_appTranslator->load("DONTFLOAT_ru_RU", path + "/../")) {
        qApp->installTranslator(m_appTranslator);
        return;
    }
    qApp->installTranslator(m_appTranslator);
    settings.setValue("language", "ru_RU");
    russianAction->setChecked(true);
    englishAction->setChecked(false);
    ui->retranslateUi(this);
    retranslateMenus();
    statusBar()->showMessage(tr("Язык: Русский"), 2000);
}

void MainWindow::setEnglishLanguage()
{
    if (englishAction->isChecked() && russianAction->isChecked() == false)
        return;
    const QString path = QCoreApplication::applicationDirPath();
    qApp->removeTranslator(m_appTranslator);
    if (!m_appTranslator->load("DONTFLOAT_en_US", path) && !m_appTranslator->load("DONTFLOAT_en_US", path + "/../")) {
        qApp->installTranslator(m_appTranslator);
        return;
    }
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
        statusBar()->showMessage(QString::fromUtf8("Ошибка: WaveformView не инициализирован"), 3000);
        return;
    }

    // Получаем текущие метки
    QVector<Marker> currentMarkers = waveformView->getMarkers();

    if (currentMarkers.size() < 2) {
        QMessageBox::warning(this,
                            QString::fromUtf8("Недостаточно меток"),
                            QString::fromUtf8("Для применения сжатия-растяжения необходимо минимум 2 метки.\n"
                                            "Используйте клавишу M для добавления меток."));
        return;
    }

    // Получаем исходные данные
    const QVector<QVector<float>>& oldData = waveformView->getAudioData();

    if (oldData.isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("Ошибка: нет загруженного аудио"), 3000);
        return;
    }

    // Применяем сжатие-растяжение (теперь возвращает структуру с данными и метками)
    TimeStretchProcessor::StretchResult stretchResult = waveformView->applyTimeStretch(currentMarkers);
    QVector<QVector<float>> newData = stretchResult.audioData;

    // Конвертируем MarkerData → Marker для WaveformView
    QVector<Marker> newMarkers;
    newMarkers.reserve(stretchResult.newMarkers.size());
    for (const MarkerData& md : stretchResult.newMarkers) {
        Marker m;
        // Копируем data-поля
        static_cast<MarkerData&>(m) = md;
        // UI-поля остаются по умолчанию (isDragging=false, dragStartSample=0)
        newMarkers.append(m);
    }

    if (newData.isEmpty() || newData[0].isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("Ошибка при обработке аудио"), 3000);
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
        QString::fromUtf8("Применить сжатие-растяжение")
    );

    // Применяем команду (push автоматически вызывает redo())
    // redo() уже обновит originalAudioData через updateOriginalData()
    undoStack->push(command);

    hasUnsavedChanges = true;

    // Явно обновляем визуализацию после применения эффекта
    if (waveformView) {
        waveformView->update();
    }

    // Переключаем QMediaPlayer на обработанное аудио:
    // сохраняем newData во временный WAV-файл и загружаем его.
    QString tempWavPath = saveProcessedAudioToTempWav(newData, sampleRate);
    if (!tempWavPath.isEmpty() && mediaPlayer) {
        mediaPlayer->setSource(QUrl::fromLocalFile(tempWavPath));
        mediaPlayer->setPosition(0);

        // Если было воспроизведение, останавливаем и сбрасываем позицию
        if (isPlaying) {
            mediaPlayer->stop();
            isPlaying = false;
            if (playbackTimer) {
                playbackTimer->stop();
            }
        }
    }

    statusBar()->showMessage(QString::fromUtf8("Сжатие-растяжение применено успешно. Размер: %1 → %2 сэмплов")
                             .arg(oldData.isEmpty() ? 0 : oldData[0].size())
                             .arg(newData.isEmpty() ? 0 : newData[0].size()), 5000);
}

void MainWindow::updatePlaybackAfterMarkerDrag()
{
    if (!waveformView || !mediaPlayer) {
        return;
    }

    // Пересчитываем аудио по меткам с тонкомпенсацией для воспроизведения
    QVector<Marker> currentMarkers = waveformView->getMarkers();
    TimeStretchProcessor::StretchResult stretchResult = waveformView->applyTimeStretch(currentMarkers);
    const QVector<QVector<float>>& processedData = stretchResult.audioData;

    if (processedData.isEmpty() || processedData[0].isEmpty()) {
        return;
    }

    int sampleRate = waveformView->getSampleRate();

    // Сохраняем текущую позицию воспроизведения ДО обновления источника
    qint64 currentPosition = mediaPlayer->position();
    qint64 oldDuration = mediaPlayer->duration(); // Старая длительность
    bool wasPlaying = (mediaPlayer->playbackState() == QMediaPlayer::PlayingState);

    // Сохраняем обработанные данные во временный WAV файл
    QString tempWavPath = saveProcessedAudioToTempWav(processedData, sampleRate);
    if (tempWavPath.isEmpty()) {
        qWarning() << "updatePlaybackAfterMarkerDrag: failed to save processed audio";
        return;
    }

    // Обновляем источник воспроизведения
    mediaPlayer->setSource(QUrl::fromLocalFile(tempWavPath));

    // Ждем загрузки нового файла перед восстановлением позиции
    // Используем одноразовый таймер для ожидания готовности медиаплеера
    QTimer::singleShot(100, this, [this, currentPosition, oldDuration, wasPlaying]() {
        if (!mediaPlayer) return;

        qint64 newDuration = mediaPlayer->duration();

        // Восстанавливаем позицию воспроизведения (с учетом новой длины файла)
        if (newDuration > 0 && oldDuration > 0 && currentPosition > 0) {
            // Масштабируем позицию пропорционально новой длине
            // Это приблизительное решение, более точное потребует пересчета позиции по меткам
            float ratio = float(newDuration) / float(oldDuration);
            qint64 newPosition = qint64(currentPosition * ratio);
            newPosition = qBound(qint64(0), newPosition, newDuration);
            mediaPlayer->setPosition(newPosition);
        } else if (newDuration > 0 && currentPosition > 0) {
            // Если старой длительности нет, просто ограничиваем текущую позицию
            mediaPlayer->setPosition(qBound(qint64(0), currentPosition, newDuration));
        } else {
            mediaPlayer->setPosition(0);
        }

        // Если было воспроизведение, продолжаем его
        if (wasPlaying) {
            mediaPlayer->play();
        }
    });

    qDebug() << "updatePlaybackAfterMarkerDrag: updated playback to processed audio, size:"
             << processedData[0].size() << "samples";
}

void MainWindow::toggleBeatWaveform()
{
    if (waveformView) {
        bool currentState = waveformView->getShowBeatWaveform();
        waveformView->setShowBeatWaveform(!currentState);
        toggleBeatWaveformAct->setChecked(!currentState);

        if (!currentState) {
            statusBar()->showMessage(QString::fromUtf8("Силуэт ударных включен"), 2000);
        } else {
            statusBar()->showMessage(QString::fromUtf8("Силуэт ударных отключен"), 2000);
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
}

QVector<Marker> MainWindow::createBeatGridMarkers(const BPMAnalyzer::AnalysisResult& analysis,
                                                  const QVector<QVector<float>>& audioData,
                                                  int beatsPerBar)
{
    QVector<Marker> gridMarkers;
    if (!waveformView || audioData.isEmpty()) return gridMarkers;

    const int sampleRate = waveformView->getSampleRate();
    const qint64 totalSamples = audioData[0].size();
    const qint64 minSegmentSamples = (sampleRate * 50) / 1000; // 50 мс между метками

    if (sampleRate <= 0 || totalSamples <= 0 || analysis.bpm <= 0) {
        return gridMarkers;
    }

    float samplesPerBeat = (60.0f * sampleRate) / analysis.bpm;
    float barLengthInQuarters = (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;

    // Начальная неподвижная метка
    gridMarkers.append(Marker(0, true, sampleRate));

    qint64 lastPos = 0;
    for (int k = 0; ; ++k) {
        qint64 pos = analysis.gridStartSample + qint64(k * samplesPerBar);
        if (pos >= totalSamples) break;
        if (pos > 0 && (pos - lastPos) >= minSegmentSamples) {
            Marker m(pos, sampleRate);
            m.originalPosition = pos;
            m.originalTimeMs = TimeUtils::samplesToMs(pos, sampleRate);
            m.updateTimeFromSamples(sampleRate);
            gridMarkers.append(m);
            lastPos = pos;
        }
    }

    // Конечная метка
    qint64 endPos = totalSamples - 1;
    if (endPos > lastPos) {
        Marker endMarker(endPos, false, true, sampleRate);
        endMarker.originalPosition = endPos;
        endMarker.originalTimeMs = TimeUtils::samplesToMs(endPos, sampleRate);
        endMarker.updateTimeFromSamples(sampleRate);
        gridMarkers.append(endMarker);
    }

    return gridMarkers;
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

QString MainWindow::saveProcessedAudioToTempWav(const QVector<QVector<float>> &data, int sampleRate) const
{
    if (data.isEmpty() || data[0].isEmpty() || sampleRate <= 0) {
        qWarning() << "saveProcessedAudioToTempWav: invalid data or sampleRate";
        return QString();
    }

    int channels = data.size();
    qint64 frames = data[0].size();

    // Проверяем, что все каналы одинаковой длины
    for (int ch = 1; ch < channels; ++ch) {
        if (data[ch].size() != frames) {
            qWarning() << "saveProcessedAudioToTempWav: channel size mismatch";
            return QString();
        }
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty()) {
        tempDir = QDir::currentPath();
    }

    QDir dir(tempDir);
    if (!dir.exists()) {
        dir.mkpath(tempDir);
    }

    QString filePath = dir.filePath("dontfloat_processed.wav");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "saveProcessedAudioToTempWav: cannot open file for writing:" << filePath;
        return QString();
    }

    // Параметры WAV
    const int bitsPerSample = 16;
    const int bytesPerSample = bitsPerSample / 8;
    const int byteRate = sampleRate * channels * bytesPerSample;
    const int blockAlign = channels * bytesPerSample;
    const qint64 dataChunkSize = frames * channels * bytesPerSample;
    const qint64 riffChunkSize = 36 + dataChunkSize;

    // Пишем заголовок WAV (RIFF little-endian)
    auto writeLE32 = [&](quint32 value) {
        char b[4];
        b[0] = static_cast<char>(value & 0xFF);
        b[1] = static_cast<char>((value >> 8) & 0xFF);
        b[2] = static_cast<char>((value >> 16) & 0xFF);
        b[3] = static_cast<char>((value >> 24) & 0xFF);
        file.write(b, 4);
    };
    auto writeLE16 = [&](quint16 value) {
        char b[2];
        b[0] = static_cast<char>(value & 0xFF);
        b[1] = static_cast<char>((value >> 8) & 0xFF);
        file.write(b, 2);
    };

    // RIFF header
    file.write("RIFF", 4);
    writeLE32(static_cast<quint32>(riffChunkSize));
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    writeLE32(16);                    // Subchunk1Size for PCM
    writeLE16(1);                     // AudioFormat PCM
    writeLE16(static_cast<quint16>(channels));
    writeLE32(static_cast<quint32>(sampleRate));
    writeLE32(static_cast<quint32>(byteRate));
    writeLE16(static_cast<quint16>(blockAlign));
    writeLE16(static_cast<quint16>(bitsPerSample));

    // data chunk
    file.write("data", 4);
    writeLE32(static_cast<quint32>(dataChunkSize));

    // Пишем interleaved PCM16 данные
    for (qint64 i = 0; i < frames; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            float sample = data[ch][static_cast<int>(i)];
            // Клэмпим в [-1.0, 1.0] и конвертируем в int16
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            qint16 s = static_cast<qint16>(sample * 32767.0f);
            writeLE16(static_cast<quint16>(s));
        }
    }

    file.close();
    qDebug() << "saveProcessedAudioToTempWav: written" << frames << "frames to" << filePath;
    return filePath;
}
