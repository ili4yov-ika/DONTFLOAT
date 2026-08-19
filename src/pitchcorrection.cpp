#include "../include/pitchcorrection.h"
#include "../include/timestretchprocessor.h"

#include <QtCore/QtMath>

#include <algorithm>
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

/** Гасит участок [from, to) с короткими фейдами — место ушедшей ноты. */
void silenceRange(QVector<float>& channel, qint64 from, qint64 to)
{
    const int len = int(to - from);
    if (len <= 0) {
        return;
    }
    const int fade = qMin(kCrossfadeSamples, len / 4);
    for (int i = 0; i < len; ++i) {
        float w = 0.0f;  // 0 — тишина, 1 — исходный звук
        if (i < fade) {
            w = 1.0f - float(i) / float(fade);
        } else if (i >= len - fade) {
            w = 1.0f - float(len - 1 - i) / float(fade);
        }
        channel[int(from) + i] *= w;
    }
}

} // namespace

bool hasPendingEdits(const QVector<PitchDetector::PitchNote>& notes)
{
    for (const PitchDetector::PitchNote& note : notes) {
        if (std::abs(note.midiPitch - note.detectedPitch) > 0.01f) {
            return true;
        }
        // Перенос ноты по времени тоже требует пересчёта: звук надо забрать
        // с прежнего места и положить на новое
        if (note.isMovedInTime()) {
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

    // Сначала освобождаем места, откуда ноты ушли: если этого не сделать,
    // старое звучание останется поверх нового и перестановка будет не слышна
    for (const PitchDetector::PitchNote& note : notes) {
        if (!note.isMovedInTime()) {
            continue;
        }
        const qint64 from = qBound<qint64>(0, note.sourceStart(), totalSamples);
        const qint64 to = qBound<qint64>(from, note.sourceEnd(), totalSamples);
        if (to <= from) {
            continue;
        }
        for (QVector<float>& channel : out) {
            silenceRange(channel, from, to);
        }
    }

    for (const PitchDetector::PitchNote& note : notes) {
        const float semitones = note.midiPitch - note.detectedPitch;
        const bool pitchChanged = std::abs(semitones) >= 0.01f;
        const bool moved = note.isMovedInTime();
        if (!pitchChanged && !moved) {
            continue;
        }

        // Звук берём с исходного места ноты, кладём — на нынешнее
        const qint64 sourceStart = qBound<qint64>(0, note.sourceStart(), totalSamples);
        const qint64 sourceEnd = qBound<qint64>(sourceStart, note.sourceEnd(), totalSamples);
        const int sourceLen = int(sourceEnd - sourceStart);
        const qint64 targetStart = qBound<qint64>(0, note.startSample, totalSamples);
        const int targetLen = int(qMin<qint64>(sourceLen, totalSamples - targetStart));
        if (sourceLen < kCrossfadeSamples * 2 || targetLen < kCrossfadeSamples * 2) {
            continue;
        }

        const float ratio = std::pow(2.0f, semitones / 12.0f);
        for (int channelIndex = 0; channelIndex < out.size(); ++channelIndex) {
            // Источник читаем из исходного аудио: соседняя нота могла уже
            // занять это место в выходном буфере
            const QVector<float>& source = channels[channelIndex];
            QVector<float> segment(sourceLen);
            std::copy(source.constBegin() + sourceStart,
                      source.constBegin() + sourceStart + sourceLen, segment.begin());

            QVector<float> processed = pitchChanged
                ? shiftSegmentPitch(segment, ratio, sampleRate)
                : segment;
            if (processed.size() != sourceLen) {
                continue;
            }
            if (targetLen < sourceLen) {
                processed.resize(targetLen);  // нота уехала к концу трека
            }
            blendInto(out[channelIndex], processed, int(targetStart));
        }
    }

    return out;
}

} // namespace PitchCorrection
