#ifndef BPMANALYZER_H
#define BPMANALYZER_H

#include <QtCore/QVector>
#include <QtCore/QPair>

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
    };

    struct AnalysisOptions {
        bool assumeFixedTempo;     // Предполагать ли фиксированный темп
        bool fastAnalysis;        // Использовать ли быстрый анализ
        float minBPM;            // Минимальный допустимый BPM
        float maxBPM;           // Максимальный допустимый BPM
        float tolerancePercent;    // Допустимое отклонение в процентах

        AnalysisOptions() 
            : assumeFixedTempo(true)
            , fastAnalysis(false)
            , minBPM(60.0f)
            , maxBPM(200.0f)
            , tolerancePercent(5.0f)
        {}
    };

    static AnalysisResult analyzeBPM(const QVector<float>& samples, 
                                   int sampleRate,
                                   const AnalysisOptions& options = AnalysisOptions());
    
    static QVector<float> fixBeats(const QVector<float>& samples, 
                                 const AnalysisResult& analysis);

    // Новый режим анализа: использовать уже известный BPM и посчитать неровности/доли
    static AnalysisResult analyzeBeatsWithGivenBPM(const QVector<float>& samples,
                                                  int sampleRate,
                                                  float bpm,
                                                  const AnalysisOptions& options = AnalysisOptions());

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
};

#endif // BPMANALYZER_H 