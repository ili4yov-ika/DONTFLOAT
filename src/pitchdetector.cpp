#include "../include/pitchdetector.h"

#include <QtCore/QtMath>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace PitchDetector {

namespace {

// Сколько сэмплов приходится на период самой высокой искомой f0. Целочисленный
// лаг сам по себе квантует частоту: при 4 сэмплах на период соседние лаги
// разнесены больше чем на полутон, поэтому берём запас.
constexpr int kSamplesPerPeriodAtTop = 10;

// Окно должно вмещать два периода самой низкой искомой f0: при лаге в один
// период на сравнение остаётся ещё столько же сэмплов. Больше — только зря
// ухудшает временно́е разрешение (короткие ноты смазываются).
constexpr float kPeriodsPerFrame = 2.0f;

constexpr int kMaxFrameSize = 4096;
constexpr int kMinFrameSize = 256;
constexpr int kMedianWindow = 5;

// Порог YIN: первый минимум CMNDF ниже него считаем периодом. Берём именно
// первый, а не глобальный — иначе выигрывают кратные периоды (октава вниз).
constexpr float kYinThreshold = 0.15f;

struct FrameEstimate {
    float midi = -1.0f;       // < 0 — кадр невокализованный/тишина
    float confidence = 0.0f;
};

/// Параметры анализа, выведенные из запрошенного диапазона частот.
struct Layout {
    int factor = 1;         ///< Коэффициент децимации исходного аудио
    double workRate = 0.0;  ///< Частота дискретизации рабочего сигнала
    int frameSize = 1024;
    int hopSize = 256;
    int minLag = 2;
    int maxLag = 512;
};

QVector<float> decimate(const QVector<float>& input, int factor)
{
    if (factor <= 1) {
        return input;
    }
    QVector<float> out;
    out.reserve(input.size() / factor + 1);
    for (qint64 i = 0; i + factor <= input.size(); i += factor) {
        // Усреднение блока — простейший антиалиасинг. Рабочая частота выбрана
        // с запасом над верхней f0, поэтому его характеристики достаточно.
        float acc = 0.0f;
        for (int j = 0; j < factor; ++j) {
            acc += input[i + j];
        }
        out.append(acc / float(factor));
    }
    return out;
}

int nextPowerOfTwo(int value)
{
    int result = 1;
    while (result < value && result < kMaxFrameSize) {
        result <<= 1;
    }
    return result;
}

Layout makeLayout(int sampleRate, const Options& opt)
{
    Layout layout;

    const double nyquist = 0.5 * sampleRate;
    const double maxHz = qBound(20.0, double(opt.maxFrequencyHz), nyquist * 0.95);
    const double requestedMinHz = qBound(4.0, double(opt.minFrequencyHz), maxHz * 0.5);

    // Рабочая частота: запас и по Найквисту, и по разрешению лага сверху.
    const double desiredRate = maxHz * kSamplesPerPeriodAtTop;
    layout.factor = qBound(1, int(sampleRate / qMax(1.0, desiredRate)), 64);
    layout.workRate = double(sampleRate) / layout.factor;

    // Кадр — под самую низкую запрошенную f0, но с потолком: иначе широкий
    // диапазон (16 Гц … 9 кГц одновременно) делает анализ неподъёмным.
    const int needed = int(std::ceil(kPeriodsPerFrame * layout.workRate / requestedMinHz));
    layout.frameSize = qBound(kMinFrameSize, nextPowerOfTwo(needed), kMaxFrameSize);
    layout.hopSize = qMax(1, layout.frameSize / 4);

    // Реально достижимая нижняя граница при выбранном кадре.
    const double minHz = qMax(requestedMinHz,
                              kPeriodsPerFrame * layout.workRate / layout.frameSize);

    layout.minLag = qMax(2, int(std::floor(layout.workRate / maxHz)));
    layout.maxLag = qMin(layout.frameSize / 2, int(std::ceil(layout.workRate / minHz)));
    return layout;
}

/// Параболическое уточнение минимума по трём соседним отсчётам.
float refineLag(const std::vector<float>& cmnd, int tau)
{
    if (tau <= 0 || tau + 1 >= int(cmnd.size())) {
        return float(tau);
    }
    const float a = cmnd[tau - 1];
    const float b = cmnd[tau];
    const float c = cmnd[tau + 1];
    const float denom = a - 2.0f * b + c;
    if (std::abs(denom) < 1e-12f) {
        return float(tau);
    }
    const float shift = 0.5f * (a - c) / denom;
    if (!(shift > -1.0f && shift < 1.0f)) {
        return float(tau);
    }
    return float(tau) + shift;
}

FrameEstimate estimateFrame(const float* samples, const Layout& layout, const Options& opt,
                            std::vector<float>& diff, std::vector<float>& cmnd)
{
    FrameEstimate est;
    const int length = layout.frameSize;

    double energy = 0.0;
    for (int i = 0; i < length; ++i) {
        energy += double(samples[i]) * samples[i];
    }
    const float rms = std::sqrt(float(energy / qMax(1, length)));
    if (rms < opt.minRms) {
        return est;
    }

    const int maxLag = layout.maxLag;
    if (maxLag <= layout.minLag) {
        return est;
    }

    // Разностная функция YIN. Нормируем на число слагаемых: иначе большие лаги
    // выигрывают просто потому, что суммируют меньше точек.
    diff.assign(maxLag + 1, 0.0f);
    for (int tau = 1; tau <= maxLag; ++tau) {
        const int n = length - tau;
        if (n <= 0) {
            diff[tau] = diff[tau - 1];
            continue;
        }
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double d = double(samples[i]) - samples[i + tau];
            sum += d * d;
        }
        diff[tau] = float(sum / n);
    }

    // Кумулятивная нормировка (CMNDF) — она и подавляет кратные периоды.
    cmnd.assign(maxLag + 1, 1.0f);
    double running = 0.0;
    for (int tau = 1; tau <= maxLag; ++tau) {
        running += diff[tau];
        cmnd[tau] = running > 1e-20
            ? float(diff[tau] * tau / running)
            : 1.0f;
    }

    // Первый локальный минимум ниже порога — это основной тон, а не его кратное.
    int bestTau = -1;
    for (int tau = layout.minLag; tau <= maxLag; ++tau) {
        if (cmnd[tau] >= kYinThreshold) {
            continue;
        }
        int local = tau;
        while (local + 1 <= maxLag && cmnd[local + 1] < cmnd[local]) {
            ++local;
        }
        bestTau = local;
        break;
    }
    if (bestTau < 0) {
        // Порог не пройден — берём глобальный минимум как запасной вариант.
        int argmin = layout.minLag;
        for (int tau = layout.minLag + 1; tau <= maxLag; ++tau) {
            if (cmnd[tau] < cmnd[argmin]) {
                argmin = tau;
            }
        }
        bestTau = argmin;
    }

    const float confidence = qBound(0.0f, 1.0f - cmnd[bestTau], 1.0f);
    if (confidence < opt.minCorrelation) {
        return est;
    }

    const float refined = refineLag(cmnd, bestTau);
    if (refined <= 0.0f) {
        return est;
    }

    const float hz = float(layout.workRate / double(refined));
    est.midi = frequencyToMidi(hz, opt.referenceHz);
    est.confidence = confidence;
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

float medianOf(QVector<float>& values)
{
    if (values.isEmpty()) {
        return 0.0f;
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

} // namespace

const TuningStandard* tuningStandards(int& count)
{
    static const TuningStandard kStandards[] = {
        {"415 Hz — Baroque",        415.0f},
        {"432 Hz — Verdi",          432.0f},
        {"435 Hz — Diapason normal", 435.0f},
        {"440 Hz — ISO 16",         440.0f},
        {"442 Hz — Orchestral",     442.0f},
        {"443 Hz — Orchestral",     443.0f},
        {"444 Hz — Orchestral",     444.0f},
    };
    count = int(sizeof(kStandards) / sizeof(kStandards[0]));
    return kStandards;
}

float frequencyToMidi(float hz, float referenceHz)
{
    if (hz <= 0.0f || referenceHz <= 0.0f) {
        return -1.0f;
    }
    return 69.0f + 12.0f * std::log2(hz / referenceHz);
}

float midiToFrequency(float midi, float referenceHz)
{
    if (referenceHz <= 0.0f) {
        return 0.0f;
    }
    return referenceHz * std::pow(2.0f, (midi - 69.0f) / 12.0f);
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

    const Layout layout = makeLayout(sampleRate, options);
    const QVector<float> work = decimate(mono, layout.factor);
    if (work.size() < layout.frameSize || layout.maxLag <= layout.minLag) {
        return notes;
    }

    const int frameCount = (work.size() - layout.frameSize) / layout.hopSize + 1;
    QVector<FrameEstimate> frames(frameCount);

    std::vector<float> diff;
    std::vector<float> cmnd;
    diff.reserve(layout.maxLag + 1);
    cmnd.reserve(layout.maxLag + 1);

    // Кадры считаются независимо друг от друга, поэтому раскладываем их по
    // ядрам. Потоки — из стандартной библиотеки, а не QThreadPool: анализ
    // запускается в том числе из ARA, то есть из рабочего потока хоста и
    // ещё до того, как плагин создал QApplication. Любой QThread/QObject в
    // такой момент делает «главным» чужой поток, и последующее создание
    // QApplication в настоящем главном потоке падало внутри платформенного
    // плагина Qt — DAW вылетала при добавлении плагина на дорожку с клипами.
    const int hardwareThreads = int(std::thread::hardware_concurrency());
    const int threadCount = std::clamp(hardwareThreads > 0 ? hardwareThreads : 1, 1, 16);
    constexpr int kMinFramesForThreads = 64;

    if (threadCount <= 1 || frameCount < kMinFramesForThreads) {
        int lastReported = -1;
        for (int f = 0; f < frameCount; ++f) {
            frames[f] = estimateFrame(work.constData() + qint64(f) * layout.hopSize,
                                      layout, options, diff, cmnd);
            if (onProgress) {
                const int pct = int(qint64(f + 1) * 100 / frameCount);
                if (pct != lastReported) {
                    lastReported = pct;
                    onProgress(pct);
                }
            }
        }
    } else {
        std::atomic<int> processed { 0 };
        std::atomic<int> finished { 0 };
        const int chunkSize = (frameCount + threadCount - 1) / threadCount;

        std::vector<std::thread> workers;
        workers.reserve(std::size_t(threadCount));
        for (int chunk = 0; chunk < threadCount; ++chunk) {
            const int from = chunk * chunkSize;
            const int to = std::min(frameCount, from + chunkSize);
            if (from >= to) {
                finished.fetch_add(1, std::memory_order_release);
                continue;
            }
            workers.emplace_back([&, from, to]() {
                // Буферы разностной функции — свои у каждого потока
                std::vector<float> localDiff;
                std::vector<float> localCmnd;
                localDiff.reserve(layout.maxLag + 1);
                localCmnd.reserve(layout.maxLag + 1);
                for (int f = from; f < to; ++f) {
                    frames[f] = estimateFrame(work.constData() + qint64(f) * layout.hopSize,
                                              layout, options, localDiff, localCmnd);
                    processed.fetch_add(1, std::memory_order_relaxed);
                }
                finished.fetch_add(1, std::memory_order_release);
            });
        }

        // Ждём и по дороге отдаём прогресс — плашка анализа должна двигаться
        int lastReported = -1;
        while (finished.load(std::memory_order_acquire) < threadCount) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            if (!onProgress) {
                continue;
            }
            const int pct = int(qint64(processed.load(std::memory_order_relaxed)) * 100 / frameCount);
            if (pct != lastReported) {
                lastReported = pct;
                onProgress(pct);
            }
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
        if (onProgress && lastReported != 100) {
            onProgress(100);
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
    QVector<float> runPitches;

    auto flushRun = [&](int endFrame) {
        if (runStart < 0 || runPitch < 0 || runPitch > 127) {
            return;
        }
        const qint64 start = qint64(runStart) * layout.hopSize * layout.factor;
        const qint64 end =
            (qint64(endFrame - 1) * layout.hopSize + layout.frameSize) * qint64(layout.factor);
        if (end - start < minNoteSamples) {
            return;
        }
        PitchNote note;
        note.startSample = start;
        note.endSample = qMin<qint64>(end, mono.size());
        // Высота — медиана дробных оценок кадров: сегментация идёт по полутону,
        // но центы нужны коррекции (сдвиг = midiPitch - detectedPitch).
        const float pitch = medianOf(runPitches);
        note.detectedPitch = pitch;
        note.midiPitch = pitch;
        note.confidence = runFrames > 0 ? float(runConfidence / runFrames) : 0.0f;
        notes.append(note);
    };

    for (int f = 0; f < frames.size(); ++f) {
        const bool voiced = frames[f].midi >= 0.0f;
        const int pitch = voiced ? int(std::lround(frames[f].midi)) : -1;

        if (pitch == runPitch && voiced) {
            runConfidence += frames[f].confidence;
            ++runFrames;
            runPitches.append(frames[f].midi);
            continue;
        }

        flushRun(f);
        if (voiced) {
            runStart = f;
            runPitch = pitch;
            runConfidence = frames[f].confidence;
            runFrames = 1;
            runPitches.clear();
            runPitches.append(frames[f].midi);
        } else {
            runStart = -1;
            runPitch = -1;
            runConfidence = 0.0;
            runFrames = 0;
            runPitches.clear();
        }
    }
    flushRun(frames.size());

    return notes;
}

} // namespace PitchDetector
