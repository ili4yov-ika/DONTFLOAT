#include "../include/timeutils.h"
#include <QtCore/QString>

qint64 TimeUtils::samplesToMs(qint64 samples, int sampleRate)
{
    if (sampleRate <= 0) {
        return 0;
    }
    return (samples * 1000) / sampleRate;
}

double TimeUtils::samplesToSeconds(qint64 samples, int sampleRate)
{
    if (sampleRate <= 0) {
        return 0.0;
    }
    return static_cast<double>(samples) / static_cast<double>(sampleRate);
}

qint64 TimeUtils::msToSamples(qint64 ms, int sampleRate)
{
    if (sampleRate <= 0) {
        return 0;
    }
    return (ms * sampleRate) / 1000;
}

qint64 TimeUtils::secondsToSamples(double seconds, int sampleRate)
{
    if (sampleRate <= 0) {
        return 0;
    }
    return static_cast<qint64>(seconds * static_cast<double>(sampleRate));
}

QString TimeUtils::formatTime(qint64 ms)
{
    return formatTimeWithMs(ms);
}

QString TimeUtils::formatTime(double seconds)
{
    qint64 ms = static_cast<qint64>(seconds * 1000.0);
    return formatTimeWithMs(ms);
}

QString TimeUtils::formatTimeWithMs(qint64 ms)
{
    int totalSeconds = static_cast<int>(ms / 1000);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    int milliseconds = static_cast<int>(ms % 1000);
    
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds / 100); // Показываем только десятые доли секунды
}

QString TimeUtils::formatTimeShort(qint64 ms)
{
    int totalSeconds = static_cast<int>(ms / 1000);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

