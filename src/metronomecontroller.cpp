#include "../include/metronomecontroller.h"
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QtCore/qmath.h>

#ifdef _MSC_VER
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#endif
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
    m_timer->setInterval(5);  // Проверяем каждые 5 мс для точности
    m_timer->setTimerType(Qt::PreciseTimer);  // Минимизировать дрейф таймера

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

    // Очищаем временный файл звука
    if (!m_soundFile.isEmpty() && QFile::exists(m_soundFile)) {
        QFile::remove(m_soundFile);
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
        m_timer->start(5);
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

// Примерная задержка вывода звука (мс): буфер звуковой карты + QSoundEffect
constexpr qint64 kAudioLatencyCompensationMs = 25;

void MetronomeController::onTimerTimeout()
{
    if (!m_enabled || !m_playing) {
        return;
    }

    if (m_bpm <= 0) {
        return;
    }

    qint64 beatInterval = qint64(60000.0f / m_bpm);

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if (m_lastBeatTime == 0) {
        m_lastBeatTime = currentTime;
    }

    // Следующий удар по сетке — с компенсацией задержки вывода, чтобы звук шёл «в такт»
    qint64 nextBeatTime = m_lastBeatTime + beatInterval;
    qint64 triggerTime = nextBeatTime - kAudioLatencyCompensationMs;

    if (currentTime >= triggerTime) {
        bool isStrongBeat = (m_currentBeatNumber % 4 == 0);

        playBeat(isStrongBeat);
        emit beatPlayed(isStrongBeat);

        // Держим сетку: следующий удар ровно через beatInterval от предыдущего по сетке
        m_lastBeatTime = nextBeatTime;
        m_currentBeatNumber++;

        // Если отстали от сетки (пауза и т.п.), подтягиваем следующую долю к будущему
        while (m_lastBeatTime + beatInterval < currentTime) {
            m_lastBeatTime += beatInterval;
            m_currentBeatNumber++;
        }
    }
}

void MetronomeController::playBeat(bool isStrongBeat)
{
    int volume = isStrongBeat ? m_strongBeatVolume : m_weakBeatVolume;

    // Пробуем воспроизвести звук через QSoundEffect (более надежно для коротких звуков)
    if (m_soundEffect && m_soundEffect->status() == QSoundEffect::Ready) {
        m_soundEffect->setVolume(volume / 100.0f);
        m_soundEffect->play();
    } else if (m_soundPlayer && m_soundPlayer->mediaStatus() == QMediaPlayer::LoadedMedia) {
        // Fallback на QMediaPlayer
        m_soundPlayer->audioOutput()->setVolume(volume / 100.0f);
        m_soundPlayer->setPosition(0); // Сбрасываем позицию
        m_soundPlayer->play();
    }
    // Если звук недоступен, просто пропускаем без вывода
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
            // Успешно загружен - также настраиваем QSoundEffect
            m_soundEffect->setSource(QUrl::fromLocalFile(tempFile));
            m_soundFile = tempFile;
        } else {
            // Fallback - попробуем использовать системный звук
            m_soundPlayer->setSource(QUrl("qrc:/sounds/metronome.wav"));
            m_soundEffect->setSource(QUrl("qrc:/sounds/metronome.wav"));
        }
    } else {
        // Fallback на ресурс
        m_soundPlayer->setSource(QUrl("qrc:/sounds/metronome.wav"));
        m_soundEffect->setSource(QUrl("qrc:/sounds/metronome.wav"));
    }
}

