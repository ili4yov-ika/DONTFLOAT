#include "../include/keyanalyzer.h"
#include <QtCore/QDebug>
#include <QtCore/QSet>
#include <cmath>
#include <algorithm>

// Заглушки для qm-dsp библиотек
#ifdef USE_MIXXX_QM_DSP
#include <dsp/keydetection/GetKeyMode.h>
#include <dsp/chromagram/Chromagram.h>
#endif

namespace {
    // Хроматические профили для мажорных и минорных тональностей
    const QVector<float> MAJOR_PROFILES = {
        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f  // C, D, E, F, G, A, B
    };
    
    const QVector<float> MINOR_PROFILES = {
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f  // C, D, Eb, F, G, Ab, Bb
    };
    
    const QVector<QString> KEY_NAMES = {
        "C Major", "C Minor",
        "C# Major", "C# Minor", 
        "D Major", "D Minor",
        "D# Major", "D# Minor",
        "E Major", "E Minor",
        "F Major", "F Minor",
        "F# Major", "F# Minor",
        "G Major", "G Minor",
        "G# Major", "G# Minor",
        "A Major", "A Minor",
        "A# Major", "A# Minor",
        "B Major", "B Minor",
        "Unknown"
    };
}

KeyAnalyzer::AnalysisResult KeyAnalyzer::analyzeKey(const QVector<float>& samples, 
                                                   int sampleRate,
                                                   const AnalysisOptions& options) {
    // Если доступна qm-dsp библиотека, используем её
    #ifdef USE_MIXXX_QM_DSP
    return analyzeKeyUsingQM(samples, sampleRate, options);
    #else
    // Иначе используем упрощенный алгоритм
    AnalysisResult result;
    result.overallConfidence = 0.0f;
    result.hasKeyChange = false;
    
    if (samples.isEmpty()) {
        qDebug() << "No samples provided for key analysis";
        return result;
    }
    
    // Извлекаем хроматические признаки
    QVector<float> chromaVector = extractChromaFeatures(samples, sampleRate, 
                                                       options.frameSize, options.hopSize);
    
    if (chromaVector.isEmpty()) {
        qDebug() << "Failed to extract chroma features";
        return result;
    }
    
    // Определяем тональность
    result.primaryKey = detectKeyFromChroma(chromaVector);
    result.chromaVector = chromaVector;
    result.overallConfidence = result.primaryKey.confidence;
    
    // Если нужно, определяем смены тональности
    if (options.detectKeyChanges) {
        // Разбиваем на кадры для анализа смены тональности
        QVector<QVector<float>> chromaFrames;
        int frameCount = samples.size() / options.hopSize;
        
        for (int i = 0; i < frameCount; ++i) {
            int start = i * options.hopSize;
            int end = std::min(start + options.frameSize, static_cast<int>(samples.size()));
            
            if (end - start >= options.frameSize) {
                QVector<float> frame(samples.begin() + start, samples.begin() + end);
                QVector<float> frameChroma = extractChromaFeatures(frame, sampleRate, 
                                                                  options.frameSize, options.hopSize);
                if (!frameChroma.isEmpty()) {
                    chromaFrames.append(frameChroma);
                }
            }
        }
        
        result.keyChanges = detectKeyChanges(chromaFrames, options.keyChangeThreshold);
        result.secondaryKey = pickSecondaryKey(chromaFrames, result.primaryKey.key);
        result.hasKeyChange = !result.keyChanges.isEmpty()
            || result.secondaryKey.key != UNKNOWN_KEY;
    }
    
    return result;
    #endif
}

#ifdef USE_MIXXX_QM_DSP
KeyAnalyzer::AnalysisResult KeyAnalyzer::analyzeKeyUsingQM(const QVector<float>& samples, 
                                                          int sampleRate,
                                                          const AnalysisOptions& options) {
    AnalysisResult result;
    result.overallConfidence = 0.0f;
    result.hasKeyChange = false;
    
    try {
        // Конфигурация для GetKeyMode
        GetKeyMode::Config config(sampleRate, options.tuningFrequency);
        config.frameOverlapFactor = 8; // Нормальное перекрытие кадров
        config.decimationFactor = 8;
        
        GetKeyMode keyDetector(config);
        
        // Конвертируем float в double для qm-dsp
        QVector<double> doubleSamples = convertToDouble(samples);
        
        // Обрабатываем аудио по кадрам
        const int frameSize = keyDetector.getBlockSize();
        const int hopSize = keyDetector.getHopSize();
        if (frameSize <= 0 || hopSize <= 0 || doubleSamples.size() < frameSize) {
            qDebug() << "GetKeyMode: invalid frame/hop size or too short audio";
            return result;
        }
        
        QVector<QVector<double>> chromaFrames;
        
        for (int i = 0; i <= doubleSamples.size() - frameSize; i += hopSize) {
            QVector<double> frame(doubleSamples.begin() + i, 
                                 doubleSamples.begin() + i + frameSize);
            
            // Обрабатываем кадр (возвращает индекс тональности)
            Q_UNUSED(keyDetector.process(frame.data())); // Результат пока не используется
            
            // Получаем хроматический вектор для кадра (24 элемента: 12 мажор + 12 минор)
            double* keyStrengths = keyDetector.getKeyStrengths();
            if (keyStrengths) {
                // Создаём вектор из массива (12 элементов для мажорных тональностей)
                QVector<double> chromaVector(12);
                for (int j = 0; j < 12; ++j) {
                    chromaVector[j] = keyStrengths[j]; // Мажорные тональности
                }
                chromaFrames.append(chromaVector);
            }
        }
        
        if (chromaFrames.isEmpty()) {
            qDebug() << "No chroma frames extracted";
            return result;
        }
        
        // Усредняем хроматические векторы
        QVector<double> avgChroma(12, 0.0);
        for (const auto& frame : chromaFrames) {
            for (int i = 0; i < 12; ++i) {
                avgChroma[i] += frame[i];
            }
        }
        
        for (int i = 0; i < 12; ++i) {
            avgChroma[i] /= chromaFrames.size();
        }
        
        // Конвертируем обратно в float
        result.chromaVector = convertToFloat(avgChroma);
        
        // Определяем тональность
        result.primaryKey = detectKeyFromChroma(result.chromaVector);
        result.overallConfidence = result.primaryKey.confidence;
        
        // Анализ смены тональности (модуляции)
        if (options.detectKeyChanges) {
            QVector<QVector<float>> floatChromaFrames;
            for (const auto& frame : chromaFrames) {
                floatChromaFrames.append(convertToFloat(frame));
            }
            result.keyChanges = detectKeyChanges(floatChromaFrames, options.keyChangeThreshold);
            result.secondaryKey = pickSecondaryKey(floatChromaFrames, result.primaryKey.key);
            result.hasKeyChange = !result.keyChanges.isEmpty()
                || result.secondaryKey.key != UNKNOWN_KEY;
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error in qm-dsp key analysis:" << e.what();
    }
    
    return result;
}
#endif

QVector<float> KeyAnalyzer::extractChromaFeatures(const QVector<float>& samples, 
                                                 int sampleRate,
                                                 int frameSize, 
                                                 int hopSize) {
    (void)sampleRate; // Подавляем предупреждение о неиспользуемом параметре
    QVector<float> chromaVector(12, 0.0f);
    
    if (samples.isEmpty()) {
        return chromaVector;
    }
    
    // Простое извлечение хроматических признаков через FFT
    // В реальной реализации здесь должен быть более сложный алгоритм
    
    // Разбиваем на кадры
    QVector<QVector<float>> frames;
    for (int i = 0; i < samples.size() - frameSize; i += hopSize) {
        QVector<float> frame(samples.begin() + i, samples.begin() + i + frameSize);
        frames.append(frame);
    }
    
    if (frames.isEmpty()) {
        return chromaVector;
    }
    
    // Простое усреднение по кадрам (заглушка)
    // В реальности здесь должен быть FFT и группировка по хроматическим классам
    for (int i = 0; i < 12; ++i) {
        float sum = 0.0f;
        for (const auto& frame : frames) {
            // Простая заглушка - используем энергию в разных частотных диапазонах
            int bin = (i * frameSize) / 12;
            if (bin < frame.size()) {
                sum += std::abs(frame[bin]);
            }
        }
        chromaVector[i] = sum / frames.size();
    }
    
    // Нормализация
    float maxVal = *std::max_element(chromaVector.begin(), chromaVector.end());
    if (maxVal > 0.0f) {
        for (int i = 0; i < 12; ++i) {
            chromaVector[i] /= maxVal;
        }
    }
    
    return chromaVector;
}

KeyAnalyzer::KeyInfo KeyAnalyzer::detectKeyFromChroma(const QVector<float>& chromaVector) {
    KeyInfo result;
    result.key = UNKNOWN_KEY;
    result.confidence = 0.0f;
    result.keyName = "Unknown";
    result.strength = 0.0f;
    result.isMajor = false;
    
    if (chromaVector.size() != 12) {
        return result;
    }
    
    float bestCorrelation = 0.0f;
    int bestKey = 0;
    bool bestIsMajor = false;
    
    // Проверяем все 24 тональности (12 мажорных + 12 минорных)
    for (int key = 0; key < 12; ++key) {
        // Мажорная тональность
        float majorCorr = 0.0f;
        for (int i = 0; i < 12; ++i) {
            int shiftedIndex = (i + key) % 12;
            majorCorr += chromaVector[i] * MAJOR_PROFILES[shiftedIndex];
        }
        
        // Минорная тональность
        float minorCorr = 0.0f;
        for (int i = 0; i < 12; ++i) {
            int shiftedIndex = (i + key) % 12;
            minorCorr += chromaVector[i] * MINOR_PROFILES[shiftedIndex];
        }
        
        if (majorCorr > bestCorrelation) {
            bestCorrelation = majorCorr;
            bestKey = key;
            bestIsMajor = true;
        }
        
        if (minorCorr > bestCorrelation) {
            bestCorrelation = minorCorr;
            bestKey = key;
            bestIsMajor = false;
        }
    }
    
    // Устанавливаем результат
    result.key = static_cast<Key>(bestKey * 2 + (bestIsMajor ? 0 : 1));
    result.confidence = std::min(bestCorrelation, 1.0f);
    result.keyName = keyToString(result.key);
    result.strength = bestCorrelation;
    result.isMajor = bestIsMajor;
    
    return result;
}

QVector<KeyAnalyzer::KeyInfo> KeyAnalyzer::detectKeyChanges(const QVector<QVector<float>>& chromaFrames,
                                                           float threshold) {
    QVector<KeyInfo> keyChanges;
    
    if (chromaFrames.size() < 2) {
        return keyChanges;
    }
    
    KeyInfo previousKey = detectKeyFromChroma(chromaFrames[0]);
    
    for (int i = 1; i < chromaFrames.size(); ++i) {
        KeyInfo currentKey = detectKeyFromChroma(chromaFrames[i]);
        
        // Проверяем, изменилась ли тональность
        if (currentKey.key != previousKey.key) {
            float keyChangeStrength = std::abs(currentKey.strength - previousKey.strength);
            
            if (keyChangeStrength > threshold) {
                currentKey.confidence = keyChangeStrength;
                keyChanges.append(currentKey);
            }
        }
        
        previousKey = currentKey;
    }
    
    return keyChanges;
}

KeyAnalyzer::KeyInfo KeyAnalyzer::pickSecondaryKey(const QVector<QVector<float>>& chromaFrames,
                                                   Key primaryKey) {
    KeyInfo secondary; // key = UNKNOWN_KEY по умолчанию

    if (chromaFrames.size() < 4) {
        return secondary;
    }

    // Гистограмма покадровых тональностей
    QVector<int> counts(int(UNKNOWN_KEY), 0);
    QVector<float> strengthSum(int(UNKNOWN_KEY), 0.0f);
    QVector<float> confidenceSum(int(UNKNOWN_KEY), 0.0f);
    int voicedFrames = 0;

    for (const QVector<float>& frame : chromaFrames) {
        const KeyInfo info = detectKeyFromChroma(frame);
        if (info.key == UNKNOWN_KEY) {
            continue;
        }
        const int idx = int(info.key);
        ++counts[idx];
        strengthSum[idx] += info.strength;
        confidenceSum[idx] += info.confidence;
        ++voicedFrames;
    }

    if (voicedFrames < 4) {
        return secondary;
    }

    // Самая частая тональность, отличная от основной
    int bestIdx = -1;
    for (int i = 0; i < counts.size(); ++i) {
        if (i == int(primaryKey)) {
            continue;
        }
        if (bestIdx < 0 || counts[i] > counts[bestIdx]) {
            bestIdx = i;
        }
    }

    // Модуляция считается значимой, если вторая тональность
    // занимает заметную долю трека
    constexpr float kMinShare = 0.2f;
    if (bestIdx < 0 || counts[bestIdx] < 2
        || float(counts[bestIdx]) / float(voicedFrames) < kMinShare) {
        return secondary;
    }

    secondary.key = static_cast<Key>(bestIdx);
    secondary.keyName = keyToString(secondary.key);
    secondary.isMajor = isMajorKey(secondary.key);
    secondary.strength = strengthSum[bestIdx] / counts[bestIdx];
    secondary.confidence = confidenceSum[bestIdx] / counts[bestIdx];
    return secondary;
}

QString KeyAnalyzer::keyToString(Key key) {
    int index = static_cast<int>(key);
    if (index >= 0 && index < KEY_NAMES.size()) {
        return KEY_NAMES[index];
    }
    return "Unknown";
}

KeyAnalyzer::Key KeyAnalyzer::stringToKey(const QString& keyString) {
    for (int i = 0; i < KEY_NAMES.size(); ++i) {
        if (KEY_NAMES[i] == keyString) {
            return static_cast<Key>(i);
        }
    }
    return UNKNOWN_KEY;
}

bool KeyAnalyzer::isMajorKey(Key key) {
    int index = static_cast<int>(key);
    return (index % 2) == 0; // Четные индексы - мажорные тональности
}

double KeyAnalyzer::samplesPerBar(const BarGrid& grid, int sampleRate) {
    if (sampleRate <= 0 || grid.bpm <= 0.0f) {
        return 0.0;
    }
    const double samplesPerBeat = (60.0 * double(sampleRate)) / double(grid.bpm);
    // Длина такта в четвертях: 4/4->4, 3/4->3, 2/4->2, 1/4->1, 6/8->3, 12/8->6
    const double barLengthInQuarters = (grid.beatsPerBar == 6) ? 3.0
                                     : (grid.beatsPerBar == 12) ? 6.0
                                     : double(std::max(1, grid.beatsPerBar));
    return barLengthInQuarters * samplesPerBeat;
}

QVector<float> KeyAnalyzer::computeChromaGoertzel(const QVector<float>& samples, int sampleRate) {
    QVector<float> chroma(12, 0.0f);
    const int n = samples.size();
    if (n < 32 || sampleRate <= 0) {
        return chroma;
    }

    constexpr double kTwoPi = 6.28318530717958647692;
    // Диапазон нот C2 (MIDI 36) .. B6 (MIDI 95) покрывает основные регистры.
    const int midiLow = 36;
    const int midiHigh = 95;
    const double nyquist = 0.5 * double(sampleRate);

    for (int midi = midiLow; midi <= midiHigh; ++midi) {
        const double freq = 440.0 * std::pow(2.0, (double(midi) - 69.0) / 12.0);
        if (freq <= 0.0 || freq >= nyquist) {
            continue;
        }

        // Алгоритм Гёрцеля: величина спектра на частоте freq за один проход.
        const double w = kTwoPi * freq / double(sampleRate);
        const double coeff = 2.0 * std::cos(w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i) {
            const double s0 = double(samples[i]) + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        const double magnitude = std::sqrt(std::max(0.0, power)) / double(n);

        chroma[midi % 12] += float(magnitude);
    }

    float maxVal = 0.0f;
    for (int i = 0; i < 12; ++i) {
        maxVal = std::max(maxVal, chroma[i]);
    }
    if (maxVal > 0.0f) {
        for (int i = 0; i < 12; ++i) {
            chroma[i] /= maxVal;
        }
    }
    return chroma;
}

QVector<KeyAnalyzer::KeyRegion> KeyAnalyzer::mergeBarsIntoRegions(const QVector<BarKey>& bars) {
    QVector<KeyRegion> regions;
    if (bars.isEmpty()) {
        return regions;
    }

    KeyRegion cur;
    cur.startBar = bars[0].barIndex;
    cur.endBar = bars[0].barIndex;
    cur.startSample = bars[0].startSample;
    cur.endSample = bars[0].endSample;
    cur.key = bars[0].key;

    for (int i = 1; i < bars.size(); ++i) {
        const BarKey& b = bars[i];
        const bool sameKey = (b.key.key == cur.key.key);
        const bool contiguous = (b.barIndex == cur.endBar + 1);
        if (sameKey && contiguous) {
            cur.endBar = b.barIndex;
            cur.endSample = b.endSample;
        } else {
            regions.append(cur);
            cur.startBar = b.barIndex;
            cur.endBar = b.barIndex;
            cur.startSample = b.startSample;
            cur.endSample = b.endSample;
            cur.key = b.key;
        }
    }
    regions.append(cur);
    return regions;
}

KeyAnalyzer::KeyInfo KeyAnalyzer::dominantModulationKey(const PerBarKeyResult& perBar,
                                                        Key excludeKey) {
    KeyInfo result; // key = UNKNOWN_KEY по умолчанию

    QVector<int> counts(int(UNKNOWN_KEY) + 1, 0);
    QVector<float> strengthSum(int(UNKNOWN_KEY) + 1, 0.0f);
    QVector<float> confidenceSum(int(UNKNOWN_KEY) + 1, 0.0f);
    for (const BarKey& b : perBar.bars) {
        if (b.key.key == UNKNOWN_KEY || b.key.key == excludeKey) {
            continue;
        }
        const int idx = int(b.key.key);
        if (idx < 0 || idx >= counts.size()) {
            continue;
        }
        ++counts[idx];
        strengthSum[idx] += b.key.strength;
        confidenceSum[idx] += b.key.confidence;
    }

    int bestIdx = -1;
    for (int i = 0; i < int(UNKNOWN_KEY); ++i) {
        if (counts[i] > 0 && (bestIdx < 0 || counts[i] > counts[bestIdx])) {
            bestIdx = i;
        }
    }
    if (bestIdx < 0) {
        return result;
    }

    result.key = static_cast<Key>(bestIdx);
    result.keyName = keyToString(result.key);
    result.isMajor = isMajorKey(result.key);
    result.strength = strengthSum[bestIdx] / float(counts[bestIdx]);
    result.confidence = confidenceSum[bestIdx] / float(counts[bestIdx]);
    return result;
}

KeyAnalyzer::PerBarKeyResult KeyAnalyzer::analyzeKeyPerBar(const QVector<float>& samples,
                                                          int sampleRate,
                                                          const BarGrid& grid,
                                                          const AnalysisOptions& options) {
    Q_UNUSED(options);
    PerBarKeyResult result;

    const qint64 n = samples.size();
    const double spb = samplesPerBar(grid, sampleRate);
    if (n <= 0 || spb < 1.0) {
        return result;
    }

    // Минимальная длина такта для устойчивого анализа (~50 мс).
    const qint64 minBarSamples = qMax<qint64>(32, qint64(sampleRate) / 20);
    const qint64 gridStart = qMax<qint64>(0, grid.gridStartSample);

    for (int barIndex = 0; ; ++barIndex) {
        const qint64 barStart = gridStart + qint64(std::llround(double(barIndex) * spb));
        if (barStart >= n) {
            break;
        }
        const qint64 barEnd = gridStart + qint64(std::llround(double(barIndex + 1) * spb));
        const qint64 sliceEnd = qMin<qint64>(barEnd, n);
        if (sliceEnd - barStart < minBarSamples) {
            // Последний неполный такт слишком короткий — пропускаем.
            break;
        }

        QVector<float> slice(samples.begin() + barStart, samples.begin() + sliceEnd);
        const QVector<float> chroma = computeChromaGoertzel(slice, sampleRate);

        BarKey bk;
        bk.barIndex = barIndex;
        bk.startSample = barStart;
        bk.endSample = barEnd;
        bk.key = detectKeyFromChroma(chroma);
        result.bars.append(bk);
    }

    if (result.bars.isEmpty()) {
        return result;
    }

    // Доминирующая тональность (по числу тактов), UNKNOWN не считаем основной.
    QVector<int> counts(int(UNKNOWN_KEY) + 1, 0);
    for (const BarKey& b : result.bars) {
        counts[int(b.key.key)]++;
    }
    int bestIdx = -1;
    int bestCount = 0;
    for (int i = 0; i < int(UNKNOWN_KEY); ++i) {
        if (counts[i] > bestCount) {
            bestCount = counts[i];
            bestIdx = i;
        }
    }
    if (bestIdx < 0) {
        result.primaryKey.key = UNKNOWN_KEY;
        result.primaryKey.keyName = keyToString(UNKNOWN_KEY);
    } else {
        result.primaryKey.key = static_cast<Key>(bestIdx);
        result.primaryKey.keyName = keyToString(result.primaryKey.key);
        result.primaryKey.isMajor = isMajorKey(result.primaryKey.key);
    }

    result.regions = mergeBarsIntoRegions(result.bars);

    // Модуляция: обнаружено больше одной различной тональности (без учёта UNKNOWN).
    QSet<int> distinct;
    for (const BarKey& b : result.bars) {
        if (b.key.key != UNKNOWN_KEY) {
            distinct.insert(int(b.key.key));
        }
    }
    result.hasModulation = distinct.size() > 1;

    return result;
}

QVector<double> KeyAnalyzer::convertToDouble(const QVector<float>& samples) {
    QVector<double> result;
    result.reserve(samples.size());
    for (float sample : samples) {
        result.append(static_cast<double>(sample));
    }
    return result;
}

QVector<float> KeyAnalyzer::convertToFloat(const QVector<double>& samples) {
    QVector<float> result;
    result.reserve(samples.size());
    for (double sample : samples) {
        result.append(static_cast<float>(sample));
    }
    return result;
}
