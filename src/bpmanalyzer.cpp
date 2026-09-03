#include "../include/bpmanalyzer.h"
#include <QtCore/QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef USE_MIXXX_QM_DSP
#include <dsp/onsets/DetectionFunction.h>
#include <dsp/tempotracking/TempoTrackV2.h>
#endif

// Константы из Mixxx
namespace {
    constexpr float kStepSecs = 0.01161f; // ~12ms разрешение для BeatMap
    constexpr int kMaximumBinSizeHz = 50; // Hz

    constexpr float kPeakThresholdMin = 0.05f;
    constexpr float kPeakThresholdMax = 0.3f;
    constexpr float kPeakThresholdStep = 0.05f;
    constexpr int kMinPeaksForAnalysis = 10;

    // Вспомогательная функция для вычисления следующей степени двойки
    int nextPowerOfTwo(int value) {
        int result = 1;
        while (result < value) {
            result *= 2;
        }
        return result;
    }

    bool sanitizeBeatPeriods(std::vector<int>& beatPeriod, size_t dfSize) {
        const int maxSafePeriod = std::max(1, static_cast<int>(dfSize) - 1);
        bool hasValidPeriod = false;
        for (int& period : beatPeriod) {
            if (period <= 0) {
                period = 1;
            } else if (period > maxSafePeriod) {
                period = maxSafePeriod;
            }
            hasValidPeriod = true;
        }
        return hasValidPeriod;
    }

    // viterbi_decode заполняет только первые T элементов; хвост остаётся 0.
    // В TempoTrackV2::calculateBeats period==0 даёт mu==0 и деление на ноль в log(.../mu) → UB и падения STL в Debug MSVC.
    void stabilizeBeatPeriodTail(std::vector<int>& beatPeriod) {
        int carry = 1;
        for (size_t i = 0; i < beatPeriod.size(); ++i) {
            if (beatPeriod[i] > 0) {
                carry = beatPeriod[i];
            } else {
                beatPeriod[i] = carry;
            }
        }
    }

    BPMAnalyzer::BeatInfo makeBeatInfo(qint64 position, float energy, float confidence) {
        BPMAnalyzer::BeatInfo beat;
        beat.position = position;
        beat.expectedPosition = position;
        beat.confidence = confidence;
        beat.deviation = 0.0f;
        beat.energy = energy;
        return beat;
    }
}

BPMAnalyzer::AnalysisResult BPMAnalyzer::analyzeBPM(const QVector<float>& samples,
                                                    int sampleRate,
                                                    const AnalysisOptions& options) {
    // Если включен режим Mixxx, используем их алгоритм
    if (options.useMixxxAlgorithm) {
        return analyzeBPMUsingMixxx(samples, sampleRate, options);
    }

    // Если задан предварительно определенный BPM, используем его
    if (options.useInitialBPM && options.initialBPM > 0.0f) {
        qDebug() << "Using initial BPM:" << options.initialBPM;
        return createBeatGridFromBPM(samples, sampleRate, options.initialBPM, options);
    }

    // Проверяем, есть ли метаданные BPM в файле (если доступны)
    // Это можно расширить для чтения тегов из аудиофайлов
    if (options.trustFileBPM && options.fileBPM > 0.0f) {
        qDebug() << "Using file BPM:" << options.fileBPM;
        return createBeatGridFromBPM(samples, sampleRate, options.fileBPM, options);
    }

    // Улучшенный алгоритм анализа BPM (поля AnalysisResult инициализированы по умолчанию)
    AnalysisResult result;

    if (samples.isEmpty()) {
        qDebug() << "No samples provided for BPM analysis";
        return result;
    }

    qDebug() << "Starting BPM analysis with" << samples.size() << "samples at" << sampleRate << "Hz";

    // Множественный анализ с разными параметрами
    QVector<AnalysisResult> candidates;

    // Анализ 1: Обнаружение пиков с разными порогами
    for (float threshold = kPeakThresholdMin; threshold <= kPeakThresholdMax; threshold += kPeakThresholdStep) {
        auto peaks = detectPeaks(samples, threshold);
        if (peaks.size() < kMinPeaksForAnalysis) continue;

        float confidence;
        float avgInterval = calculateAverageInterval(peaks, options.assumeFixedTempo, &confidence);
        float bpm = estimateBPM(avgInterval, sampleRate, options);

        if (isValidBPM(bpm, options)) {
            AnalysisResult candidate;
            candidate.bpm = bpm;
            candidate.confidence = confidence;
            candidate.beats = findBeats(samples, bpm, sampleRate, options);
            candidate.preliminaryBPM = 0.0f;
            candidate.hasPreliminaryBPM = false;
            candidates.append(candidate);

            qDebug() << "BPM candidate:" << bpm << "confidence:" << confidence << "threshold:" << threshold;
        }
    }

    // Анализ 2: Анализ по окнам (для треков с переменным темпом)
    if (!options.assumeFixedTempo) {
        int windowSize = sampleRate * 10; // 10 секунд
        for (int start = 0; start < static_cast<int>(samples.size()) - windowSize; start += windowSize / 2) {
            QVector<float> window(samples.begin() + start, samples.begin() + start + windowSize);
            auto peaks = detectPeaks(window, 0.1f);
            if (peaks.size() < 5) continue;

            float confidence;
            float avgInterval = calculateAverageInterval(peaks, false, &confidence);
            float bpm = estimateBPM(avgInterval, sampleRate, options);

            if (isValidBPM(bpm, options)) {
                AnalysisResult candidate;
                candidate.bpm = bpm;
                candidate.confidence = confidence;
                candidate.beats = findBeats(window, bpm, sampleRate, options);
                candidates.append(candidate);
            }
        }
    }

    // Выбираем лучший кандидат
    if (candidates.isEmpty()) {
        qDebug() << "No valid BPM candidates found";
        return result;
    }

    // Сортируем по уверенности
    std::sort(candidates.begin(), candidates.end(),
              [](const AnalysisResult& a, const AnalysisResult& b) {
                  return a.confidence > b.confidence;
              });

    result = candidates.first();
    // В не-Mixxx пути предварительный BPM не рассчитываем отдельно
    result.preliminaryBPM = 0.0f;
    result.hasPreliminaryBPM = false;

    // Корректируем основной результат к стандартным BPM
    float correctedMainBPM = correctToStandardBPM(result.bpm);
    if (std::abs(correctedMainBPM - result.bpm) < 10.0f) {
        qDebug() << "Corrected main BPM from" << result.bpm << "to" << correctedMainBPM;
        result.bpm = correctedMainBPM;
        result.beats = findBeats(samples, result.bpm, sampleRate, options);
    }

    // Дополнительная проверка: ищем близкие BPM и выбираем наиболее частый
    QVector<QPair<float, int>> bpmCounts;
    for (const auto& candidate : candidates) {
        bool found = false;
        for (auto& pair : bpmCounts) {
            if (std::abs(pair.first - candidate.bpm) < 5.0f) { // В пределах 5 BPM
                pair.first = (pair.first * pair.second + candidate.bpm) / (pair.second + 1);
                pair.second++;
                found = true;
                break;
            }
        }
        if (!found) {
            bpmCounts.append({candidate.bpm, 1});
        }
    }

    if (!bpmCounts.isEmpty()) {
        std::sort(bpmCounts.begin(), bpmCounts.end(),
                  [](const QPair<float, int>& a, const QPair<float, int>& b) {
                      return a.second > b.second;
                  });

        float mostFrequentBPM = bpmCounts.first().first;
        qDebug() << "Most frequent BPM:" << mostFrequentBPM << "count:" << bpmCounts.first().second;

        // Корректируем BPM к стандартным значениям
        float correctedBPM = correctToStandardBPM(mostFrequentBPM);
        if (std::abs(correctedBPM - mostFrequentBPM) < 10.0f) {
            qDebug() << "Corrected BPM from" << mostFrequentBPM << "to" << correctedBPM;
            mostFrequentBPM = correctedBPM;
        }

        // Если наиболее частый BPM отличается от лучшего по уверенности, используем его
        if (std::abs(mostFrequentBPM - result.bpm) > 10.0f && bpmCounts.first().second > 1) {
            result.bpm = mostFrequentBPM;
            result.beats = findBeats(samples, result.bpm, sampleRate, options);
            qDebug() << "Using most frequent BPM:" << result.bpm;
        }
    }

    // Анализ регулярности битов: подбор начала сетки по минимальному отклонению долей от сетки
    if (!result.beats.isEmpty() && result.bpm > 0.0f && sampleRate > 0) {
        const float beatInterval = 60.0f * sampleRate / result.bpm;
        const qint64 maxGridStartSamples = qint64(2.0 * sampleRate);
        const int numBeats = result.beats.size();

        auto deviationForGridStart = [&](qint64 gridStart) -> float {
            float sum = 0.0f;
            const int n = qMin(numBeats, 256);
            for (int i = 0; i < n; ++i) {
                float phase = (float(result.beats[i].position) - float(gridStart)) / beatInterval;
                float nearest = std::round(phase);
                sum += std::abs(phase - nearest);
            }
            return (n > 0) ? sum / n : 1.0f;
        };

        qint64 bestStart = 0;
        float bestDev = deviationForGridStart(0);

        for (int phase = 0; phase < qMin(5, numBeats); ++phase) {
            qint64 cand = result.beats[phase].position - qint64(phase * beatInterval);
            if (cand < 0) continue;
            if (cand > maxGridStartSamples) continue;
            float d = deviationForGridStart(cand);
            if (d < bestDev) {
                bestDev = d;
                bestStart = cand;
            }
        }

        result.gridStartSample = bestStart;
    } else if (!result.beats.isEmpty()) {
        qint64 detectedStart = result.beats.first().position;
        const qint64 maxGridStartSamples = qint64(2.0 * sampleRate);
        result.gridStartSample = (detectedStart <= maxGridStartSamples) ? detectedStart : 0;
    }

    if (!result.beats.isEmpty()) {
        float totalDeviation = 0.0f;
        float maxDeviation = 0.0f;

        for (const auto& beat : result.beats) {
            totalDeviation += std::abs(beat.deviation);
            maxDeviation = std::max(maxDeviation, std::abs(beat.deviation));
        }

        result.averageDeviation = totalDeviation / result.beats.size();
        result.hasIrregularBeats = (maxDeviation > options.tolerancePercent / 100.0f);
        result.isFixedTempo = !result.hasIrregularBeats &&
                             (result.averageDeviation < options.tolerancePercent / 200.0f);
    }

    qDebug() << "Final BPM result:" << result.bpm << "confidence:" << result.confidence
             << "irregular beats:" << result.hasIrregularBeats;

    return result;
}

QVector<QPair<int, float>> BPMAnalyzer::detectPeaks(const QVector<float>& samples,
                                                   float minEnergy) {
    QVector<QPair<int, float>> peaks;
    const int windowSize = 1024; // Размер окна анализа
    const int minPeakDistance = 4410; // Минимальное расстояние между пиками (~0.1 сек при 44.1 кГц)

    if (samples.size() < windowSize * 3) {
        return peaks;
    }

    // Скользящее среднее для энергии
    QVector<float> energy(samples.size());
    for (int i = windowSize; i < static_cast<int>(samples.size()) - windowSize; ++i) {
        energy[i] = calculateBeatEnergy(samples, i, windowSize);
    }

    // Поиск локальных максимумов с улучшенной логикой
    for (int i = windowSize; i < static_cast<int>(samples.size()) - windowSize; ++i) {
        if (energy[i] < minEnergy) {
            continue;
        }

        bool isPeak = true;
        // Проверяем окрестность для поиска локального максимума
        for (int j = -windowSize/4; j <= windowSize/4; ++j) {
            if (j != 0 && energy[i + j] >= energy[i]) {
                isPeak = false;
                break;
            }
        }

        if (isPeak) {
            // Проверяем минимальное расстояние от предыдущих пиков
            bool tooClose = false;
            for (const auto& peak : peaks) {
                if (std::abs(i - peak.first) < minPeakDistance) {
                    // Если новый пик сильнее, заменяем старый
                    if (energy[i] > peak.second) {
                        peaks.removeAll(peak);
                        break;
                    } else {
                        tooClose = true;
                        break;
                    }
                }
            }

            if (!tooClose) {
                peaks.append({i, energy[i]});
            }
        }
    }

    // Сортируем пики по времени
    std::sort(peaks.begin(), peaks.end(),
              [](const QPair<int, float>& a, const QPair<int, float>& b) {
                  return a.first < b.first;
              });

    qDebug() << "Detected" << peaks.size() << "peaks with minEnergy=" << minEnergy;
    return peaks;
}

float BPMAnalyzer::calculateAverageInterval(const QVector<QPair<int, float>>& peaks,
                                          bool assumeFixedTempo,
                                          float* confidence) {
    if (peaks.size() < 2) {
        if (confidence) *confidence = 0.0f;
        return 0.0f;
    }

    // Собираем все интервалы между пиками
    QVector<float> intervals;
    for (int i = 1; i < static_cast<int>(peaks.size()); ++i) {
        intervals.append(peaks[i].first - peaks[i-1].first);
    }

    if (assumeFixedTempo) {
        // Используем гистограмму для поиска наиболее частого интервала
        QVector<int> histogram(2000, 0); // Максимальный интервал 2000 сэмплов
        for (float interval : intervals) {
            int bucket = qRound(interval);
            if (bucket >= 0 && bucket < histogram.size()) {
                histogram[bucket]++;
            }
        }

        int maxCount = 0;
        float avgInterval = 0.0f;
        for (int i = 0; i < static_cast<int>(histogram.size()); ++i) {
            if (histogram[i] > maxCount) {
                maxCount = histogram[i];
                avgInterval = i;
            }
        }

        if (confidence) {
            *confidence = normalizeConfidence(float(maxCount) / intervals.size());
        }
        return avgInterval;
    } else {
        // Используем медиану для нефиксированного темпа
        std::sort(intervals.begin(), intervals.end());
        float medianInterval = intervals[intervals.size() / 2];

        if (confidence) {
            float variance = 0.0f;
            for (float interval : intervals) {
                variance += (interval - medianInterval) * (interval - medianInterval);
            }
            variance /= intervals.size();
            *confidence = normalizeConfidence(1.0f / (1.0f + std::sqrt(variance)));
        }
        return medianInterval;
    }
}

float BPMAnalyzer::estimateBPM(float averageInterval,
                              int sampleRate,
                              const AnalysisOptions& options) {
    if (averageInterval <= 0) {
        return 0.0f;
    }

    // Правильная формула: BPM = 60 * sampleRate / interval_in_samples
    float bpm = 60.0f * sampleRate / averageInterval;

    // Нормализация BPM в допустимый диапазон
    while (bpm < options.minBPM && bpm > 0) {
        bpm *= 2.0f;
    }
    while (bpm > options.maxBPM) {
        bpm *= 0.5f;
    }

    qDebug() << "BPM calculation: interval=" << averageInterval << "samples, sampleRate=" << sampleRate << ", BPM=" << bpm;
    return bpm;
}

QVector<BPMAnalyzer::BeatInfo> BPMAnalyzer::findBeats(const QVector<float>& samples,
                                                     float bpm,
                                                     int sampleRate,
                                                     const AnalysisOptions& options) {
    QVector<BeatInfo> beats;
    if (samples.isEmpty() || bpm <= 0 || sampleRate <= 0) {
        return beats;
    }

    // Интервал между битами в сэмплах
    float beatInterval = 60.0f * sampleRate / bpm;

    // Размер окна поиска (+-5% от интервала)
    int searchWindow = qRound(beatInterval * options.tolerancePercent / 100.0f);

    // Поиск первой доли только в первом интервале (0 … 1 beat), чтобы сетка строилась от первой доли, а не от второй
    int firstBeat = -1;
    float maxEnergy = 0.0f;
    int searchLimit = qMin(static_cast<int>(samples.size()), qRound(beatInterval));
    for (int i = 0; i < searchLimit; ++i) {
        float energy = calculateBeatEnergy(samples, i, 1024);
        if (energy > maxEnergy) {
            maxEnergy = energy;
            firstBeat = i;
        }
    }
    if (firstBeat < 0 && !samples.isEmpty()) {
        firstBeat = 0;
    }
    if (firstBeat < 0) {
        return beats;
    }

    // Добавляем биты с учетом отклонений
    float expectedPos = firstBeat;
    while (expectedPos < samples.size()) {
        int actualPos = expectedPos;
        float maxEnergy = calculateBeatEnergy(samples, actualPos, 1024);

        // Ищем локальный максимум энергии
        for (int offset = -searchWindow; offset <= searchWindow; ++offset) {
            int pos = qRound(expectedPos + offset);
            if (pos >= 0 && pos < samples.size()) {
                float energy = calculateBeatEnergy(samples, pos, 1024);
                if (energy > maxEnergy) {
                    maxEnergy = energy;
                    actualPos = pos;
                }
            }
        }

        // Вычисляем отклонение и уверенность
        float deviation = (actualPos - expectedPos) / beatInterval;
        float confidence = normalizeConfidence(maxEnergy);

        BeatInfo beat;
        beat.position = actualPos;
        beat.expectedPosition = expectedPos;
        beat.confidence = confidence;
        beat.deviation = deviation;
        beat.energy = maxEnergy;
        beats.append(beat);

        expectedPos += beatInterval;
    }

    return beats;
}

float BPMAnalyzer::calculateBeatEnergy(const QVector<float>& samples,
                                     int position,
                                     int windowSize) {
    float energy = 0.0f;
    int start = std::max(0, position - windowSize/2);
    int end = std::min<int>(samples.size(), position + windowSize/2);

    // Используем RMS (Root Mean Square) для более точного расчета энергии
    for (int i = start; i < end; ++i) {
        energy += samples[i] * samples[i];
    }

    // Возвращаем RMS энергию
    return std::sqrt(energy / (end - start));
}

bool BPMAnalyzer::isValidBPM(float bpm, const AnalysisOptions& options) {
    return bpm >= options.minBPM && bpm <= options.maxBPM;
}

float BPMAnalyzer::correctToStandardBPM(float bpm) {
    // Стандартные BPM для электронной музыки
    QVector<float> standardBPMs = {
        60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f, 128.0f, 130.0f, 140.0f, 150.0f, 160.0f, 170.0f, 180.0f
    };

    float closestBPM = bpm;
    float minDifference = 1000.0f;

    for (float standardBPM : standardBPMs) {
        float difference = std::abs(bpm - standardBPM);
        if (difference < minDifference) {
            minDifference = difference;
            closestBPM = standardBPM;
        }
    }

    // Корректируем только если разница не слишком большая (в пределах 10 BPM)
    if (minDifference <= 10.0f) {
        return closestBPM;
    }

    return bpm;
}

float BPMAnalyzer::normalizeConfidence(float rawConfidence) {
    // Сигмоидная нормализация для получения значения в диапазоне [0,1]
    return 1.0f / (1.0f + std::exp(-5.0f * (rawConfidence - 0.5f)));
}

// Реализация алгоритма Mixxx
BPMAnalyzer::AnalysisResult BPMAnalyzer::analyzeBPMUsingMixxx(const QVector<float>& samples,
                                                            int sampleRate,
                                                            const AnalysisOptions& options) {
    AnalysisResult result;

    if (samples.isEmpty() || sampleRate <= 0) {
        qDebug() << "Invalid input for Mixxx BPM analysis";
        return result;
    }

    // Обнаружение onset'ов с использованием алгоритма из Mixxx
    int stepSize, windowSize;
    QVector<double> detectionFunction = detectOnsets(samples, sampleRate, stepSize, windowSize);

    if (detectionFunction.isEmpty()) {
        qDebug() << "No onsets detected using Mixxx algorithm";
        return result;
    }

    // Отслеживание битов через алгоритм TempoTrackV2 из Mixxx
    QVector<BeatInfo> beats = trackBeats(detectionFunction, sampleRate, stepSize);

    if (beats.isEmpty()) {
        qDebug() << "No beats detected using Mixxx algorithm";
        return result;
    }

    result.beats = beats;
    qint64 detectedStart = beats.first().position;
    const qint64 maxGridStartSamples = qint64(2.0 * sampleRate);
    result.gridStartSample = (detectedStart <= maxGridStartSamples) ? detectedStart : 0;

    // Вычисляем BPM на основе обнаруженных битов
    if (beats.size() >= 2) {
        QVector<float> intervals;
        intervals.reserve(beats.size() - 1);
        for (int i = 1; i < static_cast<int>(beats.size()); ++i) {
            intervals.append(float(beats[i].position - beats[i-1].position));
        }

        // Базовый BPM из среднего интервала
        const float avgInterval = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
        const float baseBpm = 60.0f * sampleRate / avgInterval;
        // Сохраняем базовый BPM как предварительный (до выбора гармоники)
        result.preliminaryBPM = baseBpm;
        result.hasPreliminaryBPM = true;

        // Подбор гармоники BPM (x0.5, x1, x2, x4), минимизирующей разброс долей
        auto calcDeviationForBpm = [&](float candidateBpm) -> float {
            if (candidateBpm <= 0.0f) return std::numeric_limits<float>::infinity();
            const float beatInterval = 60.0f * sampleRate / candidateBpm; // в сэмплах
            if (beatInterval <= 0.0f) return std::numeric_limits<float>::infinity();

            // Считаем отклонение каждой фактической доли от ближайшей идеальной позиции
            float totalDeviationAbs = 0.0f;
            float maxDeviationAbs = 0.0f;
            const float gridStart = float(result.gridStartSample);
            const int considerCount = std::min<int>(int(beats.size()), 512); // ограничим, чтобы не тратить много времени
            for (int i = 0; i < considerCount; ++i) {
                const float pos = float(beats[i].position);
                const float phase = (pos - gridStart) / beatInterval; // в долях
                const float nearest = std::round(phase);
                const float deviationBeats = std::abs(phase - nearest); // |отклонение| в долях
                totalDeviationAbs += deviationBeats;
                if (deviationBeats > maxDeviationAbs) maxDeviationAbs = deviationBeats;
            }
            // Взвешиваем среднее и максимум, чтобы штрафовать большие скачки
            const float avgDev = totalDeviationAbs / std::max(1, considerCount);
            return avgDev * 0.7f + maxDeviationAbs * 0.3f;
        };

        // Кандидаты BPM (учитывая попадание в допустимый диапазон)
        QVector<float> candidates;
        candidates.reserve(8);
        auto addIfInRange = [&](float v) {
            if (v > 0.0f) {
                float x = v;
                while (x < options.minBPM) x *= 2.0f;
                while (x > options.maxBPM) x *= 0.5f;
                candidates.append(x);
            }
        };
        addIfInRange(baseBpm);
        addIfInRange(baseBpm * 2.0f);
        addIfInRange(baseBpm * 4.0f);
        addIfInRange(baseBpm * 0.5f);
        addIfInRange(baseBpm * 0.25f);

        // Выбираем BPM с минимальным отклонением сетки
        float bestBpm = baseBpm;
        float bestScore = std::numeric_limits<float>::infinity();
        for (float c : candidates) {
            const float score = calcDeviationForBpm(c);
            if (score < bestScore) {
                bestScore = score;
                bestBpm = c;
            }
        }

        result.bpm = bestBpm;

        // Подбор начала сетки по минимальному отклонению (фаза)
        const float beatIntervalForBest = 60.0f * sampleRate / result.bpm;
        const qint64 maxGridStartSamples = qint64(2.0 * sampleRate);
        auto deviationForGridStart = [&](qint64 gridStart) -> float {
            float sum = 0.0f;
            const int n = qMin(int(beats.size()), 256);
            for (int i = 0; i < n; ++i) {
                float phase = (float(beats[i].position) - float(gridStart)) / beatIntervalForBest;
                sum += std::abs(phase - std::round(phase));
            }
            return (n > 0) ? sum / n : 1.0f;
        };
        qint64 bestStart = result.gridStartSample;
        float bestDev = deviationForGridStart(bestStart);
        for (int phase = 0; phase < qMin(5, int(beats.size())); ++phase) {
            qint64 cand = beats[phase].position - qint64(phase * beatIntervalForBest);
            if (cand >= 0 && cand <= maxGridStartSamples) {
                float d = deviationForGridStart(cand);
                if (d < bestDev) { bestDev = d; bestStart = cand; }
            }
        }
        result.gridStartSample = bestStart;

        // Пересчитываем метрики регулярности для выбранного BPM
        float totalDeviation = 0.0f;
        float maxDeviation = 0.0f;
        const int considerCount2 = std::min<int>(int(beats.size()), 512);
        for (int i = 0; i < considerCount2; ++i) {
            const float expBeats = (float(beats[i].position) - float(result.gridStartSample)) / beatIntervalForBest;
            const float dev = std::abs(expBeats - std::round(expBeats)); // отклонение в долях
            totalDeviation += dev;
            if (dev > maxDeviation) maxDeviation = dev;
        }
        result.averageDeviation = totalDeviation / std::max(1, considerCount2);
        result.hasIrregularBeats = (maxDeviation > options.tolerancePercent / 100.0f);
        result.isFixedTempo = !result.hasIrregularBeats && (result.averageDeviation < options.tolerancePercent / 200.0f);
        result.confidence = 1.0f - std::min(1.0f, result.averageDeviation);
    }

    // Нормализуем BPM в допустимый диапазон
    while (result.bpm < options.minBPM && result.bpm > 0) {
        result.bpm *= 2.0f;
    }
    while (result.bpm > options.maxBPM) {
        result.bpm *= 0.5f;
    }

    qDebug() << "Mixxx algorithm detected BPM:" << result.bpm
             << "with" << beats.size() << "beats";

    return result;
}

QVector<double> BPMAnalyzer::detectOnsets(const QVector<float>& samples,
                                         int sampleRate,
                                         int& stepSize,
                                         int& windowSize) {
    QVector<double> detectionResults;

    // Вычисляем параметры окна как в Mixxx
    stepSize = static_cast<int>(sampleRate * kStepSecs);
    windowSize = nextPowerOfTwo(sampleRate / kMaximumBinSizeHz);

    qDebug() << "Mixxx onset detection: sampleRate =" << sampleRate
             << ", stepSize =" << stepSize
             << ", windowSize =" << windowSize;

#ifdef USE_MIXXX_QM_DSP
    // Если есть библиотека qm-dsp, используем её
    DFConfig config;
    config.DFType = DF_COMPLEXSD;
    config.stepSize = stepSize;
    config.frameLength = windowSize;
    config.dbRise = 3;
    config.adaptiveWhitening = false;
    config.whiteningRelaxCoeff = -1;
    config.whiteningFloor = -1;

    DetectionFunction df(config);

    // Обрабатываем сигнал окнами
    QVector<double> window(windowSize);
    for (int i = 0; i < static_cast<int>(samples.size()) - windowSize; i += stepSize) {
        // Копируем окно данных
        for (int j = 0; j < windowSize; ++j) {
            window[j] = samples[i + j];
        }
        // Вычисляем detection function
        double value = df.processTimeDomain(window.data());
        detectionResults.append(value);
    }
#else
    // Упрощённый алгоритм обнаружения onset'ов
    for (int i = 0; i < static_cast<int>(samples.size()) - windowSize; i += stepSize) {
        double energy = 0.0;
        for (int j = 0; j < windowSize; ++j) {
            energy += samples[i + j] * samples[i + j];
        }
        detectionResults.append(std::sqrt(energy / windowSize));
    }

    // Применяем spectral flux для улучшения обнаружения
    for (int i = 1; i < static_cast<int>(detectionResults.size()); ++i) {
        double flux = detectionResults[i] - detectionResults[i-1];
        if (flux < 0) flux = 0;
        detectionResults[i] = flux;
    }
#endif

    return detectionResults;
}

QVector<BPMAnalyzer::BeatInfo> BPMAnalyzer::trackBeats(const QVector<double>& detectionFunction,
                                                      int sampleRate,
                                                      int stepSize) {
    QVector<BeatInfo> beats;

    if (detectionFunction.size() < 3) {
        return beats;
    }

#ifdef USE_MIXXX_QM_DSP
    // Используем TempoTrackV2 из Mixxx
    TempoTrackV2 tt(sampleRate, stepSize);

    // Подготавливаем данные (пропускаем первые 2 значения как в Mixxx)
    std::vector<double> df;
    std::vector<int> beatPeriod;
    df.reserve(detectionFunction.size() > 2 ? detectionFunction.size() - 2 : 0);
    for (int i = 2; i < static_cast<int>(detectionFunction.size()); ++i) {
        df.push_back(detectionFunction[i]);
    }

    // Для очень коротких последовательностей Mixxx-трекер нестабилен
    if (df.size() < 3) {
        return beats;
    }

    // Внутри TempoTrackV2::viterbi_decode выходной путь индексируется по числу
    // внутренних окон и может быть длиннее грубой оценки (df/128).
    // Выделяем буфер с запасом по длине df, чтобы исключить выход за границы.
    beatPeriod.assign(df.size(), 0);

    // Вычисляем период битов
    tt.calculateBeatPeriod(df, beatPeriod);

    stabilizeBeatPeriodTail(beatPeriod);

    // Защита от некорректных значений периода:
    // отрицательные/нулевые/слишком большие значения ломают внутреннюю индексацию.
    if (!sanitizeBeatPeriods(beatPeriod, df.size())) {
        return beats;
    }

    // Вычисляем позиции битов
    std::vector<double> beatPositions;
    tt.calculateBeats(df, beatPeriod, beatPositions);
    beats.reserve(static_cast<int>(beatPositions.size()));

    // Преобразуем в BeatInfo
    for (size_t i = 0; i < beatPositions.size(); ++i) {
        const qint64 position = static_cast<qint64>((beatPositions[i] * stepSize) + stepSize / 2);
        float energy = 0.0f;
        if (!df.empty() && i < df.size()) {
            energy = static_cast<float>(df[i]);
        } else if (!df.empty()) {
            energy = static_cast<float>(df.back());
        }
        beats.append(makeBeatInfo(position, energy, 0.9f));
    }
#else
    // Упрощённый алгоритм обнаружения битов

    // Находим пики в detection function
    QVector<int> peaks;
    double threshold = 0.0;

    // Вычисляем среднее значение для порога
    for (const auto& value : detectionFunction) {
        threshold += value;
    }
    threshold = threshold / detectionFunction.size() * 1.5; // Порог = 1.5 * среднее

    // Находим локальные максимумы
    for (int i = 1; i < static_cast<int>(detectionFunction.size()) - 1; ++i) {
        if (detectionFunction[i] > threshold &&
            detectionFunction[i] > detectionFunction[i-1] &&
            detectionFunction[i] > detectionFunction[i+1]) {
            peaks.append(i);
        }
    }

    // Преобразуем пики в биты
    const auto maxIt = std::max_element(detectionFunction.begin(), detectionFunction.end());
    const double maxDetection = (maxIt != detectionFunction.end()) ? *maxIt : 0.0;
    for (int peakIdx : peaks) {
        const qint64 position = static_cast<qint64>(peakIdx * stepSize + stepSize / 2);
        const float energy = static_cast<float>(detectionFunction[peakIdx]);
        const float confidence = (maxDetection > 0.0)
            ? static_cast<float>(detectionFunction[peakIdx] / maxDetection)
            : 0.0f;
        beats.append(makeBeatInfo(position, energy, confidence));
    }

    // Фильтруем слишком близкие биты (минимальный интервал 100мс)
    const qint64 minInterval = (sampleRate * 100) / 1000; // 100ms в сэмплах
    QVector<BeatInfo> filteredBeats;
    qint64 lastBeatPos = -minInterval;

    for (const auto& beat : beats) {
        if (beat.position - lastBeatPos >= minInterval) {
            filteredBeats.append(beat);
            lastBeatPos = beat.position;
        }
    }

    beats = filteredBeats;
#endif

    // Вычисляем отклонения от среднего интервала
    if (beats.size() > 2) {
        QVector<qint64> intervals;
        for (int i = 1; i < static_cast<int>(beats.size()); ++i) {
            intervals.append(beats[i].position - beats[i-1].position);
        }

        qint64 avgInterval = std::accumulate(intervals.begin(), intervals.end(), qint64(0)) / intervals.size();

        if (avgInterval > 0) {
            for (int i = 1; i < static_cast<int>(beats.size()); ++i) {
                qint64 actualInterval = beats[i].position - beats[i-1].position;
                beats[i].deviation = float(actualInterval - avgInterval) / float(avgInterval);
            }
        }
    }

    qDebug() << "Detected" << beats.size() << "beats using Mixxx algorithm";
    return beats;
}

BPMAnalyzer::AnalysisResult BPMAnalyzer::createBeatGridFromBPM(const QVector<float>& samples,
                                                              int sampleRate,
                                                              float bpm,
                                                              const AnalysisOptions& options) {
    Q_UNUSED(options); // Параметр пока не используется, но может понадобиться в будущем

    AnalysisResult result;
    result.bpm = bpm;
    result.confidence = 1.0f; // Высокая уверенность для предварительно определенного BPM

    if (samples.isEmpty() || bpm <= 0.0f) {
        qDebug() << "Invalid parameters for beat grid creation";
        return result;
    }

    qDebug() << "Creating beat grid from BPM:" << bpm;

    // Вычисляем интервал между битами в сэмплах
    float beatInterval = (60.0f * sampleRate) / bpm;

    // Первая доля — пик только в первом интервале (0 … 1 beat), чтобы сетка шла от первой доли
    int searchLimit = std::min(static_cast<int>(beatInterval), static_cast<int>(samples.size()));
    int firstBeat = 0;
    float maxEnergy = 0.0f;
    for (int i = 0; i < searchLimit; ++i) {
        float energy = std::abs(samples[i]);
        if (energy > maxEnergy) {
            maxEnergy = energy;
            firstBeat = i;
        }
    }

    const qint64 maxGridStartSamples = qint64(2.0 * sampleRate);
    result.gridStartSample = (firstBeat <= maxGridStartSamples) ? qint64(firstBeat) : 0;

    // Создаем сетку битов от определённого начала (или от 0)
    int currentBeat = (result.gridStartSample > 0) ? firstBeat : 0;
    while (currentBeat < samples.size()) {
        BeatInfo beat;
        beat.position = currentBeat;
        beat.expectedPosition = currentBeat; // Совпадает для идеальной сетки
        beat.confidence = 1.0f; // Высокая уверенность для сетки
        beat.deviation = 0.0f;  // Нет отклонения для идеальной сетки
        beat.energy = std::abs(samples[currentBeat]);

        result.beats.append(beat);
        currentBeat += static_cast<int>(beatInterval);
    }

    qDebug() << "Created beat grid with" << result.beats.size() << "beats, starting at sample" << firstBeat;

    return result;
}

// ============================================================================
// ПОИСК НЕРОВНЫХ ДОЛЕЙ
// ============================================================================

namespace {

// Медиана in-place (порядок элементов не сохраняется).
double medianInPlace(std::vector<double>& values)
{
    if (values.empty()) {
        return 0.0;
    }
    const size_t mid = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + mid, values.end());
    const double upper = values[mid];
    if (values.size() % 2 != 0) {
        return upper;
    }
    const double lower = *std::max_element(values.begin(), values.begin() + mid);
    return 0.5 * (lower + upper);
}

} // namespace

void BPMAnalyzer::calculateDeviations(QVector<BeatInfo>& beats, float bpm, int sampleRate)
{
    calculateDeviations(beats, bpm, sampleRate, DeviationOptions());
}

BPMAnalyzer::DeviationStats BPMAnalyzer::calculateDeviations(QVector<BeatInfo>& beats,
                                                            float bpm,
                                                            int sampleRate,
                                                            const DeviationOptions& options)
{
    DeviationStats stats;
    if (beats.isEmpty() || bpm <= 0.0f || sampleRate <= 0) {
        return stats;
    }

    // Всё считаем в double: на 192 kHz позиции длинного трека выходят за диапазон
    // целых, представимых во float точно (2^24), и сетка «уезжает» на десятки сэмплов.
    const double nominalInterval = (60.0 * double(sampleRate)) / double(bpm);
    if (nominalInterval < 1.0) {
        return stats;
    }

    const int beatCount = int(beats.size());
    std::vector<double> gridIndex(size_t(beatCount), 0.0);
    double interval = nominalInterval;
    double origin = (options.gridStartSample >= 0) ? double(options.gridStartSample)
                                                   : double(beats.first().position);

    // Каждая доля сопоставляется ближайшей линии сетки, а не своему порядковому
    // номеру: пропуск или лишнее срабатывание детектора тогда портит одну долю,
    // а не весь хвост трека (раньше отклонение уходило в ±1 интервал до конца).
    auto assignGridIndices = [&]() {
        for (int i = 0; i < beatCount; ++i) {
            gridIndex[size_t(i)] = options.snapToNearestGrid
                ? std::round((double(beats[i].position) - origin) / interval)
                : double(i);
        }
    };

    auto residuals = [&]() {
        std::vector<double> values(size_t(beatCount), 0.0);
        for (int i = 0; i < beatCount; ++i) {
            values[size_t(i)] = double(beats[i].position) - (origin + gridIndex[size_t(i)] * interval);
        }
        return values;
    };

    assignGridIndices();

    // Фаза сетки — медиана остатков, а не позиция первой доли. Иначе сдвинутая
    // первая доля (затакт, шум, ложный onset) объявляет неровным весь трек.
    if (options.gridStartSample < 0) {
        for (int pass = 0; pass < 3; ++pass) {
            std::vector<double> values = residuals();
            const double shift = medianInPlace(values);
            origin += shift;
            assignGridIndices();
            if (std::abs(shift) < 0.5) {  // сошлось до долей сэмпла
                break;
            }
        }
    }

    // Уточнение интервала: если реальный темп чуть отличается от номинального,
    // отклонения растут линейно и «неровным» становится весь конец трека.
    // Регрессия по инлаерам отделяет такой дрейф от настоящего джиттера.
    if (options.refineTempo && beatCount >= 3) {
        std::vector<double> values = residuals();
        std::vector<double> magnitudes(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            magnitudes[i] = std::abs(values[i]);
        }
        // Порог инлаера: не уже четверти интервала, чтобы выборка не выродилась.
        const double inlierLimit = std::max(3.0 * medianInPlace(magnitudes), 0.25 * interval);

        double sumX = 0.0, sumY = 0.0;
        int inliers = 0;
        for (int i = 0; i < beatCount; ++i) {
            const double predicted = origin + gridIndex[size_t(i)] * interval;
            if (std::abs(double(beats[i].position) - predicted) > inlierLimit) {
                continue;
            }
            sumX += gridIndex[size_t(i)];
            sumY += double(beats[i].position);
            ++inliers;
        }

        if (inliers >= 3) {
            const double meanX = sumX / inliers;
            const double meanY = sumY / inliers;
            double covXY = 0.0, varX = 0.0;
            for (int i = 0; i < beatCount; ++i) {
                const double predicted = origin + gridIndex[size_t(i)] * interval;
                if (std::abs(double(beats[i].position) - predicted) > inlierLimit) {
                    continue;
                }
                const double dx = gridIndex[size_t(i)] - meanX;
                covXY += dx * (double(beats[i].position) - meanY);
                varX += dx * dx;
            }

            if (varX > 0.0) {
                const double limit = double(std::max(0.0f, options.maxTempoCorrection));
                const double refined = std::min(std::max(covXY / varX, nominalInterval * (1.0 - limit)),
                                                nominalInterval * (1.0 + limit));
                if (refined > 1.0) {
                    interval = refined;
                    origin = meanY - interval * meanX;
                    assignGridIndices();
                }
            }
        }
    }

    stats.beatCount = beatCount;
    stats.gridStartSample = qint64(std::llround(origin));
    stats.gridBPM = float((60.0 * double(sampleRate)) / interval);

    double sumAbs = 0.0;
    double sumSquares = 0.0;
    std::vector<double> absDeviations(size_t(beatCount), 0.0);

    for (int i = 0; i < beatCount; ++i) {
        const double expected = origin + gridIndex[size_t(i)] * interval;
        const double deviation = (double(beats[i].position) - expected) / interval;

        beats[i].expectedPosition = qint64(std::llround(expected));
        beats[i].deviation = float(deviation);

        const double magnitude = std::abs(deviation);
        absDeviations[size_t(i)] = magnitude;
        sumAbs += magnitude;
        sumSquares += deviation * deviation;
        stats.maxAbsDeviation = std::max(stats.maxAbsDeviation, float(magnitude));

        if (i > 0) {
            const double step = gridIndex[size_t(i)] - gridIndex[size_t(i - 1)];
            if (step > 1.0) {
                stats.gapCount += int(step) - 1;  // детектор пропустил доли
            } else if (step <= 0.0) {
                ++stats.duplicateCount;           // две доли на одной линии сетки
            }
        }
    }

    stats.meanAbsDeviation = float(sumAbs / beatCount);
    stats.rmsDeviation = float(std::sqrt(sumSquares / beatCount));
    stats.medianAbsDeviation = float(medianInPlace(absDeviations));

    return stats;
}

namespace {

// Медианное абсолютное отклонение (MAD) — устойчивая к выбросам мера разброса
double medianAbsoluteDeviation(const QVector<BPMAnalyzer::BeatInfo>& beats)
{
    if (beats.isEmpty()) {
        return 0.0;
    }
    std::vector<double> magnitudes;
    magnitudes.reserve(size_t(beats.size()));
    for (const auto& beat : beats) {
        if (std::isfinite(beat.deviation)) {
            magnitudes.push_back(std::abs(double(beat.deviation)));
        }
    }
    if (magnitudes.empty()) {
        return 0.0;
    }
    return medianInPlace(magnitudes);
}

// Схема области: подряд идущие неровные доли (не дальше regionGap друг от друга)
struct Region {
    int start;  // индекс первой неровной доли
    int end;    // индекс последней (включительно)
    int worst;  // индекс доли с наибольшим |deviation| внутри области
};

QVector<Region> groupIntoRegions(const QVector<int>& unalignedIndices,
                                 const QVector<BPMAnalyzer::BeatInfo>& beats,
                                 int regionGap)
{
    QVector<Region> regions;
    if (unalignedIndices.isEmpty()) {
        return regions;
    }
    Region current;
    current.start = unalignedIndices[0];
    current.end = unalignedIndices[0];
    current.worst = unalignedIndices[0];
    float worstDeviation = std::abs(beats[current.worst].deviation);

    for (int k = 1; k < unalignedIndices.size(); ++k) {
        const int idx = unalignedIndices[k];
        if (idx - current.end <= regionGap) {
            current.end = idx;
            const float magnitude = std::abs(beats[idx].deviation);
            if (magnitude > worstDeviation) {
                current.worst = idx;
                worstDeviation = magnitude;
            }
        } else {
            regions.append(current);
            current.start = idx;
            current.end = idx;
            current.worst = idx;
            worstDeviation = std::abs(beats[idx].deviation);
        }
    }
    regions.append(current);
    return regions;
}

} // namespace

QVector<int> BPMAnalyzer::findUnalignedBeats(const QVector<BeatInfo>& beats,
                                             float deviationThreshold,
                                             float minConfidence,
                                             const UnalignedOptions& options)
{
    QVector<int> unalignedIndices;
    if (beats.isEmpty()) {
        return unalignedIndices;
    }

    // Порог: адаптивный или фиксированный
    float threshold = deviationThreshold;
    if (options.adaptiveThreshold || threshold <= 0.0f) {
        const double mad = medianAbsoluteDeviation(beats);
        threshold = float(std::max(double(options.adaptiveFloor),
                                   double(options.adaptiveMultiplier) * mad));
    } else {
        threshold = std::max(0.0f, threshold);
    }

    unalignedIndices.reserve(beats.size() / 8 + 1);

    for (int i = 0; i < beats.size(); ++i) {
        const BeatInfo& beat = beats[i];

        // Мусорное отклонение (нет сетки / деление на ноль) — не повод для метки.
        if (!std::isfinite(beat.deviation)) {
            continue;
        }
        // Доля, в которой не уверен сам детектор, даёт ложные срабатывания.
        if (beat.confidence < minConfidence) {
            continue;
        }
        if (std::abs(beat.deviation) > threshold) {
            unalignedIndices.append(i);
        }
    }

    // Группировка областей: по одному представителю (наиболее отклонённому) на область
    if (options.groupRegions && !unalignedIndices.isEmpty()) {
        QVector<Region> regions = groupIntoRegions(unalignedIndices, beats, options.regionGap);
        unalignedIndices.clear();
        for (const Region& r : regions) {
            // Средняя уверенность внутри области: если низкая, не уверены в неровности
            double sumConfidence = 0.0;
            int count = 0;
            for (int i = r.start; i <= r.end && i < beats.size(); ++i) {
                if (beats[i].confidence >= 0.0f) {
                    sumConfidence += beats[i].confidence;
                    ++count;
                }
            }
            const float meanConfidence = (count > 0) ? float(sumConfidence / count) : 0.0f;
            if (meanConfidence < options.minRegionConfidence) {
                continue;
            }
            unalignedIndices.append(r.worst);
        }
    }

    return unalignedIndices;
}

BPMAnalyzer::CorrectionSelection BPMAnalyzer::selectBeatsForCorrection(
    const QVector<BeatInfo>& beats,
    float deviationThreshold,
    float minConfidence,
    const UnalignedOptions& options)
{
    CorrectionSelection result;
    result.regions = 0;

    if (beats.isEmpty()) {
        return result;
    }

    // Сначала находим неровные доли (с группировкой, если включена)
    UnalignedOptions groupOpts = options;
    groupOpts.groupRegions = true;  // всегда группируем для приоритетного отбора
    const QVector<int> unalignedIndices = findUnalignedBeats(
        beats, deviationThreshold, minConfidence, groupOpts);

    if (unalignedIndices.isEmpty()) {
        return result;
    }

    // Сортировка по приоритету: |deviation| × confidence × sqrt(energy)
    struct Candidate {
        int index;
        double priority;
    };
    QVector<Candidate> candidates;
    candidates.reserve(unalignedIndices.size());

    for (int idx : unalignedIndices) {
        const BeatInfo& beat = beats[idx];
        const double magnitude = std::abs(double(beat.deviation));
        const double confidence = std::max(0.0, double(beat.confidence));
        const double energy = std::max(0.0, double(beat.energy));
        // sqrt(energy) — чтобы не переоценивать громкие доли
        const double priority = magnitude * confidence * std::sqrt(energy);
        candidates.append(Candidate { idx, priority });
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.priority > b.priority;
              });

    result.indices.reserve(candidates.size());
    for (const Candidate& c : candidates) {
        result.indices.append(c.index);
    }

    // Количество областей — для статистики UI
    if (options.groupRegions) {
        QVector<int> rawUnaligned;
        for (int i = 0; i < beats.size(); ++i) {
            const BeatInfo& beat = beats[i];
            if (!std::isfinite(beat.deviation) || beat.confidence < minConfidence) {
                continue;
            }
            const float threshold = (options.adaptiveThreshold || deviationThreshold <= 0.0f)
                ? float(std::max(double(options.adaptiveFloor),
                                double(options.adaptiveMultiplier) * medianAbsoluteDeviation(beats)))
                : std::max(0.0f, deviationThreshold);
            if (std::abs(beat.deviation) > threshold) {
                rawUnaligned.append(i);
            }
        }
        result.regions = groupIntoRegions(rawUnaligned, beats, options.regionGap).size();
    } else {
        result.regions = result.indices.size();
    }

    return result;
}
