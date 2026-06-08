#ifndef MARKERSFILE_H
#define MARKERSFILE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include "markerengine.h"

/**
 * @brief Файл меток для тестовых аудио (tests/source4test).
 *
 * Формат строки: bar.beat - M:SS:CC  (минуты : секунды : сотые доли секунды)
 * Пример: 1.1 - 0:01:58
 *
 * Заголовок (опционально):
 *   # BPM=80 BeatsPerBar=4 SampleRate=44100 GridStartSample=1234
 */
struct MarkersFileMeta {
    float bpm = 0.f;
    int beatsPerBar = 4;
    int sampleRate = 44100;
    qint64 gridStartSample = 0;
};

struct MarkersFileEntry {
    int bar = 1;
    int beat = 1;
    qint64 samplePosition = 0;
};

class MarkersFile
{
public:
    static QString markersPathForAudio(const QString& audioPath);
    static QString formatTimeMssCc(qint64 samplePosition, int sampleRate);
    static bool sampleToBarBeat(qint64 samplePosition, float bpm, int beatsPerBar,
                                int sampleRate, qint64 gridStartSample,
                                int* outBar, int* outBeat);

    static bool writeFile(const QString& path,
                          const MarkersFileMeta& meta,
                          const QVector<Marker>& markers,
                          int sampleRate,
                          QString* error = nullptr);

    static bool readFile(const QString& path,
                         MarkersFileMeta* meta,
                         QVector<MarkersFileEntry>* entries,
                         QString* error = nullptr);

    static QVector<Marker> markersFromEntries(const QVector<MarkersFileEntry>& entries,
                                              int sampleRate);
};

#endif // MARKERSFILE_H
