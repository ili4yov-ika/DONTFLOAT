#include "../include/metronomecontroller.h"
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QtCore/qmath.h>
#include <cmath>

MetronomeController::MetronomeController(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_soundPlayer(new QMediaPlayer(this))
    , m_soundEffect(new QSoundEffect(this))
    , m_enabled(false)
    , m_playing(false)
    , m_bpm(120.0f)
    , m_strongBeatVolume(100)
    , m_weakBeatVolume(90)
    , m_lastBeatTime(0)
    , m_currentBeatNumber(0)
{
    m_timer->setInterval(10); // Проверяем каждые 10мс для точности
    
    m_soundPlayer->setAudioOutput(new QAudioOutput(this));
    m_soundPlayer->audioOutput()->setVolume(0.5f);
    m_soundEffect->setVolume(0.5f);
    
    // Создаем звук метронома
    createMetronomeSound();
    
    // Подключаем сигнал таймера
    connect(m_timer, &QTimer::timeout, this, &MetronomeController::onTimerTimeout);
    
    // Загружаем настройки
    QSettings settings("DONTFLOAT", "DONTFLOAT");
    m_strongBeatVolume = settings.value("Metronome/StrongBeatVolume", 100).toInt();
    m_weakBeatVolume = settings.value("Metronome/WeakBeatVolume", 90).toInt();
}

MetronomeController::~MetronomeController()
{
    if (m_timer) {
        m_timer->stop();
    }
}

void MetronomeController::setBPM(float bpm)
{
    m_bpm = bpm;
    // Таймер уже настроен на 10мс, интервал между ударами вычисляется в onTimerTimeout
}

void MetronomeController::setEnabled(bool enabled)
{
    m_enabled = enabled;
    
    if (m_enabled) {
        m_timer->start(10);
        m_currentBeatNumber = 0;
        m_lastBeatTime = QDateTime::currentMSecsSinceEpoch();
    } else {
        m_timer->stop();
    }
}

void MetronomeController::setPlaying(bool playing)
{
    m_playing = playing;
    if (playing && m_enabled) {
        // При начале воспроизведения сбрасываем счетчик и время
        m_currentBeatNumber = 0;
        m_lastBeatTime = QDateTime::currentMSecsSinceEpoch();
    } else if (!playing) {
        m_currentBeatNumber = 0;
    }
}

void MetronomeController::setStrongBeatVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 150) volume = 150;
    m_strongBeatVolume = volume;
}

void MetronomeController::setWeakBeatVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 150) volume = 150;
    m_weakBeatVolume = volume;
}

void MetronomeController::reset()
{
    m_currentBeatNumber = 0;
    m_lastBeatTime = QDateTime::currentMSecsSinceEpoch();
}

void MetronomeController::onTimerTimeout()
{
    // Метроном работает только когда включен И играет аудио
    if (!m_enabled || !m_playing) {
        return;
    }
    
    if (m_bpm <= 0) {
        qDebug() << "Metronome: Invalid BPM:" << m_bpm;
        return; // Защита от некорректного BPM
    }
    
    qint64 beatInterval = qint64(60000.0f / m_bpm); // интервал между ударами в мс
    
    // Используем системное время для синхронизации
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // Если это первый запуск, инициализируем время
    if (m_lastBeatTime == 0) {
        m_lastBeatTime = currentTime;
    }
    
    if (currentTime - m_lastBeatTime >= beatInterval) {
        // Определяем, какая это доля (большая или малая)
        bool isStrongBeat = (m_currentBeatNumber % 4 == 0); // Каждая 4-я доля - большая
        
        playBeat(isStrongBeat);
        emit beatPlayed(isStrongBeat);
        
        m_lastBeatTime = currentTime;
        m_currentBeatNumber++;
    }
}

void MetronomeController::playBeat(bool isStrongBeat)
{
    int volume = isStrongBeat ? m_strongBeatVolume : m_weakBeatVolume;
    
    // Пробуем воспроизвести звук через QSoundEffect (более надежно для коротких звуков)
    if (m_soundEffect && m_soundEffect->status() == QSoundEffect::Ready) {
        m_soundEffect->setVolume(volume / 100.0f);
        m_soundEffect->play();
        qDebug() << "Metronome beat" << (isStrongBeat ? "(strong)" : "(weak)") << "volume:" << volume << "% (QSoundEffect)";
    } else if (m_soundPlayer && m_soundPlayer->mediaStatus() == QMediaPlayer::LoadedMedia) {
        // Fallback на QMediaPlayer
        m_soundPlayer->audioOutput()->setVolume(volume / 100.0f);
        m_soundPlayer->setPosition(0); // Сбрасываем позицию
        m_soundPlayer->play();
        qDebug() << "Metronome beat" << (isStrongBeat ? "(strong)" : "(weak)") << "volume:" << volume << "% (QMediaPlayer)";
    } else {
        // Fallback - используем системный звук или просто визуальную индикацию
        qDebug() << "Metronome beat" << (isStrongBeat ? "(strong)" : "(weak)") << "volume:" << volume << "% (no sound)";
    }
}

void MetronomeController::createMetronomeSound()
{
    // Упрощенный подход - используем системный звук или создаем простой звук
    // Попробуем использовать ресурс из qrc, если он есть
    QString soundPath = "qrc:/sounds/metronome.wav";
    
    // Если ресурс не найден, создаем простой звук программно
    QByteArray audioData;
    const int sampleRate = 44100;
    const float duration = 0.1f; // 100ms
    const int sampleCount = static_cast<int>(sampleRate * duration);
    
    audioData.resize(sampleCount * 2); // 16-bit samples
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
        stream << static_cast<quint16>(1); // моно
        stream << static_cast<quint32>(sampleRate);
        stream << static_cast<quint32>(sampleRate * 2); // байт в секунду
        stream << static_cast<quint16>(2); // байт на сэмпл
        stream << static_cast<quint16>(16); // бит на сэмпл
        
        // data chunk
        stream.writeRawData("data", 4);
        stream << static_cast<quint32>(audioData.size());
        stream.writeRawData(audioData.data(), audioData.size());
        
        file.close();
        
        // Обновляем источник звука
        m_soundPlayer->setSource(QUrl::fromLocalFile(tempFile));
        
        // Ждем загрузки звука с таймаутом
        QTimer loadTimer;
        loadTimer.setSingleShot(true);
        loadTimer.setInterval(1000); // 1 секунда таймаут
        
        QEventLoop loop;
        connect(m_soundPlayer, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::InvalidMedia) {
                loop.quit();
            }
        });
        connect(&loadTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        loadTimer.start();
        loop.exec();
        
        if (m_soundPlayer->mediaStatus() == QMediaPlayer::LoadedMedia) {
            qDebug() << "Metronome sound loaded successfully:" << tempFile;
            // Также настраиваем QSoundEffect
            m_soundEffect->setSource(QUrl::fromLocalFile(tempFile));
            m_soundFile = tempFile;
        } else {
            qWarning() << "Failed to load metronome sound, trying fallback";
            // Fallback - попробуем использовать системный звук
            m_soundPlayer->setSource(QUrl("qrc:/sounds/metronome.wav"));
            m_soundEffect->setSource(QUrl("qrc:/sounds/metronome.wav"));
        }
    } else {
        qWarning() << "Failed to create metronome sound file";
        // Fallback на ресурс
        m_soundPlayer->setSource(QUrl("qrc:/sounds/metronome.wav"));
        m_soundEffect->setSource(QUrl("qrc:/sounds/metronome.wav"));
    }
}

