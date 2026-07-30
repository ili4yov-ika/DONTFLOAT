#include "../include/pitchcorrection.h"
#include "../include/timestretchprocessor.h"

#include <QtCore/QtMath>
#include <cmath>

namespace PitchCorrection {

namespace {

constexpr int kCrossfadeSamples = 256;

/** Ресемплинг к точной длине targetLength (линейная интерполяция). */
QVector<float> resampleToLength(const QVector<float>& input, int targetLength)
{
    if (input.isEmpty() || targetLength <= 0) {
        return {};
    }
    QVector<float> out(targetLength);
    const double step = double(input.size() - 1) / qMax(1, targetLength - 1);
    for (int i = 0; i < targetLength; ++i) {
        const double pos = i * step;
        const int idx = qMin(int(pos), input.size() - 2);
        const float t = float(pos - idx);
        if (idx + 1 < input.size()) {
            out[i] = input[idx] * (1.0f - t) + input[idx + 1] * t;
        } else {
            out[i] = input[idx];
        }
    }
    return out;
}

/** Сдвиг высоты сегмента на pitchRatio без изменения длины. */
QVector<float> shiftSegmentPitch(const QVector<float>& segment,
                                 float pitchRatio, int sampleRate)
{
    // 1) stretch с тонкомпенсацией: длина ×ratio, высота прежняя
    // 2) ресемплинг к исходной длине: высота ×ratio
    const QVector<float> stretched =
        TimeStretchProcessor::processSegment(segment, pitchRatio, true, sampleRate);
    if (stretched.isEmpty()) {
        return segment;
    }
    return resampleToLength(stretched, segment.size());
}

/** Вписывает processed на место [start, start+len) с кроссфейдом на краях. */
void blendInto(QVector<float>& channel, const QVector<float>& processed, int start)
{
    const int len = processed.size();
    const int fade = qMin(kCrossfadeSamples, len / 4);
    for (int i = 0; i < len; ++i) {
        float w = 1.0f;
        if (i < fade) {
            w = float(i) / float(fade);
        } else if (i >= len - fade) {
            w = float(len - 1 - i) / float(fade);
        }
        const int idx = start + i;
        channel[idx] = channel[idx] * (1.0f - w) + processed[i] * w;
    }
}

} // namespace

bool hasPendingEdits(const QVector<PitchDetector::PitchNote>& notes)
{
    for (const PitchDetector::PitchNote& note : notes) {
        if (std::abs(note.midiPitch - note.detectedPitch) > 0.01f) {
            return true;
        }
    }
    return false;
}

QVector<QVector<float>> apply(const QVector<QVector<float>>& channels,
                              const QVector<PitchDetector::PitchNote>& notes,
                              int sampleRate)
{
    if (channels.isEmpty() || sampleRate <= 0 || !hasPendingEdits(notes)) {
        return channels;
    }

    QVector<QVector<float>> out = channels;
    const qint64 totalSamples = channels[0].size();

    for (const PitchDetector::PitchNote& note : notes) {
        const float semitones = note.midiPitch - note.detectedPitch;
        if (std::abs(semitones) < 0.01f) {
            continue;
        }
        const qint64 start = qBound<qint64>(0, note.startSample, totalSamples);
        const qint64 end = qBound<qint64>(start, note.endSample, totalSamples);
        const int len = int(end - start);
        if (len < kCrossfadeSamples * 2) {
            continue;
        }

        const float ratio = std::pow(2.0f, semitones / 12.0f);
        for (QVector<float>& channel : out) {
            QVector<float> segment(len);
            std::copy(channel.constBegin() + start,
                      channel.constBegin() + start + len, segment.begin());
            const QVector<float> shifted = shiftSegmentPitch(segment, ratio, sampleRate);
            if (shifted.size() == len) {
                blendInto(channel, shifted, int(start));
            }
        }
    }

    return out;
}

} // namespace PitchCorrection
