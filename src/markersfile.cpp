#include "../include/markersfile.h"
#include "../include/timeutils.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>
#include <QtCore/QStringConverter>
#include <algorithm>
#include <cmath>

QString MarkersFile::markersPathForAudio(const QString& audioPath)
{
    const QFileInfo info(audioPath);
    return info.absolutePath() + QLatin1Char('/')
           + info.completeBaseName() + QStringLiteral("_markers.txt");
}

QString MarkersFile::formatTimeMssCc(qint64 samplePosition, int sampleRate)
{
    if (sampleRate <= 0) {
        return QStringLiteral("0:00:00");
    }
    const qint64 ms = TimeUtils::samplesToMs(samplePosition, sampleRate);
    const int totalCs = static_cast<int>(ms / 10);
    const int cs = totalCs % 100;
    const int totalSec = totalCs / 100;
    const int sec = totalSec % 60;
    const int min = totalSec / 60;
    return QStringLiteral("%1:%2:%3")
        .arg(min)
        .arg(sec, 2, 10, QChar('0'))
        .arg(cs, 2, 10, QChar('0'));
}

bool MarkersFile::sampleToBarBeat(qint64 samplePosition, float bpm, int beatsPerBar,
                                  int sampleRate, qint64 gridStartSample,
                                  int* outBar, int* outBeat)
{
    if (!outBar || !outBeat || bpm <= 0.f || sampleRate <= 0 || beatsPerBar <= 0) {
        return false;
    }

    const float samplesPerBeat = (60.0f * sampleRate) / bpm;
    const float barLengthInQuarters =
        (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    const float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    const int subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : beatsPerBar;

    float subsFromStart;
    if (gridStartSample > 0) {
        subsFromStart = float(samplePosition - gridStartSample) / samplesPerBeat;
        if (subsFromStart < 0.f) {
            subsFromStart = 0.f;
        }
    } else {
        subsFromStart = float(samplePosition) / samplesPerBeat;
    }

    const int subIndex = qMax(0, int(std::floor(subsFromStart + 1e-6f)));
    *outBar = subIndex / subdivisionsPerBar + 1;
    *outBeat = (subIndex % subdivisionsPerBar) + 1;
    return true;
}

bool MarkersFile::writeFile(const QString& path,
                            const MarkersFileMeta& meta,
                            const QVector<Marker>& markers,
                            int sampleRate,
                            QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Не удалось открыть файл для записи: %1").arg(path);
        }
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "# BPM=" << meta.bpm
        << " BeatsPerBar=" << meta.beatsPerBar
        << " SampleRate=" << meta.sampleRate
        << " GridStartSample=" << meta.gridStartSample << "\n";

    QVector<Marker> sorted = markers;
    std::sort(sorted.begin(), sorted.end(),
              [](const Marker& a, const Marker& b) { return a.position < b.position; });

    for (const Marker& m : sorted) {
        if (m.isEndMarker) {
            continue;
        }
        int bar = 1;
        int beat = 1;
        sampleToBarBeat(m.position, meta.bpm, meta.beatsPerBar, sampleRate,
                        meta.gridStartSample, &bar, &beat);
        out << bar << '.' << beat << " - "
            << formatTimeMssCc(m.position, sampleRate) << "\r\n";
    }

    return true;
}

bool MarkersFile::readFile(const QString& path,
                           MarkersFileMeta* meta,
                           QVector<MarkersFileEntry>* entries,
                           QString* error)
{
    if (!meta || !entries) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Не удалось открыть файл: %1").arg(path);
        }
        return false;
    }

    entries->clear();

    static const QRegularExpression headerRe(
        QStringLiteral(R"(^#\s*BPM=([\d.]+)\s+BeatsPerBar=(\d+)\s+SampleRate=(\d+)\s+GridStartSample=(\d+)\s*$)"));
    static const QRegularExpression lineRe(
        QStringLiteral(R"(^\s*(\d+)\.(\d+)\s*-\s*(\d+):(\d{2}):(\d{2})\s*$)"));

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    bool headerFound = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            const QRegularExpressionMatch hm = headerRe.match(line);
            if (hm.hasMatch()) {
                meta->bpm = hm.captured(1).toFloat();
                meta->beatsPerBar = hm.captured(2).toInt();
                meta->sampleRate = hm.captured(3).toInt();
                meta->gridStartSample = hm.captured(4).toLongLong();
                headerFound = true;
            }
            continue;
        }

        const QRegularExpressionMatch m = lineRe.match(line);
        if (!m.hasMatch()) {
            continue;
        }

        MarkersFileEntry entry;
        entry.bar = m.captured(1).toInt();
        entry.beat = m.captured(2).toInt();
        const int min = m.captured(3).toInt();
        const int sec = m.captured(4).toInt();
        const int cs = m.captured(5).toInt();
        const qint64 totalCs = static_cast<qint64>(min) * 6000 + sec * 100 + cs;
        const qint64 ms = totalCs * 10;
        entry.samplePosition = TimeUtils::msToSamples(ms, meta->sampleRate > 0 ? meta->sampleRate : 44100);
        entries->append(entry);
    }

    if (entries->isEmpty()) {
        if (error) {
            *error = QStringLiteral("В файле меток нет данных: %1").arg(path);
        }
        return false;
    }

    Q_UNUSED(headerFound);
    return true;
}

QVector<Marker> MarkersFile::markersFromEntries(const QVector<MarkersFileEntry>& entries,
                                                  int sampleRate)
{
    QVector<Marker> markers;
    markers.reserve(entries.size() + 1);
    markers.append(Marker(0, true, sampleRate));

    for (const MarkersFileEntry& e : entries) {
        if (e.samplePosition <= 0) {
            continue;
        }
        Marker m(e.samplePosition, sampleRate);
        m.originalPosition = e.samplePosition;
        markers.append(m);
    }
    return markers;
}
