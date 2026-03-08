#include "../include/timeutils.h"
#include <QtCore/QString>
#include <QtCore/QtGlobal>

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

QString TimeUtils::formatTimeAndBars(qint64 msPosition, float bpm, int beatsPerBar,
                                    int sampleRate, qint64 gridStartSample)
{
    QString timeStr = formatTimeWithMs(msPosition);
    if (bpm <= 0.0f) {
        return timeStr + " | --.--.--";
    }
    qint64 samplePos = (msPosition * qint64(sampleRate)) / 1000;
    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    float barLengthInQuarters = (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    int subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
    float samplesPerSubdivision = samplesPerBar / float(subdivisionsPerBar);
    float subsFromStart;
    if (gridStartSample > 0) {
        subsFromStart = float(samplePos - gridStartSample) / samplesPerSubdivision;
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

