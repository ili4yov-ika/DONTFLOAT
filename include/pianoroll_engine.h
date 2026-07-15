#ifndef PIANOROLL_ENGINE_H
#define PIANOROLL_ENGINE_H

/**
 * Движок пианоролла для PitchGridWidget: viewport таймлайна, огибающая волны,
 * тактовая сетка и snap к долям.
 */

#include <QtCore/QString>
#include <QtCore/QVector>

namespace PianoRollEngine {

struct Viewport {
    float samplesPerPixel = 1.0f;
    int visibleSamples = 0;
    int maxStartSample = 0;
    int startSample = 0;

    static Viewport compute(qint64 totalSamples, int widthPx, float zoomLevel, float horizontalOffset);
    float sampleToPixelX(qint64 sample) const;
    qint64 pixelToSample(int x) const;
};

struct WaveformEnvelope {
    QVector<int> upper;
    QVector<int> lower;
    int pixelWidth = 0;
    float samplesPerPixel = 1.0f;

    bool isEmpty() const { return pixelWidth <= 0 || upper.isEmpty(); }
};

struct GridLine {
    float x = 0.0f;
    bool isBarLine = false;
};

struct BeatGridMetrics {
    float samplesPerBeat = 0.0f;
    float samplesPerSubdivision = 0.0f;
    int subdivisionsPerBar = 4;
};

BeatGridMetrics computeBeatGridMetrics(float bpm, int beatsPerBar, int sampleRate);

WaveformEnvelope buildWaveformEnvelope(const QVector<QVector<float>>& channels,
                                       int pixelWidth,
                                       int widgetHeight);

QVector<GridLine> visibleGridLines(const Viewport& viewport,
                                   const BeatGridMetrics& metrics,
                                   qint64 gridStartSample,
                                   int widthPx);

QVector<GridLine> visibleGridLines(const Viewport& viewport,
                                   float bpm,
                                   int beatsPerBar,
                                   int sampleRate,
                                   qint64 gridStartSample,
                                   int widthPx);

qint64 snapToGrid(qint64 sample,
                  const BeatGridMetrics& metrics,
                  qint64 gridStartSample,
                  bool enabled);

qint64 snapToGrid(qint64 sample,
                  float bpm,
                  int beatsPerBar,
                  int sampleRate,
                  qint64 gridStartSample,
                  bool enabled);

bool isBlackKey(int midiNote);

/** Тональность вида «C Major» / «A Minor»; пустая строка — не задана. */
struct KeySignature {
    int rootPitchClass = -1;
    bool isMinor = false;

    bool isValid() const { return rootPitchClass >= 0 && rootPitchClass < 12; }

    static KeySignature fromString(const QString& keyText);
    bool containsMidiNote(int midiNote) const;
};

bool isPitchInKeys(int midiNote, const KeySignature& primary, const KeySignature& secondary);

QString midiNoteName(int midiNote);

} // namespace PianoRollEngine

#endif // PIANOROLL_ENGINE_H
