#include "../include/bpmanalyzer.h"
#include <QtCore/QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>

// Заглушки для Mixxx библиотек 
// В реальной интеграции нужно подключить qm-dsp
#ifdef USE_MIXXX_QM_DSP
#include <dsp/onsets/DetectionFunction.h>
#include <dsp/tempotracking/TempoTrackV2.h>
#endif

// Константы из Mixxx
namespace {
    constexpr float kStepSecs = 0.01161f; // ~12ms разрешение для BeatMap
    constexpr int kMaximumBinSizeHz = 50; // Hz
    
    // Вспомогательная функция для вычисления следующей степени двойки
    int nextPowerOfTwo(int value) {
        int result = 1;
        while (result < value) {
            result *= 2;
        }
        return result;
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
    
    // Улучшенный алгоритм анализа BPM
    AnalysisResult result;
    result.confidence = 0.0f;
    result.hasIrregularBeats = false;
    result.averageDeviation = 0.0f;
    result.isFixedTempo = true;
    result.gridStartSample = 0;
    result.preliminaryBPM = 0.0f;
    result.hasPreliminaryBPM = false;

    if (samples.isEmpty()) {
        qDebug() << "No samples provided for BPM analysis";
        return result;
    }

    qDebug() << "Starting BPM analysis with" << samples.size() << "samples at" << sampleRate << "Hz";

    // Множественный анализ с разными параметрами
    QVector<AnalysisResult> candidates;
    
    // Анализ 1: Обнаружение пиков с разными порогами
    for (float threshold = 0.05f; threshold <= 0.3f; threshold += 0.05f) {
        auto peaks = detectPeaks(samples, threshold);
        if (peaks.size() < 10) continue; // Недостаточно пиков
        
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

    // Анализ регулярности битов
    if (!result.beats.isEmpty()) {
        result.gridStartSample = result.beats.first().position;
        
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

QVector<float> BPMAnalyzer::fixBeats(const QVector<float>& samples, 
                                    const AnalysisResult& analysis) {
    if (samples.isEmpty() || analysis.beats.isEmpty()) {
        return samples;
    }

    QVector<float> result = samples;
    const int sampleCount = samples.size();
    
    // Размер окна для коррекции (10мс)
    const int windowSize = 441; // при 44100 Hz

    // Применяем коррекцию для каждого бита
    for (const auto& beat : analysis.beats) {
        int pos = beat.position;
        if (pos < 0 || pos >= sampleCount) {
            continue;
        }

        // Находим локальный максимум в окне
        int maxPos = pos;
        float maxVal = std::abs(samples[pos]);
        
        int start = std::max(0, pos - windowSize/2);
        int end = std::min<int>(sampleCount, pos + windowSize/2);
        
        for (int i = start; i < end; ++i) {
            float val = std::abs(samples[i]);
            if (val > maxVal) {
                maxVal = val;
                maxPos = i;
            }
        }

        // Усиливаем бит в позиции максимума
        if (maxPos != pos) {
            // Копируем форму волны из максимума в позицию бита
            for (int i = -windowSize/4; i <= windowSize/4; ++i) {
                int srcPos = maxPos + i;
                int destPos = pos + i;
                if (srcPos >= 0 && srcPos < sampleCount && 
                    destPos >= 0 && destPos < sampleCount) {
                    result[destPos] = samples[srcPos];
                }
            }
        }

        // Усиливаем амплитуду бита
        float amplification = 1.2f + 0.3f * beat.confidence;
        for (int i = -windowSize/4; i <= windowSize/4; ++i) {
            int p = pos + i;
            if (p >= 0 && p < sampleCount) {
                result[p] *= amplification;
            }
        }
    }

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

    // Поиск первого бита
    int firstBeat = -1;
    float maxEnergy = 0.0f;
    for (int i = 0; i < qMin(static_cast<int>(samples.size()), qRound(beatInterval * 2)); ++i) {
        float energy = calculateBeatEnergy(samples, i, 1024);
        if (energy > maxEnergy) {
            maxEnergy = energy;
            firstBeat = i;
        }
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
    result.confidence = 0.0f;
    result.hasIrregularBeats = false;
    result.averageDeviation = 0.0f;
    result.isFixedTempo = true;
    result.gridStartSample = 0;
    result.preliminaryBPM = 0.0f;
    result.hasPreliminaryBPM = false;

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
    result.gridStartSample = beats.first().position;

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

        // Пересчитываем метрики регулярности для выбранного BPM
        const float beatIntervalForBest = 60.0f * sampleRate / result.bpm;
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
    std::vector<double> beatPeriod;
    for (int i = 2; i < static_cast<int>(detectionFunction.size()); ++i) {
        df.push_back(detectionFunction[i]);
        beatPeriod.push_back(0.0);
    }
    
    // Вычисляем период битов
    tt.calculateBeatPeriod(df, beatPeriod);
    
    // Вычисляем позиции битов
    std::vector<double> beatPositions;
    tt.calculateBeats(df, beatPeriod, beatPositions);
    
    // Преобразуем в BeatInfo
    for (size_t i = 0; i < beatPositions.size(); ++i) {
        BeatInfo beat;
        beat.position = static_cast<qint64>(
            (beatPositions[i] * stepSize) + stepSize / 2
        );
        beat.confidence = 0.9f; // Mixxx обычно даёт хорошие результаты
        beat.deviation = 0.0f; // Будет вычислено позже
        // Исправляем проблему с типами: приводим оба аргумента к одному типу
        // Используем индекс из df, ограничивая его размером массива
        if (!df.empty() && i < df.size()) {
            beat.energy = df[i];
        } else if (!df.empty()) {
            beat.energy = df[df.size() - 1]; // Используем последний элемент
        } else {
            beat.energy = 0.0;
        }
        beats.append(beat);
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
    for (int peakIdx : peaks) {
        BeatInfo beat;
        beat.position = static_cast<qint64>(peakIdx * stepSize + stepSize / 2);
        beat.confidence = static_cast<float>(
            detectionFunction[peakIdx] / (*std::max_element(detectionFunction.begin(), detectionFunction.end()))
        );
        beat.deviation = 0.0f;
        beat.energy = static_cast<float>(detectionFunction[peakIdx]);
        beats.append(beat);
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
    result.hasIrregularBeats = false;
    result.averageDeviation = 0.0f;
    result.isFixedTempo = true;
    result.gridStartSample = 0;

    if (samples.isEmpty() || bpm <= 0.0f) {
        qDebug() << "Invalid parameters for beat grid creation";
        return result;
    }

    qDebug() << "Creating beat grid from BPM:" << bpm;

    // Вычисляем интервал между битами в сэмплах
    float beatInterval = (60.0f * sampleRate) / bpm;
    
    // Находим первый бит (ищем пик в начале трека)
    int searchWindow = static_cast<int>(beatInterval * 2);
    int firstBeat = 0;
    float maxEnergy = 0.0f;
    
    for (int i = 0; i < std::min(searchWindow, static_cast<int>(samples.size())); ++i) {
        float energy = std::abs(samples[i]);
        if (energy > maxEnergy) {
            maxEnergy = energy;
            firstBeat = i;
        }
    }
    
    result.gridStartSample = firstBeat;
    
    // Создаем сетку битов
    int currentBeat = firstBeat;
    while (currentBeat < samples.size()) {
        BeatInfo beat;
        beat.position = currentBeat;
        beat.confidence = 1.0f; // Высокая уверенность для сетки
        beat.deviation = 0.0f;  // Нет отклонения для идеальной сетки
        beat.energy = std::abs(samples[currentBeat]);
        
        result.beats.append(beat);
        currentBeat += static_cast<int>(beatInterval);
    }

    qDebug() << "Created beat grid with" << result.beats.size() << "beats, starting at sample" << firstBeat;

    return result;
}

QVector<float> BPMAnalyzer::alignToBeatGrid(const QVector<float>& samples,
                                           int sampleRate,
                                           float bpm,
                                           qint64 gridStartSample) {
    if (samples.isEmpty() || bpm <= 0.0f) {
        return samples;
    }

    QVector<float> result = samples;
    
    // Вычисляем интервал между битами
    float beatInterval = (60.0f * sampleRate) / bpm;
    
    // Создаем временную шкалу для выравнивания
    QVector<float> alignedSamples(samples.size(), 0.0f);
    
    // Находим ближайшие биты для каждого сэмпла
    for (int i = 0; i < static_cast<int>(samples.size()); ++i) {
        // Вычисляем позицию в битах относительно начала сетки
        float beatPosition = (i - gridStartSample) / beatInterval;
        
        // Округляем до ближайшего бита
        int nearestBeat = static_cast<int>(std::round(beatPosition));
        
        // Вычисляем новую позицию
        int newPosition = gridStartSample + static_cast<int>(nearestBeat * beatInterval);
        
        // Если новая позиция в пределах массива, копируем сэмпл
        if (newPosition >= 0 && newPosition < samples.size()) {
            alignedSamples[newPosition] += samples[i];
        }
    }

    qDebug() << "Aligned samples to beat grid with BPM:" << bpm << "starting at:" << gridStartSample;

    return alignedSamples;
} 