#include "../include/mainwindow.h"
#include "./ui_mainwindow.h"
#include "../include/beatfixcommand.h"
#include <QtGui/QGuiApplication>
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
#include <QtGui/QKeyEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtMultimedia/QSoundEffect>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDir>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QInputDialog>
#include "../include/bpmanalyzer.h"
#include "../include/bpmfixdialog.h"
#include "../include/metronomesettingsdialog.h"
#include "../include/mixxxbpmanalyzer.h"
#include <QUndoStack>
#include <QtGui/QShortcut>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QRegularExpressionValidator>
#include <cmath>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , waveformView(nullptr)
    , pitchGridWidget(nullptr)
    , horizontalScrollBar(nullptr)
    , waveformVerticalScrollBar(nullptr)
    , pitchGridVerticalScrollBar(nullptr)
    , fileMenu(nullptr)
    , editMenu(nullptr)
    , viewMenu(nullptr)
    , settingsMenu(nullptr)
    , colorSchemeMenu(nullptr)
    , currentFileName("")
    , isPlaying(false)
    , currentPosition(0)
    , playbackTimer(nullptr)
    , mediaPlayer(nullptr)
    , audioOutput(nullptr)
    , settings("DONTFLOAT", "DONTFLOAT")
    , hasUnsavedChanges(false)
    , metronomeTimer(nullptr)
    , metronomeSound(nullptr)
    , isMetronomeEnabled(false)
    , lastBeatTime(0)
    , isLoopEnabled(false)
    , loopStartPosition(0)
    , loopEndPosition(0)
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
    undoStack = new QUndoStack(this);

    createActions();  // Create actions first
    ui->setupUi(this);  // Then setup UI
    createMenus();   // Then create menus

    // Настраиваем валидатор для поля BPM
    QRegularExpressionValidator* bpmValidator = new QRegularExpressionValidator(
        QRegularExpression("^\\d{1,4}(\\.\\d{1,2})?$"), this);
    ui->bpmEdit->setValidator(bpmValidator);

    // Create and setup WaveformView
    waveformView = new WaveformView(this);
    if (!ui->waveformWidget->layout()) {
        ui->waveformWidget->setLayout(new QVBoxLayout());
    }
    ui->waveformWidget->layout()->addWidget(waveformView);
    
    // Create and setup PitchGridWidget
    pitchGridWidget = new PitchGridWidget(this);
    if (!ui->pitchGridWidget->layout()) {
        ui->pitchGridWidget->setLayout(new QVBoxLayout());
    }
    ui->pitchGridWidget->layout()->addWidget(pitchGridWidget);
    
    // Create and setup horizontal scrollbar
    horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar->setMinimum(0);
    horizontalScrollBar->setMaximum(1000);
    horizontalScrollBar->setSingleStep(10);  // Small step
    horizontalScrollBar->setPageStep(100);   // Large step (for PageUp/PageDown)
    if (!ui->scrollBarWidget->layout()) {
        ui->scrollBarWidget->setLayout(new QVBoxLayout());
    }
    ui->scrollBarWidget->layout()->addWidget(horizontalScrollBar);
    
    // Create and setup vertical scrollbar for waveform
    waveformVerticalScrollBar = new QScrollBar(Qt::Vertical, this);
    waveformVerticalScrollBar->setMinimum(0);
    waveformVerticalScrollBar->setMaximum(1000);
    waveformVerticalScrollBar->setSingleStep(10);  // Small step
    waveformVerticalScrollBar->setPageStep(100);   // Large step (for PageUp/PageDown)
    if (!ui->waveformVerticalScrollBarWidget->layout()) {
        ui->waveformVerticalScrollBarWidget->setLayout(new QVBoxLayout());
    }
    ui->waveformVerticalScrollBarWidget->layout()->addWidget(waveformVerticalScrollBar);
    
    // Create and setup vertical scrollbar for pitch grid
    pitchGridVerticalScrollBar = new QScrollBar(Qt::Vertical, this);
    pitchGridVerticalScrollBar->setMinimum(0);
    pitchGridVerticalScrollBar->setMaximum(1000);
    pitchGridVerticalScrollBar->setSingleStep(10);  // Small step
    pitchGridVerticalScrollBar->setPageStep(100);   // Large step (for PageUp/PageDown)
    if (!ui->pitchGridVerticalScrollBarWidget->layout()) {
        ui->pitchGridVerticalScrollBarWidget->setLayout(new QVBoxLayout());
    }
    ui->pitchGridVerticalScrollBarWidget->layout()->addWidget(pitchGridVerticalScrollBar);
    
    // Устанавливаем ширину вертикальных скроллбаров равной высоте горизонтального скроллбара
    int horizontalScrollBarHeight = horizontalScrollBar->sizeHint().height();
    ui->waveformVerticalScrollBarWidget->setMaximumWidth(horizontalScrollBarHeight);
    ui->waveformVerticalScrollBarWidget->setMinimumWidth(horizontalScrollBarHeight);
    ui->pitchGridVerticalScrollBarWidget->setMaximumWidth(horizontalScrollBarHeight);
    ui->pitchGridVerticalScrollBarWidget->setMinimumWidth(horizontalScrollBarHeight);
    
    // Initialize audio
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);

    // Setup connections after all objects are created
    setupConnections();

    // Set application icon
    QIcon appIcon(":/icons/resources/icons/logo.svg");
    setWindowIcon(appIcon);
    QGuiApplication::setWindowIcon(appIcon);

    // Restore window state
    readSettings();
    
    // Initialize timer for time updates
    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    playbackTimer->setInterval(33); // ~30 fps

    // Initial window title
    updateWindowTitle();
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
    if (metronomeTimer) {
        metronomeTimer->stop();
        delete metronomeTimer;
    }

    // Освобождаем аудио компоненты
    if (mediaPlayer) {
        mediaPlayer->stop();
        delete mediaPlayer;
    }
    if (audioOutput) {
        delete audioOutput;
    }
    if (metronomeSound) {
        delete metronomeSound;
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
    if (waveformVerticalScrollBar) {
        delete waveformVerticalScrollBar;
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

void MainWindow::readSettings()
{
    settings.beginGroup("MainWindow");
    
    // Восстановление геометрии окна
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        setWindowState(Qt::WindowMaximized);
    } else {
        restoreGeometry(geometry);
    }
    
    // Восстановление состояния окна (развернуто/свернуто/нормально)
    const QByteArray windowState = settings.value("windowState", QByteArray()).toByteArray();
    if (!windowState.isEmpty()) {
        restoreState(windowState);
    }
    
    // Восстанавливаем цветовую схему
    QString colorScheme = settings.value("colorScheme", "default").toString();
    setColorScheme(colorScheme);
    
    settings.endGroup();
}

void MainWindow::writeSettings()
{
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.endGroup();
}

void MainWindow::setupConnections()
{
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::playAudio);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopAudio);
    connect(ui->bpmEdit, &QLineEdit::editingFinished, this, &MainWindow::updateBPM);
    // Временно закомментировано до пересборки UI
    // После пересборки проекта в Qt Creator раскомментировать эти строки:
    // connect(ui->bpmIncreaseButton, &QPushButton::clicked, this, &MainWindow::increaseBPM);
    // connect(ui->bpmDecreaseButton, &QPushButton::clicked, this, &MainWindow::decreaseBPM);
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, &MainWindow::updatePlaybackPosition);
    
    // Подключаем комбобокс выбора режима отображения
    connect(ui->displayModeCombo, &QComboBox::currentIndexChanged, this,
        [this](int index) {
            if (waveformView) {
                waveformView->setTimeDisplayMode(index == 0);
                waveformView->setBarsDisplayMode(index == 1);
                updateTimeLabel(mediaPlayer->position());
            }
        });
    
    // Связываем скроллбар с WaveformView
    connect(horizontalScrollBar, &QScrollBar::valueChanged, this,
        [this](int value) {
            if (waveformView) {
                float maxOffset = waveformView->getZoomLevel() > 1.0f ? 1.0f : 0.0f;
                float offset = float(value) / float(horizontalScrollBar->maximum());
                offset = qBound(0.0f, offset, maxOffset);
                waveformView->setHorizontalOffset(offset);
            }
        });

    // Связываем вертикальный скроллбар для WaveformView
    connect(waveformVerticalScrollBar, &QScrollBar::valueChanged, this,
        [this](int value) {
            if (waveformView) {
                float offset = float(value) / float(waveformVerticalScrollBar->maximum());
                offset = qBound(0.0f, offset, 1.0f);
                waveformView->setVerticalOffset(offset);
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

    // Обновляем скроллбар при изменении масштаба
    connect(waveformView, &WaveformView::zoomChanged, this,
        [this](float zoom) {
            if (zoom <= 1.0f) {
                horizontalScrollBar->setMaximum(0);
                horizontalScrollBar->setPageStep(1000);
                horizontalScrollBar->setValue(0);
                waveformView->setHorizontalOffset(0.0f);
            } else {
                int total = 10000;
                int page = int(total / zoom);
                horizontalScrollBar->setMaximum(total - page);
                horizontalScrollBar->setPageStep(page);
                
                float currentOffset = waveformView->getHorizontalOffset();
                int value = int(currentOffset * total);
                horizontalScrollBar->setValue(value);
            }
        });

    // Подключаем сигнал изменения позиции от WaveformView
    connect(waveformView, &WaveformView::positionChanged, this,
        [this](qint64 msPosition) {
            if (waveformView) {
                // Теперь WaveformView отправляет позицию в миллисекундах
                mediaPlayer->setPosition(msPosition);
                updateTimeLabel(msPosition);
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
                }
            });

        // Синхронизация масштаба и смещения
        connect(waveformView, &WaveformView::zoomChanged, this,
            [this](float zoom) {
                if (pitchGridWidget) {
                    pitchGridWidget->setZoomLevel(zoom);
                }
            });

        // Синхронизация горизонтального смещения
        connect(horizontalScrollBar, &QScrollBar::valueChanged, this,
            [this](int value) {
                if (pitchGridWidget) {
                    float maxOffset = waveformView->getZoomLevel() > 1.0f ? 1.0f : 0.0f;
                    float offset = float(value) / float(horizontalScrollBar->maximum());
                    offset = qBound(0.0f, offset, maxOffset);
                    pitchGridWidget->setHorizontalOffset(offset);
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
    connect(ui->loopButton, &QPushButton::clicked, this, &MainWindow::toggleLoop);
    
    // Инициализация метронома
    metronomeTimer = new QTimer(this);
    metronomeTimer->setInterval(10); // Проверяем каждые 10мс
    metronomeSound = new QSoundEffect(this);
    
    // Создаем простой звук метронома программно
    createSimpleMetronomeSound();
    
    // Проверяем, загрузился ли звук
    if (metronomeSound->status() != QSoundEffect::Ready) {
        qWarning() << "Failed to load metronome sound:" << metronomeSound->status();
    }
    
    isMetronomeEnabled = false;
    lastBeatTime = 0;
    
    // Инициализация циклов
    isLoopEnabled = false;
    loopStartPosition = 0;
    loopEndPosition = 0;
    
    connect(metronomeTimer, &QTimer::timeout, this, [this]() {
        if (isPlaying && isMetronomeEnabled) {
            float bpm = ui->bpmEdit->text().toFloat();
            qint64 currentTime = mediaPlayer->position();
            qint64 beatInterval = qint64(60000.0f / bpm); // интервал между ударами в мс
            
            if (currentTime - lastBeatTime >= beatInterval) {
                // Просто воспроизводим звук через QSoundEffect
                if (metronomeSound->status() == QSoundEffect::Ready) {
                    metronomeSound->play();
                }
                lastBeatTime = currentTime;
            }
        }
    });

    // Подключаем сигналы стека отмены
    connect(undoStack, &QUndoStack::canUndoChanged, undoAct, &QAction::setEnabled);
    connect(undoStack, &QUndoStack::canRedoChanged, redoAct, &QAction::setEnabled);

    // Добавляем горячие клавиши
    setupShortcuts();
}

void MainWindow::playAudio()
{
    if (!isPlaying) {
        isPlaying = true;
        ui->playButton->setIcon(QIcon(":/icons/resources/icons/pause.svg"));
        mediaPlayer->play();
        playbackTimer->start();
        statusBar()->showMessage(tr("Воспроизведение..."));
    } else {
        isPlaying = false;
        ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
        mediaPlayer->pause();
        playbackTimer->stop();
        statusBar()->showMessage(tr("Пауза"));
    }
}

void MainWindow::stopAudio()
{
    isPlaying = false;
    currentPosition = 0;
    ui->playButton->setIcon(QIcon(":/icons/resources/icons/play.svg"));
    mediaPlayer->stop();
    playbackTimer->stop();
    ui->timeLabel->setText("00:00:000");
    statusBar()->showMessage(tr("Остановлено"));
}

void MainWindow::updateTime()
{
    if (isPlaying) {
        currentPosition = mediaPlayer->position();
        int minutes = (currentPosition / 60000);
        int seconds = (currentPosition / 1000) % 60;
        int milliseconds = currentPosition % 1000;
        ui->timeLabel->setText(QString("%1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(milliseconds / 100)); // Показываем только десятые доли секунды
    }
}

void MainWindow::updateBPM()
{
    bool ok;
    float bpm = ui->bpmEdit->text().toFloat(&ok);
    if (ok && bpm > 0.0f && bpm <= 9999.99f) {
        waveformView->setBPM(bpm);
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
    if (ui->displayModeCombo->currentIndex() == 0) {
        // Режим отображения времени
        int minutes = (msPosition / 60000);
        int seconds = (msPosition / 1000) % 60;
        int milliseconds = msPosition % 1000;
        ui->timeLabel->setText(QString("%1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(milliseconds / 100));
    } else {
        // Режим отображения тактов
        if (waveformView) {
            float beatsPerMinute = ui->bpmEdit->text().toFloat();
            float beatsPerSecond = beatsPerMinute / 60.0f;
            float secondsFromStart = float(msPosition) / 1000.0f;
            float beatsFromStart = beatsPerSecond * secondsFromStart;
            
            int bar = int(beatsFromStart / 4) + 1;
            int beat = int(beatsFromStart) % 4 + 1;
            float subBeat = (beatsFromStart - float(int(beatsFromStart))) * 4.0f;
            
            ui->timeLabel->setText(QString("%1.%2.%3")
                .arg(bar)
                .arg(beat)
                .arg(int(subBeat + 1)));
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
    }
    
    // Обновляем позицию в PitchGridWidget
    if (pitchGridWidget) {
        pitchGridWidget->setPlaybackPosition(position);
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
    metronomeSettingsAct = new QAction(QString::fromUtf8("&Метроном..."), this);
    metronomeSettingsAct->setStatusTip(QString::fromUtf8("Настройки метронома"));
    connect(metronomeSettingsAct, &QAction::triggered, this, &MainWindow::showMetronomeSettings);

    keyboardShortcutsAct = new QAction(QString::fromUtf8("&Горячие клавиши..."), this);
    keyboardShortcutsAct->setStatusTip(QString::fromUtf8("Просмотр горячих клавиш"));
    connect(keyboardShortcutsAct, &QAction::triggered, this, &MainWindow::showKeyboardShortcuts);

    // Playback actions
    playPauseAct = new QAction(QString::fromUtf8("Воспроизведение/Пауза"), this);
    playPauseAct->setShortcut(QKeySequence(Qt::Key_Space));
    connect(playPauseAct, &QAction::triggered, this, &MainWindow::playAudio);

    stopAct = new QAction(QString::fromUtf8("Стоп"), this);
    stopAct->setShortcut(QKeySequence(Qt::Key_S));
    connect(stopAct, &QAction::triggered, this, &MainWindow::stopAudio);

    // Metronome action
    metronomeAct = new QAction(QString::fromUtf8("Метроном"), this);
    metronomeAct->setShortcut(QKeySequence(Qt::Key_M));
    metronomeAct->setCheckable(true);
    connect(metronomeAct, &QAction::triggered, this, &MainWindow::toggleMetronome);

    // Loop actions
    loopStartAct = new QAction(QString::fromUtf8("Установить начало цикла"), this);
    loopStartAct->setShortcut(QKeySequence(Qt::Key_A));
    connect(loopStartAct, &QAction::triggered, this, &MainWindow::setLoopStart);

    loopEndAct = new QAction(QString::fromUtf8("Установить конец цикла"), this);
    loopEndAct->setShortcut(QKeySequence(Qt::Key_B));
    connect(loopEndAct, &QAction::triggered, this, &MainWindow::setLoopEnd);

    // Edit actions - use QUndoStack's built-in actions
    undoAct = undoStack->createUndoAction(this);
    undoAct->setText(QString::fromUtf8("&Отменить"));
    undoAct->setShortcuts(QKeySequence::Undo);

    redoAct = undoStack->createRedoAction(this);
    redoAct->setText(QString::fromUtf8("&Повторить"));
    redoAct->setShortcuts(QKeySequence::Redo);
}

void MainWindow::createMenus()
{
    // File menu
    fileMenu = menuBar()->addMenu(QString::fromUtf8("&Файл"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    // Edit menu
    editMenu = menuBar()->addMenu(QString::fromUtf8("&Правка"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();

    // View menu
    viewMenu = menuBar()->addMenu(QString::fromUtf8("&Вид"));
    
    // Themes submenu in View menu
    QMenu* themesMenu = viewMenu->addMenu(QString::fromUtf8("Темы"));
    themesMenu->addAction(defaultThemeAct);
    
    // Color scheme submenu in View menu
    colorSchemeMenu = viewMenu->addMenu(QString::fromUtf8("Цветовое оформление"));
    colorSchemeMenu->addAction(darkSchemeAct);
    colorSchemeMenu->addAction(lightSchemeAct);
    
    // Settings menu
    settingsMenu = menuBar()->addMenu(QString::fromUtf8("&Настройки"));
    settingsMenu->addAction(metronomeSettingsAct);
    settingsMenu->addAction(keyboardShortcutsAct);
}

void MainWindow::setupShortcuts()
{
    // Добавляем дополнительную клавишу P для воспроизведения/паузы
    QShortcut* playShortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(playShortcut, &QShortcut::activated, this, &MainWindow::playAudio);

    // Добавляем действия в окно для обработки клавиш
    addAction(playPauseAct);
    addAction(stopAct);
    addAction(metronomeAct);
    addAction(loopStartAct);
    addAction(loopEndAct);
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
        "<p><b>M</b> - Включить/выключить метроном</p>"
        "<p><b>A</b> - Установить начало цикла</p>"
        "<p><b>B</b> - Установить конец цикла</p>"
        "<p><b>Ctrl+Z</b> - Отменить</p>"
        "<p><b>Ctrl+Y</b> - Повторить</p>"
        "<p><b>Ctrl+O</b> - Открыть файл</p>"
        "<p><b>Ctrl+S</b> - Сохранить файл</p>"
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
        if (!isLoopEnabled) {
            toggleLoop();
        }
        statusBar()->showMessage(QString::fromUtf8("Установлено начало цикла"), 2000);
    }
}

void MainWindow::setLoopEnd()
{
    if (waveformView && mediaPlayer) {
        loopEndPosition = mediaPlayer->position();
        waveformView->setLoopEnd(loopEndPosition);
        if (!isLoopEnabled) {
            toggleLoop();
        }
        statusBar()->showMessage(QString::fromUtf8("Установлен конец цикла"), 2000);
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
    if (isMetronomeEnabled) {
        toggleMetronome();
    }
    
    // Сбрасываем состояние цикла
    if (isLoopEnabled) {
        toggleLoop();
    }
    
    // Сбрасываем позицию воспроизведения
    currentPosition = 0;
    
    // Обновляем интерфейс
    ui->timeLabel->setText("00:00:000");
    waveformView->setPlaybackPosition(0);
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
    QVector<QVector<float>> audioData = loadAudioFile(filePath);
    if (!audioData.isEmpty()) {
        // Блокируем главное окно
        setEnabled(false);
        
        // Создаем диалог до начала анализа с корректной кодировкой текста
        BPMFixDialog dialog(this, BPMAnalyzer::AnalysisResult());
        dialog.setWindowTitle(QString::fromUtf8("Анализ и выравнивание долей"));
        dialog.setWindowModality(Qt::ApplicationModal);
        dialog.show();
        
        // Настраиваем опции анализа
        BPMAnalyzer::AnalysisOptions options;
        options.assumeFixedTempo = true;
        options.fastAnalysis = false;
        options.minBPM = 60.0f;
        options.maxBPM = 200.0f;
        options.tolerancePercent = 5.0f;
        
        // Анализируем BPM и сетку битов через Mixxx, затем детектим неровности
        dialog.updateProgress(QString::fromUtf8("Анализ аудио (Mixxx)..."), 30);
        QVector<float> interleaved;
        // собрать interleaved стерео
        if (audioData.size() >= 2) {
            const auto& L = audioData[0];
            const auto& R = audioData[1];
            int frames = qMin(L.size(), R.size());
            interleaved.resize(frames * 2);
            for (int i = 0, j = 0; i < frames; ++i) {
                interleaved[j++] = L[i];
                interleaved[j++] = R[i];
            }
        } else {
            // моно -> дублируем
            const auto& M = audioData[0];
            int frames = M.size();
            interleaved.resize(frames * 2);
            for (int i = 0, j = 0; i < frames; ++i) {
                interleaved[j++] = M[i];
                interleaved[j++] = M[i];
            }
        }
        auto mixxxRes = MixxxBpmAnalyzerFacade::analyzeStereoInterleaved(interleaved, waveformView->getSampleRate(), /*fast*/ false);

        BPMAnalyzer::AnalysisOptions opt2 = options;
        float bpmForIrregular = mixxxRes.bpm > 0.0f ? mixxxRes.bpm : options.minBPM;
        dialog.updateProgress(QString::fromUtf8("Детект неровностей..."), 60);
        BPMAnalyzer::AnalysisResult analysis = BPMAnalyzer::analyzeBeatsWithGivenBPM(audioData[0], waveformView->getSampleRate(), bpmForIrregular, opt2);
        if (mixxxRes.supportsBeatTracking && !mixxxRes.beatPositionsFrames.isEmpty()) {
            // Подменим найденные биты Mixxx как идеальные позиции, сохранив bpm
            analysis.bpm = mixxxRes.bpm > 0.0f ? mixxxRes.bpm : analysis.bpm;
            analysis.beats.clear();
            analysis.beats.reserve(mixxxRes.beatPositionsFrames.size());
            for (int posFrames : mixxxRes.beatPositionsFrames) {
                BPMAnalyzer::BeatInfo bi{};
                // posFrames уже в фреймах (1 фрейм = 1 сэмпл на канал)
                bi.position = static_cast<qint64>(posFrames);
                bi.confidence = 1.0f;
                bi.deviation = 0.0f;
                bi.energy = 0.0f;
                analysis.beats.append(bi);
            }
            analysis.isFixedTempo = true;
            analysis.hasIrregularBeats = false;
            analysis.averageDeviation = 0.0f;
        }
        
        dialog.updateProgress(QString::fromUtf8("Анализ завершен"), 100);
        dialog.showResult(analysis);
        
        // Сохраняем оригинальные данные
        QVector<QVector<float>> originalData = audioData;
        
        if (dialog.exec() == QDialog::Accepted && dialog.shouldFixBeats()) {
            dialog.updateProgress(QString::fromUtf8("Выравнивание долей..."), 50);
            
            // Создаем копию для исправленных данных
            QVector<QVector<float>> fixedData = audioData;
            for (int i = 0; i < fixedData.size(); ++i) {
                fixedData[i] = BPMAnalyzer::fixBeats(fixedData[i], analysis);
            }
            
            // Создаем и выполняем команду отмены
            BeatFixCommand* command = new BeatFixCommand(waveformView, originalData, fixedData, analysis.bpm, analysis.beats);
            undoStack->push(command);
            
            dialog.updateProgress(QString::fromUtf8("Выравнивание завершено"), 100);
            statusBar()->showMessage(QString::fromUtf8("Доли выровнены"), 2000);
            hasUnsavedChanges = true;
        } else {
            // Если пользователь отказался от выравнивания, просто обновляем отображение
            waveformView->setAudioData(audioData);
            waveformView->setBeatInfo(analysis.beats);
            ui->bpmEdit->setText(QString::number(analysis.bpm, 'f', 2));
            waveformView->setBPM(analysis.bpm);
            
            // Обновляем PitchGridWidget
            if (pitchGridWidget) {
                pitchGridWidget->setAudioData(audioData);
                pitchGridWidget->setSampleRate(waveformView->getSampleRate());
                pitchGridWidget->setBPM(analysis.bpm);
            }
            
            // Выравниваем отображение по тактовой сетке и сбрасываем масштаб
            waveformView->setZoomLevel(1.0f);
            float samplesPerBeat = (float(waveformView->getSampleRate()) * 60.0f) / analysis.bpm;
            float samplesPerBar = samplesPerBeat * 4.0f;
            if (!analysis.beats.isEmpty()) {
                float offset = float(analysis.beats[0].position) / samplesPerBar;
                offset = offset - floor(offset);
                waveformView->setHorizontalOffset(offset);
            } else {
                waveformView->setHorizontalOffset(0.0f);
            }
        }
        
        // Обновляем скроллбар
        horizontalScrollBar->setValue(0);

        // Разблокируем главное окно
        setEnabled(true);
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
    connect(&decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
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
    } else if (event->key() == Qt::Key_Up && event->modifiers() == Qt::ControlModifier) {
        increaseBPM();
    } else if (event->key() == Qt::Key_Down && event->modifiers() == Qt::ControlModifier) {
        decreaseBPM();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::toggleMetronome()
{
    isMetronomeEnabled = !isMetronomeEnabled;
    ui->metronomeButton->setChecked(isMetronomeEnabled);
    
    if (isMetronomeEnabled) {
        float bpm = ui->bpmEdit->text().toFloat();
        metronomeTimer->start(qint64(60000.0f / bpm / 4)); // Проверяем 4 раза за удар для точности
        statusBar()->showMessage(tr("Метроном включен"), 2000);
    } else {
        metronomeTimer->stop();
        statusBar()->showMessage(tr("Метроном выключен"), 2000);
    }
}

void MainWindow::toggleLoop()
{
    isLoopEnabled = !isLoopEnabled;
    ui->loopButton->setChecked(isLoopEnabled);
    
    if (isLoopEnabled) {
        // Устанавливаем точки цикла
        loopStartPosition = mediaPlayer->position();
        loopEndPosition = waveformView ? 
            (waveformView->getAudioData()[0].size() * 1000LL) / waveformView->getSampleRate() : 
            mediaPlayer->duration();
        
        statusBar()->showMessage(tr("Цикл включен"), 2000);
    } else {
        loopStartPosition = 0;
        loopEndPosition = 0;
        statusBar()->showMessage(tr("Цикл выключен"), 2000);
    }
}

void MainWindow::updateLoopPoints()
{
    if (isLoopEnabled && isPlaying) {
        qint64 position = mediaPlayer->position();
        if (position >= loopEndPosition) {
            mediaPlayer->setPosition(loopStartPosition);
        }
    }
}



void MainWindow::showMetronomeSettings()
{
    MetronomeSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Применяем новые настройки метронома
        if (metronomeSound) {
            int volume = settings.value("Metronome/Volume", 50).toInt();
            metronomeSound->setVolume(volume / 100.0f);
            
            QString soundType = settings.value("Metronome/Sound", "click").toString();
            // TODO: Обновить звук метронома в зависимости от выбранного типа
        }
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
    } else {
        // System theme
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(QApplication::style()->standardPalette());
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
    
    settings.setValue("colorScheme", scheme);
    statusBar()->showMessage(tr("Цветовая схема изменена: %1").arg(scheme == "dark" ? "Тёмная" : "Светлая"), 2000);
}

void MainWindow::createSimpleMetronomeSound()
{
    // Создаем простой звук метронома программно
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    
    // Создаем простой звук щелчка
    const float duration = 0.1f; // 100ms
    const int sampleCount = static_cast<int>(format.sampleRate() * duration);
    QByteArray audioData;
    audioData.resize(sampleCount * format.bytesPerSample());
    
    qint16* samples = reinterpret_cast<qint16*>(audioData.data());
    for (int i = 0; i < sampleCount; ++i) {
        // Простой щелчок - быстрое затухание с синусоидой
        float t = static_cast<float>(i) / sampleCount;
        float amplitude = (1.0f - t) * 0.3f; // Затухание
        float frequency = 800.0f; // Частота щелчка
        float sample = amplitude * std::sin(2.0f * M_PI * frequency * t);
        samples[i] = static_cast<qint16>(sample * 32767.0f);
    }
    
    // Сохраняем во временный файл
    QString tempFile = QDir::tempPath() + "/metronome_temp.wav";
    QFile file(tempFile);
    if (file.open(QIODevice::WriteOnly)) {
        // Простая WAV заголовок
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        // RIFF заголовок
        stream.writeRawData("RIFF", 4);
        stream << static_cast<quint32>(36 + audioData.size());
        stream.writeRawData("WAVE", 4);
        
        // fmt chunk
        stream.writeRawData("fmt ", 4);
        stream << static_cast<quint32>(16);
        stream << static_cast<quint16>(1); // PCM
        stream << static_cast<quint16>(format.channelCount());
        stream << static_cast<quint32>(format.sampleRate());
        stream << static_cast<quint32>(format.sampleRate() * format.channelCount() * format.bytesPerSample());
        stream << static_cast<quint16>(format.channelCount() * format.bytesPerSample());
        stream << static_cast<quint16>(format.bytesPerSample() * 8);
        
        // data chunk
        stream.writeRawData("data", 4);
        stream << static_cast<quint32>(audioData.size());
        stream.writeRawData(audioData.data(), audioData.size());
        
        file.close();
        
        // Обновляем источник звука
        metronomeSound->setSource(QUrl::fromLocalFile(tempFile));
        
        qDebug() << "Created metronome sound file:" << tempFile;
    } else {
        qWarning() << "Failed to create metronome sound file";
    }
}
