#include "waveformanalyzer.h"
#include <QtCore/QDebug>
#include <QtCore/QtMath>
#include <cmath>
#include <algorithm>
#include <numeric>

// Заглушки для Mixxx библиотек
#ifdef USE_MIXXX_QM_DSP
#include <dsp/transforms/FFT.h>
#include <dsp/signalconditioning/Filter.h>
#endif

namespace {
    // Константы для анализа
    constexpr float MIN_AMPLITUDE = 0.0001f;
    constexpr float MAX_AMPLITUDE = 1.0f;
    constexpr int DEFAULT_WINDOW_SIZE = 1024;
    constexpr int DEFAULT_FFT_SIZE = 1024;
    
    // Частотные полосы для анализа (Гц)
    const QVector<QPair<float, float>> FREQUENCY_BANDS = {
        {20, 60},      // Sub-bass
        {60, 250},     // Bass
        {250, 500},    // Low midrange
        {500, 2000},   // Midrange
        {2000, 4000},  // Upper midrange
        {4000, 6000},  // Presence
        {6000, 20000}  // Brilliance
    };
}

WaveformAnalyzer::WaveformData WaveformAnalyzer::analyzeWaveform(const QVector<float>& leftChannel,
                                                                const QVector<float>& rightChannel,
                                                                int sampleRate,
                                                                const AnalysisOptions& options) {
    WaveformData data;
    data.sampleRate = sampleRate;
    
    // Копируем каналы
    data.leftChannel = leftChannel;
    data.rightChannel = rightChannel;
    
    // Создаем моно канал (усреднение стерео)
    int minLength = std::min(leftChannel.size(), rightChannel.size());
    data.monoChannel.reserve(minLength);
    
    for (int i = 0; i < minLength; ++i) {
        float left = (i < leftChannel.size()) ? leftChannel[i] : 0.0f;
        float right = (i < rightChannel.size()) ? rightChannel[i] : 0.0f;
        data.monoChannel.append((left + right) / 2.0f);
    }
    
    // Вычисляем длительность
    data.duration = (data.monoChannel.size() * 1000) / sampleRate;
    
    // Вычисляем максимальную амплитуду
    data.maxAmplitude = 0.0f;
    for (float sample : data.monoChannel) {
        data.maxAmplitude = std::max(data.maxAmplitude, std::abs(sample));
    }
    
    // Вычисляем RMS уровень
    float sumSquares = 0.0f;
    for (float sample : data.monoChannel) {
        sumSquares += sample * sample;
    }
    data.rmsLevel = std::sqrt(sumSquares / data.monoChannel.size());
    
    return data;
}

WaveformAnalyzer::WaveformVisualization WaveformAnalyzer::generateVisualization(const WaveformData& data,
                                                                              const AnalysisOptions& options) {
    WaveformVisualization visualization;
    
    if (data.monoChannel.isEmpty()) {
        return visualization;
    }
    
    // Генерируем пики волновой формы
    visualization.peaks = calculatePeaks(data.monoChannel, options.resolution);
    
    // Вычисляем RMS значения
    QVector<float> rmsValues = calculateRMS(data.monoChannel, DEFAULT_WINDOW_SIZE);
    visualization.rms.reserve(rmsValues.size());
    for (int i = 0; i < rmsValues.size(); ++i) {
        float position = static_cast<float>(i) / rmsValues.size();
        visualization.rms.append({position, rmsValues[i]});
    }
    
    // Генерируем цвета
    if (options.generateColors) {
        visualization.colors = generateColors(data.monoChannel, 
                                            options.colorSaturation, 
                                            options.colorBrightness);
    }
    
    // Анализируем частотные полосы
    if (options.analyzeFrequencyBands) {
        visualization.frequencyBands = calculateFrequencyBands(data.monoChannel, 
                                                             data.sampleRate, 
                                                             options.fftSize);
    }
    
    // Вычисляем спектральные признаки
    if (options.calculateSpectralFeatures) {
        visualization.spectralCentroid = calculateSpectralCentroid(data.monoChannel, 
                                                                 data.sampleRate, 
                                                                 options.fftSize);
        visualization.zeroCrossingRate = calculateZeroCrossingRate(data.monoChannel, 
                                                                  DEFAULT_WINDOW_SIZE);
    }
    
    return visualization;
}

QVector<float> WaveformAnalyzer::calculateRMS(const QVector<float>& samples, int windowSize) {
    QVector<float> rmsValues;
    
    if (samples.isEmpty() || windowSize <= 0) {
        return rmsValues;
    }
    
    int numWindows = samples.size() / windowSize;
    rmsValues.reserve(numWindows);
    
    for (int i = 0; i < numWindows; ++i) {
        int start = i * windowSize;
        int end = std::min(start + windowSize, samples.size());
        
        float sumSquares = 0.0f;
        int count = 0;
        
        for (int j = start; j < end; ++j) {
            sumSquares += samples[j] * samples[j];
            count++;
        }
        
        if (count > 0) {
            rmsValues.append(std::sqrt(sumSquares / count));
        }
    }
    
    return rmsValues;
}

QVector<QPair<float, float>> WaveformAnalyzer::calculatePeaks(const QVector<float>& samples, int resolution) {
    QVector<QPair<float, float>> peaks;
    
    if (samples.isEmpty() || resolution <= 0) {
        return peaks;
    }
    
    // Даунсэмплируем до нужного разрешения
    QVector<float> downsampled = downsample(samples, resolution);
    
    peaks.reserve(downsampled.size());
    
    for (int i = 0; i < downsampled.size(); ++i) {
        float position = static_cast<float>(i) / (downsampled.size() - 1);
        peaks.append({position, downsampled[i]});
    }
    
    return peaks;
}

QVector<QColor> WaveformAnalyzer::generateColors(const QVector<float>& samples, 
                                                float saturation, 
                                                float brightness) {
    QVector<QColor> colors;
    
    if (samples.isEmpty()) {
        return colors;
    }
    
    // Находим максимальную амплитуду для нормализации
    float maxAmplitude = 0.0f;
    for (float sample : samples) {
        maxAmplitude = std::max(maxAmplitude, std::abs(sample));
    }
    
    if (maxAmplitude < MIN_AMPLITUDE) {
        maxAmplitude = MIN_AMPLITUDE;
    }
    
    colors.reserve(samples.size());
    
    for (float sample : samples) {
        float normalizedAmplitude = std::abs(sample) / maxAmplitude;
        QColor color = amplitudeToColor(normalizedAmplitude, maxAmplitude, saturation, brightness);
        colors.append(color);
    }
    
    return colors;
}

QVector<float> WaveformAnalyzer::calculateFrequencyBands(const QVector<float>& samples, 
                                                        int sampleRate, 
                                                        int fftSize) {
    QVector<float> bands(FREQUENCY_BANDS.size(), 0.0f);
    
    if (samples.isEmpty() || sampleRate <= 0 || fftSize <= 0) {
        return bands;
    }
    
    // Вычисляем FFT
    QVector<float> fftResult = calculateFFT(samples, fftSize);
    QVector<float> magnitudeSpectrum = calculateMagnitudeSpectrum(fftResult);
    
    // Группируем по частотным полосам
    for (int i = 0; i < FREQUENCY_BANDS.size(); ++i) {
        float lowFreq = FREQUENCY_BANDS[i].first;
        float highFreq = FREQUENCY_BANDS[i].second;
        
        int lowBin = static_cast<int>((lowFreq * fftSize) / sampleRate);
        int highBin = static_cast<int>((highFreq * fftSize) / sampleRate);
        
        lowBin = std::max(0, std::min(lowBin, magnitudeSpectrum.size() - 1));
        highBin = std::max(0, std::min(highBin, magnitudeSpectrum.size() - 1));
        
        float bandEnergy = 0.0f;
        for (int j = lowBin; j <= highBin; ++j) {
            bandEnergy += magnitudeSpectrum[j];
        }
        
        bands[i] = bandEnergy;
    }
    
    return bands;
}

QVector<float> WaveformAnalyzer::calculateSpectralCentroid(const QVector<float>& samples, 
                                                          int sampleRate, 
                                                          int fftSize) {
    QVector<float> centroids;
    
    if (samples.isEmpty() || sampleRate <= 0 || fftSize <= 0) {
        return centroids;
    }
    
    // Разбиваем на кадры
    int hopSize = fftSize / 2;
    int numFrames = (samples.size() - fftSize) / hopSize + 1;
    centroids.reserve(numFrames);
    
    for (int i = 0; i < numFrames; ++i) {
        int start = i * hopSize;
        int end = std::min(start + fftSize, samples.size());
        
        QVector<float> frame(samples.begin() + start, samples.begin() + end);
        if (frame.size() < fftSize) {
            frame.resize(fftSize, 0.0f);
        }
        
        QVector<float> fftResult = calculateFFT(frame, fftSize);
        QVector<float> magnitudeSpectrum = calculateMagnitudeSpectrum(fftResult);
        
        float weightedSum = 0.0f;
        float magnitudeSum = 0.0f;
        
        for (int j = 0; j < magnitudeSpectrum.size(); ++j) {
            float frequency = (j * sampleRate) / fftSize;
            float magnitude = magnitudeSpectrum[j];
            
            weightedSum += frequency * magnitude;
            magnitudeSum += magnitude;
        }
        
        float centroid = (magnitudeSum > 0.0f) ? (weightedSum / magnitudeSum) : 0.0f;
        centroids.append(centroid);
    }
    
    return centroids;
}

QVector<float> WaveformAnalyzer::calculateZeroCrossingRate(const QVector<float>& samples, 
                                                          int windowSize) {
    QVector<float> zcr;
    
    if (samples.isEmpty() || windowSize <= 0) {
        return zcr;
    }
    
    int numWindows = samples.size() / windowSize;
    zcr.reserve(numWindows);
    
    for (int i = 0; i < numWindows; ++i) {
        int start = i * windowSize;
        int end = std::min(start + windowSize, samples.size());
        
        int crossings = 0;
        for (int j = start + 1; j < end; ++j) {
            if ((samples[j-1] >= 0.0f) != (samples[j] >= 0.0f)) {
                crossings++;
            }
        }
        
        float rate = static_cast<float>(crossings) / (end - start - 1);
        zcr.append(rate);
    }
    
    return zcr;
}

QVector<float> WaveformAnalyzer::downsample(const QVector<float>& samples, int targetLength) {
    QVector<float> result;
    
    if (samples.isEmpty() || targetLength <= 0) {
        return result;
    }
    
    if (samples.size() <= targetLength) {
        return samples;
    }
    
    result.reserve(targetLength);
    float step = static_cast<float>(samples.size()) / targetLength;
    
    for (int i = 0; i < targetLength; ++i) {
        float index = i * step;
        int intIndex = static_cast<int>(index);
        float frac = index - intIndex;
        
        if (intIndex >= 0 && intIndex < samples.size() - 1) {
            float value = samples[intIndex] * (1.0f - frac) + samples[intIndex + 1] * frac;
            result.append(value);
        } else if (intIndex < samples.size()) {
            result.append(samples[intIndex]);
        } else {
            result.append(0.0f);
        }
    }
    
    return result;
}

QVector<float> WaveformAnalyzer::upsample(const QVector<float>& samples, int targetLength) {
    QVector<float> result;
    
    if (samples.isEmpty() || targetLength <= 0) {
        return result;
    }
    
    if (samples.size() >= targetLength) {
        return downsample(samples, targetLength);
    }
    
    result.reserve(targetLength);
    float step = static_cast<float>(samples.size()) / targetLength;
    
    for (int i = 0; i < targetLength; ++i) {
        float index = i * step;
        int intIndex = static_cast<int>(index);
        float frac = index - intIndex;
        
        if (intIndex >= 0 && intIndex < samples.size() - 1) {
            float value = samples[intIndex] * (1.0f - frac) + samples[intIndex + 1] * frac;
            result.append(value);
        } else if (intIndex < samples.size()) {
            result.append(samples[intIndex]);
        } else {
            result.append(0.0f);
        }
    }
    
    return result;
}

QVector<float> WaveformAnalyzer::calculateFFT(const QVector<float>& samples, int fftSize) {
    QVector<float> result(fftSize * 2, 0.0f); // Комплексные числа: real + imag
    
    if (samples.isEmpty() || fftSize <= 0) {
        return result;
    }
    
    // Простая реализация FFT (заглушка)
    // В реальной реализации здесь должен быть эффективный FFT алгоритм
    for (int i = 0; i < std::min(samples.size(), fftSize); ++i) {
        result[i * 2] = samples[i]; // Real part
        result[i * 2 + 1] = 0.0f;   // Imaginary part
    }
    
    // Простое преобразование Фурье (неэффективное, только для демонстрации)
    for (int k = 0; k < fftSize; ++k) {
        float real = 0.0f;
        float imag = 0.0f;
        
        for (int n = 0; n < fftSize; ++n) {
            float angle = -2.0f * M_PI * k * n / fftSize;
            real += samples[n] * std::cos(angle);
            imag += samples[n] * std::sin(angle);
        }
        
        result[k * 2] = real;
        result[k * 2 + 1] = imag;
    }
    
    return result;
}

QVector<float> WaveformAnalyzer::calculateMagnitudeSpectrum(const QVector<float>& fftResult) {
    QVector<float> magnitude;
    
    if (fftResult.size() < 2) {
        return magnitude;
    }
    
    int spectrumSize = fftResult.size() / 2;
    magnitude.reserve(spectrumSize);
    
    for (int i = 0; i < spectrumSize; ++i) {
        float real = fftResult[i * 2];
        float imag = fftResult[i * 2 + 1];
        float mag = std::sqrt(real * real + imag * imag);
        magnitude.append(mag);
    }
    
    return magnitude;
}

QColor WaveformAnalyzer::hsvToRgb(float h, float s, float v) {
    int h_i = static_cast<int>(h * 6);
    float f = h * 6 - h_i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    
    float r, g, b;
    
    switch (h_i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: r = g = b = 0; break;
    }
    
    return QColor(static_cast<int>(r * 255), 
                  static_cast<int>(g * 255), 
                  static_cast<int>(b * 255));
}

QColor WaveformAnalyzer::amplitudeToColor(float amplitude, float maxAmplitude, 
                                         float saturation, float brightness) {
    // Нормализуем амплитуду
    float normalizedAmplitude = std::min(amplitude / maxAmplitude, 1.0f);
    
    // Преобразуем амплитуду в оттенок (0 = красный, 0.33 = зеленый, 0.66 = синий)
    float hue = (1.0f - normalizedAmplitude) * 0.66f; // От синего к красному
    
    return hsvToRgb(hue, saturation, brightness);
}
