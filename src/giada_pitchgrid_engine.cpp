#include "../include/giada_pitchgrid_engine.h"

#include <QtCore/QtMath>
#include <cmath>

namespace GiadaPitchGridEngine {

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

WaveformPeaks buildWaveformPeaks(const QVector<QVector<float>>& channels,
                                 int pixelWidth,
                                 int widgetHeight)
{
    WaveformPeaks peaks;
    if (channels.isEmpty() || channels[0].isEmpty() || pixelWidth <= 0 || widgetHeight <= 2) {
        return peaks;
    }

    const int totalFrames = channels[0].size();
    const int numChannels = channels.size();

    peaks.pixelWidth = pixelWidth;
    peaks.ratio = float(totalFrames) / float(pixelWidth);
    if (peaks.ratio < 1.0f) {
        peaks.pixelWidth = totalFrames;
        peaks.ratio = 1.0f;
    }

    peaks.upper.resize(peaks.pixelWidth);
    peaks.lower.resize(peaks.pixelWidth);

    const int centerY = widgetHeight / 2;
    const int amplitude = qMax(1, centerY - 1);

    for (int i = 0; i < peaks.pixelWidth; ++i) {
        const int pc = int(i * peaks.ratio);
        const int pn = int((i + 1) * peaks.ratio);

        float peakSup = 0.0f;
        float peakInf = 0.0f;

        for (int k = pc; k < pn; ++k) {
            if (k >= totalFrames) {
                continue;
            }

            float avg = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch) {
                if (k < channels[ch].size()) {
                    avg += channels[ch][k];
                }
            }
            avg /= qMax(1, numChannels);

            if (avg > peakSup) {
                peakSup = avg;
            } else if (avg <= peakInf) {
                peakInf = avg;
            }
        }

        peaks.upper[i] = centerY - int(peakSup * amplitude);
        peaks.lower[i] = centerY - int(peakInf * amplitude);

        peaks.upper[i] = qBound(0, peaks.upper[i], widgetHeight - 1);
        peaks.lower[i] = qBound(0, peaks.lower[i], widgetHeight - 1);
    }

    return peaks;
}

static void beatGridIntervals(float bpm,
                              int beatsPerBar,
                              int sampleRate,
                              float& samplesPerBeat,
                              float& samplesPerSubdivision,
                              int& subdivisionsPerBar)
{
    samplesPerBeat = (60.0f * sampleRate) / qMax(0.01f, bpm);
    const float barLengthInQuarters =
        (beatsPerBar == 6) ? 3.0f : (beatsPerBar == 12) ? 6.0f : float(qMax(1, beatsPerBar));
    const float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
    samplesPerSubdivision = samplesPerBar / float(subdivisionsPerBar);
}

QVector<BeatLine> visibleBeatLines(const Viewport& viewport,
                                   float bpm,
                                   int beatsPerBar,
                                   int sampleRate,
                                   qint64 gridStartSample,
                                   int widthPx)
{
    QVector<BeatLine> lines;
    if (bpm <= 0.0f || sampleRate <= 0 || widthPx <= 0) {
        return lines;
    }

    float samplesPerBeat = 0.0f;
    float samplesPerSubdivision = 0.0f;
    int subdivisionsPerBar = 4;
    beatGridIntervals(bpm, beatsPerBar, sampleRate,
                      samplesPerBeat, samplesPerSubdivision, subdivisionsPerBar);

    int firstSubdivision = 0;
    if (gridStartSample > 0) {
        const float subsFromGrid = float(viewport.startSample - gridStartSample) / samplesPerSubdivision;
        firstSubdivision = int(std::floor(subsFromGrid));
    } else {
        firstSubdivision = int(viewport.startSample / samplesPerSubdivision);
    }

    for (int sub = firstSubdivision; ; ++sub) {
        const qint64 samplePos = gridStartSample > 0
            ? qint64(gridStartSample + sub * qint64(samplesPerSubdivision))
            : qint64(sub * qint64(samplesPerSubdivision));
        if (samplePos < viewport.startSample) {
            continue;
        }

        const float x = viewport.sampleToPixelX(samplePos);
        if (x >= widthPx) {
            break;
        }

        BeatLine line;
        line.x = x;
        line.isBarLine = (sub % subdivisionsPerBar) == 0;
        lines.append(line);
    }

    return lines;
}

qint64 snapToBeatGrid(qint64 sample,
                      float bpm,
                      int beatsPerBar,
                      int sampleRate,
                      qint64 gridStartSample,
                      bool enabled)
{
    if (!enabled || bpm <= 0.0f || sampleRate <= 0) {
        return sample;
    }

    float samplesPerBeat = 0.0f;
    float samplesPerSubdivision = 0.0f;
    int subdivisionsPerBar = 4;
    beatGridIntervals(bpm, beatsPerBar, sampleRate,
                      samplesPerBeat, samplesPerSubdivision, subdivisionsPerBar);
    Q_UNUSED(subdivisionsPerBar);

    const qint64 ref = gridStartSample > 0 ? gridStartSample : 0;
    const double subs = double(sample - ref) / double(samplesPerSubdivision);
    const qint64 snapped = ref + qint64(std::llround(subs) * samplesPerSubdivision);
    return qMax(qint64(0), snapped);
}

bool isBlackKey(int midiNote)
{
    const int note = midiNote % 12;
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

} // namespace GiadaPitchGridEngine
