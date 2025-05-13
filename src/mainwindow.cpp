#include "mainwindow.h"
#include "./ui_mainwindow.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isPlaying(false)
    , currentPosition(0)
    , settings("DONTFLOAT", "DONTFLOAT")
{
    ui->setupUi(this);
    
    // Создаем и настраиваем WaveformView
    waveformView = new WaveformView(this);
    if (!ui->waveformWidget->layout()) {
        ui->waveformWidget->setLayout(new QVBoxLayout());
    }
    ui->waveformWidget->layout()->addWidget(waveformView);
    
    // Инициализация аудио
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    
    createActions();
    createMenus();
    setupConnections();

    // Установка иконки приложения
    QIcon appIcon(":/icons/resources/icons/logo.svg");
    setWindowIcon(appIcon);
    QGuiApplication::setWindowIcon(appIcon);

    // Восстановление состояния окна
    readSettings();
    
    // Инициализация таймера для обновления времени
    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    playbackTimer->setInterval(33); // ~30 fps

    // Начальное название окна
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
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
        currentPosition += playbackTimer->interval();
        int minutes = (currentPosition / 1000) / 60;
        int seconds = (currentPosition / 1000) % 60;
        int milliseconds = currentPosition % 1000;
        ui->timeLabel->setText(QString("%1:%2:%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(milliseconds, 3, QChar('0')));
    }
}

void MainWindow::updateBPM()
{
    bool ok;
    float bpm = ui->bpmEdit->text().toFloat(&ok);
    if (ok && bpm > 0.0f && bpm <= 999.99f) {
        waveformView->setBPM(bpm);
        statusBar()->showMessage(tr("BPM установлен: %1").arg(bpm), 2000);
    } else {
        ui->bpmEdit->setText("120.00");
        statusBar()->showMessage(tr("Неверное значение BPM"), 2000);
    }
}

void MainWindow::createActions()
{
    openAct = new QAction(tr("&Открыть..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Открыть аудиофайл"));
    connect(openAct, &QAction::triggered, this, &MainWindow::openAudioFile);

    exitAct = new QAction(tr("&Выход"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Выйти из приложения"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);
}

void MainWindow::openAudioFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Открыть аудиофайл"), "",
        tr("Аудиофайлы (*.wav *.mp3 *.flac);;Все файлы (*)"));

    if (!fileName.isEmpty()) {
        currentFileName = fileName;
        updateWindowTitle();
        processAudioFile(fileName);
        mediaPlayer->setSource(QUrl::fromLocalFile(fileName));
        statusBar()->showMessage(tr("Файл загружен: %1").arg(fileName), 2000);
    }
}

void MainWindow::processAudioFile(const QString& filePath)
{
    QVector<QVector<float>> audioData = loadAudioFile(filePath);
    if (!audioData.isEmpty()) {
        waveformView->setAudioData(audioData);
        waveformView->setBPM(ui->bpmEdit->text().toFloat());
    }
}

QVector<QVector<float>> MainWindow::loadAudioFile(const QString& filePath)
{
    QVector<QVector<float>> channels;
    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(filePath));

    QEventLoop loop;
    QVector<float> leftChannel;
    QVector<float> rightChannel;

    connect(&decoder, &QAudioDecoder::bufferReady, this,
        [&]() {
            QAudioBuffer buffer = decoder.read();
            const float* data = buffer.constData<float>();
            int frameCount = buffer.frameCount();
            int channelCount = buffer.format().channelCount();

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

    channels.append(leftChannel);
    if (!rightChannel.isEmpty()) {
        channels.append(rightChannel);
    }

    return channels;
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
