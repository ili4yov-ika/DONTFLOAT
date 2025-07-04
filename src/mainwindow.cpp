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
#include <QtGui/QKeyEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtCore/QDataStream>
#include <QtCore/QFile>

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
    
    // Создаем и настраиваем скроллбар
    horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar->setMinimum(0);
    horizontalScrollBar->setMaximum(1000); // Будем масштабировать значения
    if (!ui->scrollBarWidget->layout()) {
        ui->scrollBarWidget->setLayout(new QVBoxLayout());
    }
    ui->scrollBarWidget->layout()->addWidget(horizontalScrollBar);
    
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
                // Преобразуем значение скроллбара в смещение
                float offset = float(value) / float(horizontalScrollBar->maximum());
                waveformView->setHorizontalOffset(offset);
            }
        });

    // Обновляем скроллбар при изменении масштаба
    connect(waveformView, &WaveformView::zoomChanged, this,
        [this](float zoom) {
            if (zoom <= 1.0f) {
                // При масштабе 100% или меньше показываем весь трек
                horizontalScrollBar->setMaximum(0);
                horizontalScrollBar->setPageStep(1000);
                horizontalScrollBar->setValue(0);
            } else {
                // При увеличении масштаба регулируем размер и положение ползунка
                int total = 10000; // Используем фиксированное большое значение для точности
                int page = int(total / zoom);
                horizontalScrollBar->setMaximum(total - page);
                horizontalScrollBar->setPageStep(page);
                
                // Сохраняем текущую видимую позицию
                float currentOffset = waveformView->getHorizontalOffset();
                int value = int(currentOffset * total);
                horizontalScrollBar->setValue(value);
            }
        });

    // Подключаем сигнал изменения позиции от WaveformView
    connect(waveformView, &WaveformView::positionChanged, this,
        [this](qint64 samplePosition) {
            if (waveformView) {
                qint64 msPosition = (samplePosition * 1000) / waveformView->getSampleRate();
                mediaPlayer->setPosition(msPosition);
                updateTimeLabel(msPosition);
            }
        });

    // Связываем скроллбар с горизонтальной прокруткой
    connect(horizontalScrollBar, &QScrollBar::valueChanged, this,
        [this](int value) {
            if (waveformView) {
                float offset = float(value) / float(horizontalScrollBar->maximum() + horizontalScrollBar->pageStep());
                waveformView->setHorizontalOffset(offset);
            }
        });

    // Добавляем кнопки загрузки и сохранения в тулбар
    ui->loadButton->setIcon(QIcon(":/icons/resources/icons/load.svg"));
    ui->saveButton->setIcon(QIcon(":/icons/resources/icons/save.svg"));
    connect(ui->loadButton, &QPushButton::clicked, this, &MainWindow::openAudioFile);
    connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::saveAudioFile);
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
    if (ok && bpm > 0.0f && bpm <= 999.99f) {
        waveformView->setBPM(bpm);
        statusBar()->showMessage(tr("BPM установлен: %1").arg(bpm), 2000);
    } else {
        ui->bpmEdit->setText("120.00");
        statusBar()->showMessage(tr("Неверное значение BPM"), 2000);
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
        qint64 samplePosition = (position * waveformView->getSampleRate()) / 1000;
        waveformView->setPlaybackPosition(samplePosition);
        updateTimeLabel(position);
    }
}

void MainWindow::createActions()
{
    openAct = new QAction(tr("&Открыть..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Открыть аудиофайл"));
    openAct->setIcon(QIcon(":/icons/resources/icons/load.svg"));
    connect(openAct, &QAction::triggered, this, &MainWindow::openAudioFile);

    saveAct = new QAction(tr("&Сохранить как..."), this);
    saveAct->setShortcuts(QKeySequence::SaveAs);
    saveAct->setStatusTip(tr("Сохранить аудиофайл"));
    saveAct->setIcon(QIcon(":/icons/resources/icons/save.svg"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveAudioFile);

    exitAct = new QAction(tr("&Выход"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Выйти из приложения"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
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
                statusBar()->showMessage(tr("Файл сохранен: %1").arg(fileName), 2000);
            } else {
                statusBar()->showMessage(tr("Ошибка: нет данных для сохранения"), 2000);
            }
        } else {
            statusBar()->showMessage(tr("Ошибка: не удалось открыть файл для записи"), 2000);
        }
    }
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
    } else {
        QMainWindow::keyPressEvent(event);
    }
}
