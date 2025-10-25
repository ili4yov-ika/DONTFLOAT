#ifndef BPMANALYZER_H
#define BPMANALYZER_H

#include <QtCore/QVector>
#include <QtCore/QPair>
#include <memory>
#include <vector>

// Forward declarations for Mixxx integration
class DetectionFunction;
class TempoTrackV2;

class BPMAnalyzer
{
public:
    struct BeatInfo {
        qint64 position;     // Позиция в сэмплах
        float confidence;    // Уверенность определения (0-1)
        float deviation;     // Отклонение от идеальной позиции в долях
        float energy;       // Энергия бита (для определения акцентов)
    };

    struct AnalysisResult {
        float bpm;
        float confidence;    // Общая уверенность в определении BPM (0-1)
        QVector<BeatInfo> beats;
        bool hasIrregularBeats;
        float averageDeviation;
        bool isFixedTempo;  // Определяет, имеет ли трек фиксированный темп
        qint64 gridStartSample; // Опорная позиция сетки (первая доля)
        float preliminaryBPM;   // Предварительный BPM (например, базовый из Mixxx до гармоник)
        bool hasPreliminaryBPM; // Признак наличия предварительного BPM
    };

    struct AnalysisOptions {
        bool assumeFixedTempo;     // Предполагать ли фиксированный темп
        bool fastAnalysis;        // Использовать ли быстрый анализ
        float minBPM;            // Минимальный допустимый BPM
        float maxBPM;           // Максимальный допустимый BPM
        float tolerancePercent;    // Допустимое отклонение в процентах
        bool useMixxxAlgorithm;   // Использовать ли алгоритм от Mixxx
        float initialBPM;        // Предварительно определенный BPM (0 = автоопределение)
        bool useInitialBPM;      // Использовать ли предварительно определенный BPM
        float fileBPM;           // BPM из метаданных файла
        bool trustFileBPM;       // Доверять ли BPM из метаданных файла

        AnalysisOptions() 
            : assumeFixedTempo(true)
            , fastAnalysis(false)
            , minBPM(60.0f)
            , maxBPM(200.0f)
            , tolerancePercent(5.0f)
            , useMixxxAlgorithm(true)  // По умолчанию используем Mixxx
            , initialBPM(0.0f)
            , useInitialBPM(false)
            , fileBPM(0.0f)
            , trustFileBPM(false)
        {}
    };

    static AnalysisResult analyzeBPM(const QVector<float>& samples, 
                                   int sampleRate,
                                   const AnalysisOptions& options = AnalysisOptions());
    
    static QVector<float> fixBeats(const QVector<float>& samples, 
                                 const AnalysisResult& analysis);
    
    // Вспомогательные функции
    static float correctToStandardBPM(float bpm);
    
    // Методы для работы с предварительно определенным BPM
    static AnalysisResult createBeatGridFromBPM(const QVector<float>& samples,
                                              int sampleRate,
                                              float bpm,
                                              const AnalysisOptions& options = AnalysisOptions());
    
    static QVector<float> alignToBeatGrid(const QVector<float>& samples,
                                        int sampleRate,
                                        float bpm,
                                        qint64 gridStartSample = 0);
    
    // Методы интеграции с Mixxx
    static AnalysisResult analyzeBPMUsingMixxx(const QVector<float>& samples, 
                                              int sampleRate,
                                              const AnalysisOptions& options);

private:
    // Улучшенные методы анализа
    static QVector<QPair<int, float>> detectPeaks(const QVector<float>& samples,
                                                float minEnergy = 0.1f);
    
    static float calculateAverageInterval(const QVector<QPair<int, float>>& peaks,
                                       bool assumeFixedTempo,
                                       float* confidence = nullptr);
    
    static float estimateBPM(float averageInterval, 
                           int sampleRate,
                           const AnalysisOptions& options);
    
    static QVector<BeatInfo> findBeats(const QVector<float>& samples,
                                    float bpm,
                                    int sampleRate,
                                    const AnalysisOptions& options);

    // Новые вспомогательные методы
    static float calculateBeatEnergy(const QVector<float>& samples,
                                  int position,
                                  int windowSize);
    
    static bool isValidBPM(float bpm, const AnalysisOptions& options);
    
    static float normalizeConfidence(float rawConfidence);
    
    // Вспомогательные методы для Mixxx интеграции
    static QVector<double> detectOnsets(const QVector<float>& samples, 
                                       int sampleRate,
                                       int& stepSize,
                                       int& windowSize);
    static QVector<BeatInfo> trackBeats(const QVector<double>& detectionFunction, 
                                       int sampleRate,
                                       int stepSize);
};

#endif // BPMANALYZER_H 