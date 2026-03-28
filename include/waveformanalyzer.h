#ifndef WAVEFORMANALYZER_H
#define WAVEFORMANALYZER_H

#include <QVector>
#include <QPair>
#include <QColor>
#include <memory>

// Forward declarations for Mixxx integration
class Waveform;
class WaveformFactory;

class WaveformAnalyzer
{
public:
    struct WaveformData {
        QVector<float> leftChannel;    // Левый канал
        QVector<float> rightChannel;   // Правый канал
        QVector<float> monoChannel;    // Моно канал (усредненный)
        int sampleRate;                // Частота дискретизации
        qint64 duration;               // Длительность в миллисекундах
        float maxAmplitude;            // Максимальная амплитуда
        float rmsLevel;                // RMS уровень
    };

    struct WaveformVisualization {
        QVector<QPair<float, float>> peaks;      // Пики волновой формы
        QVector<QPair<float, float>> rms;        // RMS значения
        QVector<QColor> colors;                  // Цвета для визуализации
        QVector<float> frequencyBands;           // Частотные полосы
        QVector<float> spectralCentroid;         // Спектральный центроид
        QVector<float> zeroCrossingRate;         // Частота пересечения нуля
    };

    struct AnalysisOptions {
        int resolution;                 // Разрешение волновой формы (пикселей)
        bool generateColors;           // Генерировать ли цвета
        bool analyzeFrequencyBands;    // Анализировать ли частотные полосы
        bool calculateSpectralFeatures; // Вычислять ли спектральные признаки
        int fftSize;                   // Размер FFT
        float colorSaturation;         // Насыщенность цветов (0-1)
        float colorBrightness;         // Яркость цветов (0-1)

        AnalysisOptions() 
            : resolution(1024)
            , generateColors(true)
            , analyzeFrequencyBands(true)
            , calculateSpectralFeatures(true)
            , fftSize(1024)
            , colorSaturation(0.8f)
            , colorBrightness(0.7f)
        {}
    };

    static WaveformData analyzeWaveform(const QVector<float>& leftChannel,
                                       const QVector<float>& rightChannel,
                                       int sampleRate,
                                       const AnalysisOptions& options = AnalysisOptions());
    
    static WaveformVisualization generateVisualization(const WaveformData& data,
                                                      const AnalysisOptions& options = AnalysisOptions());
    
    // Методы интеграции с Mixxx
    static WaveformData analyzeWaveformUsingMixxx(const QVector<float>& leftChannel,
                                                 const QVector<float>& rightChannel,
                                                 int sampleRate,
                                                 const AnalysisOptions& options);

    // Вспомогательные методы
    static QVector<float> calculateRMS(const QVector<float>& samples, int windowSize);
    static QVector<QPair<float, float>> calculatePeaks(const QVector<float>& samples, int resolution);
    static QVector<QColor> generateColors(const QVector<float>& samples, 
                                         float saturation, 
                                         float brightness);
    static QVector<float> calculateFrequencyBands(const QVector<float>& samples, 
                                                 int sampleRate, 
                                                 int fftSize);
    static QVector<float> calculateSpectralCentroid(const QVector<float>& samples, 
                                                   int sampleRate, 
                                                   int fftSize);
    static QVector<float> calculateZeroCrossingRate(const QVector<float>& samples, 
                                                   int windowSize);

private:
    // Методы для работы с Mixxx
    static QVector<float> downsample(const QVector<float>& samples, int targetLength);
    static QVector<float> upsample(const QVector<float>& samples, int targetLength);
    
    // Методы анализа
    static QVector<float> calculateFFT(const QVector<float>& samples, int fftSize);
    static QVector<float> calculateMagnitudeSpectrum(const QVector<float>& fftResult);
    static QVector<float> groupIntoBands(const QVector<float>& spectrum, 
                                        int sampleRate, 
                                        int numBands);
    
    // Методы визуализации
    static QColor hsvToRgb(float h, float s, float v);
    static QColor amplitudeToColor(float amplitude, float maxAmplitude, 
                                  float saturation, float brightness);
};

#endif // WAVEFORMANALYZER_H
