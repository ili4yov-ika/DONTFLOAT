#include "../include/pianoroll_engine.h"

#include <QtCore/QtMath>
#include <cmath>

namespace PianoRollEngine {

Viewport Viewport::compute(qint64 totalSamples, int widthPx, float zoomLevel, float horizontalOffset)
{
    Viewport vp;
    if (totalSamples <= 0 || widthPx <= 0) {
        return vp;
    }

    const float zoom = qMax(0.01f, zoomLevel);
    vp.samplesPerPixel = float(totalSamples) / (widthPx * zoom);
    vp.visibleSamples = int(widthPx * vp.samplesPerPixel);
    vp.maxStartSample = qMax(0, int(totalSamples) - vp.visibleSamples);
    vp.startSample = int(qBound(0.0f, horizontalOffset, 1.0f) * vp.maxStartSample);
    return vp;
}

float Viewport::sampleToPixelX(qint64 sample) const
{
    if (samplesPerPixel <= 0.0f) {
        return 0.0f;
    }
    return (float(sample) - float(startSample)) / samplesPerPixel;
}

qint64 Viewport::pixelToSample(int x) const
{
    if (samplesPerPixel <= 0.0f) {
        return startSample;
    }
    return startSample + qint64(x * samplesPerPixel);
}

BeatGridMetrics computeBeatGridMetrics(float bpm, int beatsPerBar, int sampleRate)
{
    BeatGridMetrics metrics;
    if (bpm <= 0.0f || sampleRate <= 0) {
        return metrics;
    }

    metrics.samplesPerBeat = (60.0f * sampleRate) / bpm;

    const float barLengthInQuarters =
        (beatsPerBar == 6) ? 3.0f : (beatsPerBar == 12) ? 6.0f : float(qMax(1, beatsPerBar));
    const float samplesPerBar = barLengthInQuarters * metrics.samplesPerBeat;

    metrics.subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
    metrics.samplesPerSubdivision = samplesPerBar / float(metrics.subdivisionsPerBar);
    return metrics;
}

WaveformEnvelope buildWaveformEnvelope(const QVector<QVector<float>>& channels,
                                       int pixelWidth,
                                       int widgetHeight)
{
    WaveformEnvelope envelope;
    if (channels.isEmpty() || channels[0].isEmpty() || pixelWidth <= 0 || widgetHeight <= 2) {
        return envelope;
    }

    const int totalFrames = channels[0].size();
    const int numChannels = channels.size();

    envelope.pixelWidth = pixelWidth;
    envelope.samplesPerPixel = float(totalFrames) / float(pixelWidth);
    if (envelope.samplesPerPixel < 1.0f) {
        envelope.pixelWidth = totalFrames;
        envelope.samplesPerPixel = 1.0f;
    }

    envelope.upper.resize(envelope.pixelWidth);
    envelope.lower.resize(envelope.pixelWidth);

    const int centerY = widgetHeight / 2;
    const int amplitude = qMax(1, centerY - 1);

    for (int column = 0; column < envelope.pixelWidth; ++column) {
        const int sampleStart = int(column * envelope.samplesPerPixel);
        const int sampleEnd = int((column + 1) * envelope.samplesPerPixel);

        float maxSample = 0.0f;
        float minSample = 0.0f;

        for (int frame = sampleStart; frame < sampleEnd; ++frame) {
            if (frame >= totalFrames) {
                break;
            }

            float mixed = 0.0f;
            for (int channel = 0; channel < numChannels; ++channel) {
                if (frame < channels[channel].size()) {
                    mixed += channels[channel][frame];
                }
            }
            mixed /= qMax(1, numChannels);

            maxSample = qMax(maxSample, mixed);
            minSample = qMin(minSample, mixed);
        }

        envelope.upper[column] = qBound(0, centerY - int(maxSample * amplitude), widgetHeight - 1);
        envelope.lower[column] = qBound(0, centerY - int(minSample * amplitude), widgetHeight - 1);
    }

    return envelope;
}

QVector<GridLine> visibleGridLines(const Viewport& viewport,
                                     const BeatGridMetrics& metrics,
                                     qint64 gridStartSample,
                                     int widthPx)
{
    QVector<GridLine> lines;
    if (metrics.samplesPerSubdivision <= 0.0f || widthPx <= 0) {
        return lines;
    }

    int firstSubdivision = 0;
    if (gridStartSample > 0) {
        const float subsFromGrid =
            float(viewport.startSample - gridStartSample) / metrics.samplesPerSubdivision;
        firstSubdivision = int(std::floor(subsFromGrid));
    } else {
        firstSubdivision = int(viewport.startSample / metrics.samplesPerSubdivision);
    }

    for (int subdivision = firstSubdivision; ; ++subdivision) {
        const qint64 samplePos = gridStartSample > 0
            ? qint64(gridStartSample + subdivision * metrics.samplesPerSubdivision)
            : qint64(subdivision * metrics.samplesPerSubdivision);
        if (samplePos < viewport.startSample) {
            continue;
        }

        const float x = viewport.sampleToPixelX(samplePos);
        if (x >= widthPx) {
            break;
        }

        GridLine line;
        line.x = x;
        line.isBarLine = (subdivision % metrics.subdivisionsPerBar) == 0;
        lines.append(line);
    }

    return lines;
}

QVector<GridLine> visibleGridLines(const Viewport& viewport,
                                   float bpm,
                                   int beatsPerBar,
                                   int sampleRate,
                                   qint64 gridStartSample,
                                   int widthPx)
{
    return visibleGridLines(viewport,
                            computeBeatGridMetrics(bpm, beatsPerBar, sampleRate),
                            gridStartSample,
                            widthPx);
}

qint64 snapToGrid(qint64 sample,
                  const BeatGridMetrics& metrics,
                  qint64 gridStartSample,
                  bool enabled)
{
    if (!enabled || metrics.samplesPerSubdivision <= 0.0f) {
        return sample;
    }

    const qint64 ref = gridStartSample > 0 ? gridStartSample : 0;
    const double subdivisions = double(sample - ref) / double(metrics.samplesPerSubdivision);
    const qint64 snapped = ref + qint64(std::llround(subdivisions) * metrics.samplesPerSubdivision);
    return qMax(qint64(0), snapped);
}

qint64 snapToGrid(qint64 sample,
                  float bpm,
                  int beatsPerBar,
                  int sampleRate,
                  qint64 gridStartSample,
                  bool enabled)
{
    return snapToGrid(sample,
                      computeBeatGridMetrics(bpm, beatsPerBar, sampleRate),
                      gridStartSample,
                      enabled);
}

bool canSplitNoteAt(qint64 startSample,
                    qint64 endSample,
                    qint64 cutSample,
                    qint64 minPartSamples)
{
    const qint64 minPart = qMax<qint64>(1, minPartSamples);
    if (endSample - startSample < 2 * minPart) {
        return false;
    }
    return cutSample - startSample >= minPart && endSample - cutSample >= minPart;
}

bool isBlackKey(int midiNote)
{
    switch (midiNote % 12) {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:
        return true;
    default:
        return false;
    }
}

static int pitchClassFromRootName(const QString& rootName)
{
    static const struct {
        const char* name;
        int pitchClass;
    } roots[] = {
        {"C", 0}, {"C#", 1}, {"D", 2}, {"D#", 3}, {"E", 4}, {"F", 5},
        {"F#", 6}, {"G", 7}, {"G#", 8}, {"A", 9}, {"A#", 10}, {"B", 11},
    };

    for (const auto& root : roots) {
        if (rootName.compare(QString::fromLatin1(root.name), Qt::CaseInsensitive) == 0) {
            return root.pitchClass;
        }
    }
    return -1;
}

static bool pitchClassInScale(int pitchClass, int rootPitchClass, bool isMinor)
{
    static const int majorIntervals[] = {0, 2, 4, 5, 7, 9, 11};
    static const int minorIntervals[] = {0, 2, 3, 5, 7, 8, 10};

    const int relative = (pitchClass - rootPitchClass + 12) % 12;
    const int* intervals = isMinor ? minorIntervals : majorIntervals;
    const int intervalCount = isMinor ? 7 : 7;

    for (int i = 0; i < intervalCount; ++i) {
        if (relative == intervals[i]) {
            return true;
        }
    }
    return false;
}

KeySignature KeySignature::fromString(const QString& keyText)
{
    KeySignature key;
    const QString trimmed = keyText.trimmed();
    if (trimmed.isEmpty()) {
        return key;
    }

    const QStringList parts = trimmed.split(QChar(' '), Qt::SkipEmptyParts);
    if (parts.size() != 2) {
        return key;
    }

    const QString mode = parts[1];
    if (mode.compare(QStringLiteral("Major"), Qt::CaseInsensitive) != 0
        && mode.compare(QStringLiteral("Minor"), Qt::CaseInsensitive) != 0) {
        return key;
    }

    key.rootPitchClass = pitchClassFromRootName(parts[0]);
    key.isMinor = mode.compare(QStringLiteral("Minor"), Qt::CaseInsensitive) == 0;
    return key;
}

bool KeySignature::containsMidiNote(int midiNote) const
{
    if (!isValid()) {
        return false;
    }
    return pitchClassInScale(midiNote % 12, rootPitchClass, isMinor);
}

bool isPitchInKeys(int midiNote, const KeySignature& primary, const KeySignature& secondary)
{
    if (!primary.isValid() && !secondary.isValid()) {
        return true;
    }
    if (primary.isValid() && primary.containsMidiNote(midiNote)) {
        return true;
    }
    if (secondary.isValid() && secondary.containsMidiNote(midiNote)) {
        return true;
    }
    return false;
}

QString midiNoteName(int midiNote)
{
    static const QString noteNames[] = {
        QStringLiteral("C"), QStringLiteral("C#"), QStringLiteral("D"), QStringLiteral("D#"),
        QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("F#"), QStringLiteral("G"),
        QStringLiteral("G#"), QStringLiteral("A"), QStringLiteral("A#"), QStringLiteral("B")
    };

    const int octave = (midiNote / 12) - 1;
    const int note = midiNote % 12;
    return QStringLiteral("%1%2").arg(noteNames[note]).arg(octave);
}

} // namespace PianoRollEngine
