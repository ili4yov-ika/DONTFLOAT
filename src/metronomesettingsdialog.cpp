#include "../include/metronomesettingsdialog.h"
#include "../include/metronomecontroller.h"
#include "ui_metronomesettingsdialog.h"
#include <QFileDialog>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <cmath>

MetronomeSettingsDialog::MetronomeSettingsDialog(QWidget *parent, MetronomeController *controller)
    : QDialog(parent)
    , ui(new Ui::MetronomeSettingsDialog)
    , settings("DONTFLOAT", "DONTFLOAT")
    , metronomeController(controller)
{
    ui->setupUi(this);
    
    // Инициализируем звуковые эффекты
    metronomeSoundEffect = new QSoundEffect(this);
    metronomeSoundEffect->setVolume(1.0f);
    
    metronomePlayer = new QMediaPlayer(this);
    metronomePlayer->setAudioOutput(new QAudioOutput(this));
    
    // Создаем звук метронома программно
    createMetronomeSound();
    
    // Подключаем сигналы кнопок
    connect(ui->testButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onTestButtonClicked);
    connect(ui->testWeakButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onTestWeakButtonClicked);
    connect(ui->selectSoundButton, &QPushButton::clicked, this, &MetronomeSettingsDialog::onSelectSoundButtonClicked);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &MetronomeSettingsDialog::saveSettings);
    
    // Подключаем сигналы ползунков
    connect(ui->strongBeatSlider, &QSlider::valueChanged, this, &MetronomeSettingsDialog::onStrongBeatVolumeChanged);
    connect(ui->weakBeatSlider, &QSlider::valueChanged, this, &MetronomeSettingsDialog::onWeakBeatVolumeChanged);

    // Загружаем настройки: сначала из контроллера (если есть), иначе из QSettings
    loadSettings();
    
    // Обновляем статус загрузки звука
    updateSoundStatus();
}

MetronomeSettingsDialog::~MetronomeSettingsDialog()
{
    delete ui;
}

void MetronomeSettingsDialog::saveSettings()
{
    int strongVolume = ui->strongBeatSlider->value();
    int weakVolume = ui->weakBeatSlider->value();
    
    // Сохраняем в QSettings
    settings.setValue("Metronome/StrongBeatVolume", strongVolume);
    settings.setValue("Metronome/WeakBeatVolume", weakVolume);
    settings.setValue("Metronome/Sound", ui->soundComboBox->currentData());
    
    // Применяем к контроллеру напрямую (если есть)
    if (metronomeController) {
        metronomeController->setStrongBeatVolume(strongVolume);
        metronomeController->setWeakBeatVolume(weakVolume);
    }
}

void MetronomeSettingsDialog::loadSettings()
{
    // Если есть контроллер, используем его текущие значения (более актуальные)
    if (metronomeController) {
        ui->strongBeatSlider->setValue(metronomeController->getStrongBeatVolume());
        ui->weakBeatSlider->setValue(metronomeController->getWeakBeatVolume());
    } else {
        // Иначе загружаем из QSettings
        ui->strongBeatSlider->setValue(settings.value("Metronome/StrongBeatVolume", 100).toInt());
        ui->weakBeatSlider->setValue(settings.value("Metronome/WeakBeatVolume", 90).toInt());
    }
    
    QString sound = settings.value("Metronome/Sound", "click").toString();
    int index = ui->soundComboBox->findData(sound);
    if (index != -1)
    {
        ui->soundComboBox->setCurrentIndex(index);
    }
}

void MetronomeSettingsDialog::createMetronomeSound()
{
    // Создаем простой звук метронома (короткий "клик" - синусоида 1000 Гц)
    const int sampleRate = 44100;
    const double duration = 0.05; // 50 мс
    const int frequency = 1000; // 1000 Гц
    const int numSamples = static_cast<int>(sampleRate * duration);
    
    QByteArray wavData;
    QDataStream stream(&wavData, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // WAV заголовок
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + numSamples * 2); // Размер файла - 8
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16); // Размер fmt chunk
    stream << quint16(1); // Audio format (1 = PCM)
    stream << quint16(1); // Количество каналов (моно)
    stream << quint32(sampleRate); // Sample rate
    stream << quint32(sampleRate * 2); // Byte rate
    stream << quint16(2); // Block align
    stream << quint16(16); // Bits per sample
    stream.writeRawData("data", 4);
    stream << quint32(numSamples * 2); // Размер данных
    
    // Генерируем синусоиду с затуханием
    for (int i = 0; i < numSamples; ++i) {
        double t = double(i) / sampleRate;
        double amplitude = 0.5 * exp(-t * 20.0); // Экспоненциальное затухание
        qint16 sample = qint16(amplitude * 32767.0 * sin(2.0 * M_PI * frequency * t));
        stream << sample;
    }
    
    // Сохраняем во временный файл
    QString tempFile = QDir::tempPath() + "/metronome_test.wav";
    QFile file(tempFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(wavData);
        file.close();
        metronomeSoundFile = tempFile;
        metronomeSoundEffect->setSource(QUrl::fromLocalFile(tempFile));
        metronomePlayer->setSource(QUrl::fromLocalFile(tempFile));
    } else {
        qWarning() << "Failed to create metronome test sound file";
    }
}

void MetronomeSettingsDialog::onTestButtonClicked()
{
    int strongVolume = ui->strongBeatSlider->value();
    playTestSound(strongVolume, true);
}

void MetronomeSettingsDialog::onTestWeakButtonClicked()
{
    int weakVolume = ui->weakBeatSlider->value();
    playTestSound(weakVolume, false);
}

void MetronomeSettingsDialog::playTestSound(int volume, bool isStrong)
{
    Q_UNUSED(isStrong); // Параметр пока не используется
    if (metronomeSoundFile.isEmpty()) {
        qWarning() << "Metronome sound file not created";
        return;
    }
    
    // Пробуем воспроизвести через QSoundEffect (быстрее для коротких звуков)
    if (metronomeSoundEffect && metronomeSoundEffect->status() == QSoundEffect::Ready) {
        metronomeSoundEffect->setVolume(volume / 100.0f);
        metronomeSoundEffect->play();
    } else if (metronomePlayer && metronomePlayer->mediaStatus() == QMediaPlayer::LoadedMedia) {
        // Fallback на QMediaPlayer
        metronomePlayer->audioOutput()->setVolume(volume / 100.0f);
        metronomePlayer->setPosition(0);
        metronomePlayer->play();
    } else {
        // Если звук еще не загружен, пытаемся загрузить
        metronomeSoundEffect->setSource(QUrl::fromLocalFile(metronomeSoundFile));
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(500); // 500мс таймаут
        
        connect(metronomeSoundEffect, &QSoundEffect::statusChanged, &loop, [&]() {
            if (metronomeSoundEffect->status() == QSoundEffect::Ready || 
                metronomeSoundEffect->status() == QSoundEffect::Error) {
                loop.quit();
            }
        });
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timeoutTimer.start();
        loop.exec();
        
        if (metronomeSoundEffect->status() == QSoundEffect::Ready) {
            metronomeSoundEffect->setVolume(volume / 100.0f);
            metronomeSoundEffect->play();
        } else {
            qWarning() << "Failed to load metronome sound for testing";
        }
    }
}

void MetronomeSettingsDialog::updateSoundStatus()
{
    QString status;
    if (metronomeSoundEffect && metronomeSoundEffect->status() == QSoundEffect::Ready) {
        status = "✓ Звук готов";
    } else if (metronomePlayer && metronomePlayer->mediaStatus() == QMediaPlayer::LoadedMedia) {
        status = "✓ Звук готов (QMediaPlayer)";
    } else {
        status = "⚠ Звук не загружен";
    }
    
    if (ui->statusLabel) {
        ui->statusLabel->setText(status);
    }
}

void MetronomeSettingsDialog::onStrongBeatVolumeChanged(int value)
{
    ui->strongBeatValueLabel->setText(QString::number(value) + "%");
}

void MetronomeSettingsDialog::onWeakBeatVolumeChanged(int value)
{
    ui->weakBeatValueLabel->setText(QString::number(value) + "%");
}

void MetronomeSettingsDialog::onSelectSoundButtonClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Выберите звук метронома"), "", tr("Аудиофайлы (*.wav *.mp3)"));
    if (!fileName.isEmpty())
    {
        ui->soundComboBox->addItem(tr("Пользовательский"), fileName);
        ui->soundComboBox->setCurrentIndex(ui->soundComboBox->count() - 1);
    }
}