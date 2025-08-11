#include "bpmanalyzer.h"
#include <QtCore/QDebug>
#include <cmath>
#include <algorithm>

BPMAnalyzer::AnalysisResult BPMAnalyzer::analyzeBPM(const QVector<float>& samples, 
                                                    int sampleRate,
                                                    const AnalysisOptions& options) {
    AnalysisResult result;
    result.confidence = 0.0f;
    result.hasIrregularBeats = false;
    result.averageDeviation = 0.0f;
    result.isFixedTempo = true;

    // Обнаружение пиков с минимальным порогом энергии
    auto peaks = detectPeaks(samples, 0.1f);
    if (peaks.isEmpty()) {
        qDebug() << "No peaks detected in audio";
        return result;
    }

    // Вычисление среднего интервала между пиками
    float confidence;
    float avgInterval = calculateAverageInterval(peaks, options.assumeFixedTempo, &confidence);
    result.confidence = confidence;

    // Оценка BPM на основе интервала
    result.bpm = estimateBPM(avgInterval, sampleRate, options);
    if (!isValidBPM(result.bpm, options)) {
        qDebug() << "Invalid BPM detected:" << result.bpm;
        return result;
    }

    // Поиск битов с учетом BPM
    result.beats = findBeats(samples, result.bpm, sampleRate, options);

    // Анализ регулярности битов
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
    
    if (samples.size() < windowSize * 3) {
        return peaks;
    }

    // Скользящее среднее для энергии
    QVector<float> energy(samples.size());
    for (int i = windowSize; i < samples.size() - windowSize; ++i) {
        energy[i] = calculateBeatEnergy(samples, i, windowSize);
    }

    // Поиск локальных максимумов
    for (int i = windowSize; i < samples.size() - windowSize; ++i) {
        if (energy[i] < minEnergy) {
            continue;
        }

        bool isPeak = true;
        for (int j = -windowSize/2; j <= windowSize/2; ++j) {
            if (j != 0 && energy[i + j] >= energy[i]) {
                isPeak = false;
                break;
            }
        }

        if (isPeak) {
            peaks.append({i, energy[i]});
        }
    }

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
    for (int i = 1; i < peaks.size(); ++i) {
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
        for (int i = 0; i < histogram.size(); ++i) {
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

    float bpm = 60.0f * sampleRate / averageInterval;

    // Нормализация BPM в допустимый диапазон
    while (bpm < options.minBPM && bpm > 0) {
        bpm *= 2.0f;
    }
    while (bpm > options.maxBPM) {
        bpm *= 0.5f;
    }

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
    for (int i = 0; i < qMin(samples.size(), qRound(beatInterval * 2)); ++i) {
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
    
    for (int i = start; i < end; ++i) {
        energy += samples[i] * samples[i];
    }
    
    return energy / windowSize;
}

bool BPMAnalyzer::isValidBPM(float bpm, const AnalysisOptions& options) {
    return bpm >= options.minBPM && bpm <= options.maxBPM;
}

float BPMAnalyzer::normalizeConfidence(float rawConfidence) {
    // Сигмоидная нормализация для получения значения в диапазоне [0,1]
    return 1.0f / (1.0f + std::exp(-5.0f * (rawConfidence - 0.5f)));
} 