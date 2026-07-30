#include "../include/pitchdetector.h"

#include <QtCore/QtMath>
#include <algorithm>
#include <cmath>

namespace PitchDetector {

namespace {

constexpr int kTargetSampleRate = 11025; // Децимация ускоряет автокорреляцию в ~16 раз
constexpr int kFrameSize = 1024;
constexpr int kHopSize = 256;
constexpr int kMedianWindow = 5;

struct FrameEstimate {
    float midi = -1.0f;       // < 0 — кадр невокализованный/тишина
    float correlation = 0.0f;
};

QVector<float> decimate(const QVector<float>& input, int factor)
{
    if (factor <= 1) {
        return input;
    }
    QVector<float> out;
    out.reserve(input.size() / factor + 1);
    for (int i = 0; i + factor <= input.size(); i += factor) {
        // Усреднение блока — простейший антиалиасинг, достаточный для f0 < 1.2 кГц
        float acc = 0.0f;
        for (int j = 0; j < factor; ++j) {
            acc += input[i + j];
        }
        out.append(acc / float(factor));
    }
    return out;
}

FrameEstimate estimateFrame(const float* samples, int length, int sampleRate,
                            const Options& opt)
{
    FrameEstimate est;

    double energy = 0.0;
    for (int i = 0; i < length; ++i) {
        energy += double(samples[i]) * samples[i];
    }
    const float rms = std::sqrt(float(energy / qMax(1, length)));
    if (rms < opt.minRms) {
        return est;
    }

    const int minLag = qMax(2, int(sampleRate / opt.maxFrequencyHz));
    const int maxLag = qMin(length / 2, int(sampleRate / opt.minFrequencyHz));
    if (maxLag <= minLag) {
        return est;
    }

    float bestNorm = -1.0f;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double corr = 0.0;
        double normA = 0.0;
        double normB = 0.0;
        const int n = length - lag;
        for (int i = 0; i < n; ++i) {
            const float a = samples[i];
            const float b = samples[i + lag];
            corr += double(a) * b;
            normA += double(a) * a;
            normB += double(b) * b;
        }
        const double denom = std::sqrt(normA * normB);
        if (denom <= 1e-12) {
            continue;
        }
        const float norm = float(corr / denom);
        if (norm > bestNorm) {
            bestNorm = norm;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestNorm < opt.minCorrelation) {
        return est;
    }

    // Защита от октавной ошибки: если lag/2 даёт почти такую же корреляцию,
    // истинный период вдвое короче (нота на октаву выше).
    const int halfLag = bestLag / 2;
    if (halfLag >= minLag) {
        double corr = 0.0;
        double normA = 0.0;
        double normB = 0.0;
        const int n = length - halfLag;
        for (int i = 0; i < n; ++i) {
            const float a = samples[i];
            const float b = samples[i + halfLag];
            corr += double(a) * b;
            normA += double(a) * a;
            normB += double(b) * b;
        }
        const double denom = std::sqrt(normA * normB);
        if (denom > 1e-12 && float(corr / denom) > bestNorm * 0.9f) {
            bestLag = halfLag;
        }
    }

    const float hz = float(sampleRate) / float(bestLag);
    est.midi = frequencyToMidi(hz);
    est.correlation = bestNorm;
    return est;
}

QVector<FrameEstimate> smoothEstimates(const QVector<FrameEstimate>& frames)
{
    QVector<FrameEstimate> out = frames;
    const int half = kMedianWindow / 2;
    QVector<float> window;
    window.reserve(kMedianWindow);

    for (int i = 0; i < frames.size(); ++i) {
        if (frames[i].midi < 0.0f) {
            continue;
        }
        window.clear();
        for (int j = qMax(0, i - half); j <= qMin(frames.size() - 1, i + half); ++j) {
            if (frames[j].midi >= 0.0f) {
                window.append(frames[j].midi);
            }
        }
        if (window.size() >= 3) {
            std::sort(window.begin(), window.end());
            out[i].midi = window[window.size() / 2];
        }
    }
    return out;
}

} // namespace

float frequencyToMidi(float hz)
{
    if (hz <= 0.0f) {
        return -1.0f;
    }
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

QVector<PitchNote> detectNotes(const QVector<float>& mono,
                               int sampleRate,
                               const Options& options,
                               const std::function<void(int)>& onProgress)
{
    QVector<PitchNote> notes;
    if (mono.isEmpty() || sampleRate <= 0) {
        return notes;
    }

    const int factor = qMax(1, sampleRate / kTargetSampleRate);
    const QVector<float> work = decimate(mono, factor);
    const int workRate = sampleRate / factor;
    if (work.size() < kFrameSize) {
        return notes;
    }

    const int frameCount = (work.size() - kFrameSize) / kHopSize + 1;
    QVector<FrameEstimate> frames(frameCount);

    int lastReported = -1;
    for (int f = 0; f < frameCount; ++f) {
        frames[f] = estimateFrame(work.constData() + qint64(f) * kHopSize,
                                  kFrameSize, workRate, options);
        if (onProgress) {
            const int pct = int(qint64(f + 1) * 100 / frameCount);
            if (pct != lastReported) {
                lastReported = pct;
                onProgress(pct);
            }
        }
    }

    frames = smoothEstimates(frames);

    // Сегментация: подряд идущие кадры с одинаковым округлённым полутоном → нота
    const qint64 minNoteSamples =
        qint64(options.minNoteDurationMs) * sampleRate / 1000;

    int runStart = -1;
    int runPitch = -1;
    double runConfidence = 0.0;
    int runFrames = 0;

    auto flushRun = [&](int endFrame) {
        if (runStart < 0 || runPitch < 0 || runPitch > 127) {
            return;
        }
        const qint64 start = qint64(runStart) * kHopSize * factor;
        const qint64 end =
            (qint64(endFrame - 1) * kHopSize + kFrameSize) * qint64(factor);
        if (end - start < minNoteSamples) {
            return;
        }
        PitchNote note;
        note.startSample = start;
        note.endSample = qMin<qint64>(end, mono.size());
        note.midiPitch = float(runPitch);
        note.detectedPitch = float(runPitch);
        note.confidence = runFrames > 0 ? float(runConfidence / runFrames) : 0.0f;
        notes.append(note);
    };

    for (int f = 0; f < frames.size(); ++f) {
        const bool voiced = frames[f].midi >= 0.0f;
        const int pitch = voiced ? int(std::lround(frames[f].midi)) : -1;

        if (pitch == runPitch && voiced) {
            runConfidence += frames[f].correlation;
            ++runFrames;
            continue;
        }

        flushRun(f);
        if (voiced) {
            runStart = f;
            runPitch = pitch;
            runConfidence = frames[f].correlation;
            runFrames = 1;
        } else {
            runStart = -1;
            runPitch = -1;
            runConfidence = 0.0;
            runFrames = 0;
        }
    }
    flushRun(frames.size());

    return notes;
}

} // namespace PitchDetector
