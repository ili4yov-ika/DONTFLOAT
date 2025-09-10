#include "../include/mainwindow.h"
#include "../include/beatfixcommand.h"
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

#include <QtGui/QKeyEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtCore/QDataStream>
#include <QtCore/QFile>

#include <QtWidgets/QMessageBox>
#include <QtCore/QDir>
#include <QtCore/QtGlobal>
#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QtMath>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QInputDialog>
#include "../include/bpmanalyzer.h"
#include "../include/bpmfixdialog.h"
#include "../include/metronomesettingsdialog.h"
#include <QUndoStack>
#include <QtGui/QShortcut>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QValidator>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QRegularExpression>
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
    , isPitchGridVisible(true)
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

    createActions();  // Create actions first
    ui->setupUi(this);  // Then setup UI
    
    // Устанавливаем флаги окна для поддержки разворачивания, минимизации и изменения размера
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    
    // Явно разрешаем изменение размера окна
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Убираем все ограничения размера
    setMinimumSize(800, 500);
    setMaximumSize(16777215, 16777215);
    
    createMenus();   // Then create menus

    // Настраиваем валидатор для поля BPM
    // QValidator* bpmValidator = new QRegularExpressionValidator(
    //     QRegularExpression("^\\d{1,4}(\\.\\d{1,2})?$"), this);
    // ui->bpmEdit->setValidator(bpmValidator);

    // Настраиваем валидатор для поля Такты (целое 1..32)
    // QValidator* barsValidator = new QRegularExpressionValidator(
    //     QRegularExpression("^(?:[1-9]|[12]\\d|3[0-2])$"), this);
    // if (ui->barsEdit) ui->barsEdit->setValidator(barsValidator);

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
    
    // Устанавливаем значения по умолчанию в UI
    ui->bpmEdit->setText(QString::number(defaultBPM, 'f', 2));
    ui->barsEdit->setText(QString::number(defaultBeatsPerBar));
    
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
    QString currentScheme = settings.value("colorScheme", "default").toString();
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
            "    background: #404040;"
            "    width: 16px;"
            "    border: none;"
            "}"
            "QScrollBar::handle:vertical {"
            "    background: #606060;"
            "    min-height: 20px;"
            "    border-radius: 0px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "    background: #707070;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
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
    
    // Применяем текущую цветовую схему к виджетам
    currentScheme = settings.value("colorScheme", "default").toString();
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
                "    background: #e0e0e0;"
                "    width: 16px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: #c0c0c0;"
                "    min-height: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: #a0a0a0;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
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
                "    background: #404040;"
                "    width: 16px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: #606060;"
                "    min-height: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: #707070;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
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

void MainWindow::changeEvent(QEvent *event)
{
    // Обрабатываем изменения состояния окна стандартным способом
    QMainWindow::changeEvent(event);
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
    QString colorScheme = settings.value("colorScheme", "default").toString();
    setColorScheme(colorScheme);
    
    // Восстанавливаем видимость питч-сетки
    isPitchGridVisible = settings.value("pitchGridVisible", true).toBool();
    
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
    
    // Обновляем текст действия
    if (togglePitchGridAct) {
        if (isPitchGridVisible) {
            togglePitchGridAct->setText(QString::fromUtf8("Убрать питч-сетку"));
        } else {
            togglePitchGridAct->setText(QString::fromUtf8("Показать питч-сетку"));
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
    
    settings.endGroup();
}

void MainWindow::setupConnections()
{
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::playAudio);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopAudio);
    connect(ui->bpmEdit, &QLineEdit::editingFinished, this, &MainWindow::updateBPM);
    connect(ui->barsEdit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        int bpb = ui->barsEdit->text().toInt(&ok);
        if (ok && waveformView) {
            waveformView->setBeatsPerBar(bpb);
            // Синхронизируем размер такта с PitchGridWidget
            if (pitchGridWidget) {
                pitchGridWidget->setBeatsPerBar(bpb);
            }
            // Принудительно обновляем отображение тактов
            waveformView->update();
            if (pitchGridWidget) {
                pitchGridWidget->update();
            }
            statusBar()->showMessage(QString::fromUtf8("Размер такта: %1").arg(bpb), 2000);
        }
    });
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
        });

    // Обновляем горизонтальный скроллбар при изменении смещения
    connect(waveformView, &WaveformView::horizontalOffsetChanged, this,
        [this](float offset) {
            updateHorizontalScrollBarFromOffset(offset);
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
    metronomeTimer = new QTimer(this);
    metronomeTimer->setInterval(10); // Проверяем каждые 10мс
    metronomeSound = new QMediaPlayer(this);
    
    // Создаем простой звук метронома программно
    createSimpleMetronomeSound();
    
    // Проверяем, загрузился ли звук
    if (metronomeSound->mediaStatus() != QMediaPlayer::LoadedMedia) {
        qWarning() << "Failed to load metronome sound:" << metronomeSound->mediaStatus();
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
                // Просто воспроизводим звук через QMediaPlayer
                if (metronomeSound->mediaStatus() == QMediaPlayer::LoadedMedia) {
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
        // Синхронизируем BPM с PitchGridWidget
        if (pitchGridWidget) {
            pitchGridWidget->setBPM(bpm);
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
            const int bpb = waveformView->getBeatsPerBar();
            int bar = int(beatsFromStart / bpb) + 1;
            int beat = int(beatsFromStart) % bpb + 1;
            float subBeat = (beatsFromStart - float(int(beatsFromStart))) * float(bpb);
            ui->timeLabel->setText(QString("%1.%2.%3")
                .arg(bar)
                .arg(beat)
                .arg(int(subBeat + 1)));
        }
    }
    
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

    // Pitch grid toggle action
    togglePitchGridAct = new QAction(QString::fromUtf8("Убрать питч-сетку"), this);
    togglePitchGridAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    togglePitchGridAct->setStatusTip(QString::fromUtf8("Переключить видимость питч-сетки"));
    connect(togglePitchGridAct, &QAction::triggered, this, &MainWindow::togglePitchGrid);

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
    
    // Add pitch grid toggle to View menu
    viewMenu->addSeparator();
    viewMenu->addAction(togglePitchGridAct);
    
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
        "<p><b>M</b> - Включить/выключить метроном</p>"
        "<p><b>A</b> - Установить точку A (начало цикла)</p>"
        "<p><b>B</b> - Установить точку B (конец цикла)</p>"
        "<p><b>Shift+A</b> - Удалить точку A</p>"
        "<p><b>Shift+B</b> - Удалить точку B</p>"
        "<p><b>Ctrl+Z</b> - Отменить</p>"
        "<p><b>Ctrl+Y</b> - Повторить</p>"
        "<p><b>Ctrl+O</b> - Открыть файл</p>"
        "<p><b>Ctrl+S</b> - Сохранить файл</p>"
        "<p><b>Ctrl+G</b> - Переключить питч-сетку</p>"
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
    if (isMetronomeEnabled) {
        toggleMetronome();
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
        options.useMixxxAlgorithm = true; // Используем алгоритм Mixxx для лучшей точности
        
        // Анализируем BPM с отображением прогресса
        dialog.updateProgress(QString::fromUtf8("Анализ аудио..."), 50);
        BPMAnalyzer::AnalysisResult analysis = BPMAnalyzer::analyzeBPM(audioData[0], waveformView->getSampleRate(), options);
        
        dialog.updateProgress(QString::fromUtf8("Анализ завершен"), 100);
        dialog.showResult(analysis);
        
        // Сохраняем оригинальные данные
        QVector<QVector<float>> originalData = audioData;
        
        if (dialog.exec() == QDialog::Accepted && dialog.shouldFixBeats()) {
            dialog.updateProgress(QString::fromUtf8("Выравнивание долей..."), 50);
            
            // Используем обнаруженный BPM для выравнивания
            qDebug() << "Using detected BPM:" << analysis.bpm;
            
            // Создаем копию для исправленных данных
            QVector<QVector<float>> fixedData = audioData;
            
            // Если пользователь хочет пометить неровные доли, выполняем выравнивание
            if (dialog.markIrregularBeats()) {
                qDebug() << "Marking irregular beats with BPM:" << analysis.bpm;
                for (int i = 0; i < fixedData.size(); ++i) {
                    fixedData[i] = BPMAnalyzer::fixBeats(fixedData[i], analysis);
                }
                
                // Создаем и выполняем команду отмены
                BeatFixCommand* command = new BeatFixCommand(waveformView, originalData, fixedData, analysis.bpm, analysis.beats);
                undoStack->push(command);
                
                dialog.updateProgress(QString::fromUtf8("Выравнивание завершено"), 100);
                statusBar()->showMessage(QString::fromUtf8("Доли выровнены по BPM: %1").arg(analysis.bpm, 0, 'f', 1), 2000);
                hasUnsavedChanges = true;
            } else {
                // Если пользователь не хочет помечать неровные доли, просто обновляем отображение
                qDebug() << "Skipping beat alignment, showing original data";
                waveformView->setAudioData(audioData);
                waveformView->setBeatInfo(analysis.beats);
                ui->bpmEdit->setText(QString::number(analysis.bpm, 'f', 2));
                waveformView->setBPM(analysis.bpm);
                
                // Обновляем PitchGridWidget
                if (pitchGridWidget) {
                    pitchGridWidget->setAudioData(audioData);
                    pitchGridWidget->setSampleRate(waveformView->getSampleRate());
                    pitchGridWidget->setBPM(analysis.bpm);
                    pitchGridWidget->update();
                }
                
                dialog.updateProgress(QString::fromUtf8("Отображение без выравнивания"), 100);
                statusBar()->showMessage(QString::fromUtf8("Трек загружен без выравнивания долей"), 2000);
            }
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
                // Принудительно обновляем отображение тактов
                pitchGridWidget->update();
            }
            
            // Принудительно обновляем отображение тактов в WaveformView
            waveformView->update();
            
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
    MetronomeSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Применяем новые настройки метронома
        if (metronomeSound) {
            // В Qt6 громкость устанавливается через QAudioOutput
            // int volume = settings.value("Metronome/Volume", 50).toInt();
            // metronomeSound->setVolume(volume / 100.0f);
            
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
                "    background: #404040;"
                "    width: 16px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: #606060;"
                "    min-height: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: #707070;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
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
                "    background: #e0e0e0;"
                "    width: 16px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: #c0c0c0;"
                "    min-height: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: #a0a0a0;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
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
                "    background: #404040;"
                "    width: 16px;"
                "    border: none;"
                "}"
                "QScrollBar::handle:vertical {"
                "    background: #606060;"
                "    min-height: 20px;"
                "    border-radius: 0px;"
                "}"
                "QScrollBar::handle:vertical:hover {"
                "    background: #707070;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                "    background: none;"
                "    border: none;"
                "}"
            );
        }
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
        
        // Ждем загрузки звука
        QEventLoop loop;
        connect(metronomeSound, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::InvalidMedia) {
                loop.quit();
            }
        });
        loop.exec();
        
        qDebug() << "Created metronome sound file:" << tempFile;
    } else {
        qWarning() << "Failed to create metronome sound file";
    }
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
    
    // Обновляем текст действия
    if (isPitchGridVisible) {
        togglePitchGridAct->setText(QString::fromUtf8("Убрать питч-сетку"));
    } else {
        togglePitchGridAct->setText(QString::fromUtf8("Показать питч-сетку"));
    }
    
    // Сохраняем настройку
    settings.setValue("pitchGridVisible", isPitchGridVisible);
}
