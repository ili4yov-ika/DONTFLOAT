#include "keyanalyzer.h"
#include <QtCore/QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>

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
            int end = std::min(start + options.frameSize, samples.size());
            
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
        result.hasKeyChange = !result.keyChanges.isEmpty();
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
        int frameSize = keyDetector.getBlockSize();
        int hopSize = keyDetector.getHopSize();
        
        QVector<QVector<double>> chromaFrames;
        
        for (int i = 0; i < doubleSamples.size() - frameSize; i += hopSize) {
            QVector<double> frame(doubleSamples.begin() + i, 
                                 doubleSamples.begin() + i + frameSize);
            
            // Получаем хроматический вектор для кадра
            double* chromaData = keyDetector.process(frame.data());
            if (chromaData) {
                QVector<double> chromaVector(chromaData, 12); // 12 полутонов
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
        
        // Анализ смены тональности
        if (options.detectKeyChanges) {
            QVector<QVector<float>> floatChromaFrames;
            for (const auto& frame : chromaFrames) {
                floatChromaFrames.append(convertToFloat(frame));
            }
            result.keyChanges = detectKeyChanges(floatChromaFrames, options.keyChangeThreshold);
            result.hasKeyChange = !result.keyChanges.isEmpty();
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
