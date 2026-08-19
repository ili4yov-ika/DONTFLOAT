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
        qint64 position = 0;          // Позиция в сэмплах
        qint64 expectedPosition = 0;  // Ожидаемая позиция (для вычисления коррекции)
        float confidence = 0.0f;      // Уверенность определения (0-1)
        float deviation = 0.0f;       // Отклонение от идеальной позиции в долях
        float energy = 0.0f;          // Энергия бита (для определения акцентов)
    };

    struct AnalysisResult {
        float bpm = 0.0f;
        float confidence = 0.0f;    // Общая уверенность в определении BPM (0-1)
        QVector<BeatInfo> beats;
        bool hasIrregularBeats = false;
        float averageDeviation = 0.0f;
        bool isFixedTempo = true;      // Определяет, имеет ли трек фиксированный темп
        qint64 gridStartSample = 0;    // Опорная позиция сетки (первая доля)
        float preliminaryBPM = 0.0f;   // Предварительный BPM (например, базовый из Mixxx до гармоник)
        bool hasPreliminaryBPM = false; // Признак наличия предварительного BPM
    };

    // Настройки поиска неровных долей (см. calculateDeviations)
    struct DeviationOptions {
        // Опорная линия сетки в сэмплах. Отрицательное значение — оценить по самим долям.
        static constexpr qint64 kAutoGridStart = -1;

        qint64 gridStartSample;   // Начало сетки (kAutoGridStart = определить автоматически)
        bool snapToNearestGrid;   // Сопоставлять долю ближайшей линии сетки, а не порядковому номеру
        bool refineTempo;         // Уточнять интервал сетки регрессией (гасит дрейф темпа)
        float maxTempoCorrection; // Предел уточнения интервала (доля от номинального, 0.05 = ±5%)

        DeviationOptions()
            : gridStartSample(kAutoGridStart)
            , snapToNearestGrid(true)
            , refineTempo(false)
            , maxTempoCorrection(0.05f)
        {}
    };

    // Статистика отклонений долей от сетки (результат calculateDeviations)
    struct DeviationStats {
        int beatCount;              // Сколько долей обработано
        int gapCount;               // Пропущенные линии сетки (доли, которые детектор не нашёл)
        int duplicateCount;         // Доли, попавшие на одну линию сетки (лишние срабатывания)
        float meanAbsDeviation;     // Среднее |отклонение| в долях интервала
        float medianAbsDeviation;   // Медианное |отклонение| (устойчиво к выбросам)
        float maxAbsDeviation;      // Максимальное |отклонение|
        float rmsDeviation;         // Среднеквадратичное отклонение (джиттер)
        float gridBPM;              // BPM сетки, по которой считались отклонения
        qint64 gridStartSample;     // Опорная линия использованной сетки

        DeviationStats()
            : beatCount(0)
            , gapCount(0)
            , duplicateCount(0)
            , meanAbsDeviation(0.0f)
            , medianAbsDeviation(0.0f)
            , maxAbsDeviation(0.0f)
            , rmsDeviation(0.0f)
            , gridBPM(0.0f)
            , gridStartSample(0)
        {}
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

    // Вспомогательные функции
    static float correctToStandardBPM(float bpm);

    // Поиск неровных долей: раскладка долей по сетке BPM и отбор выбивающихся.
    // Заполняет BeatInfo::expectedPosition и BeatInfo::deviation (в долях интервала).
    static void calculateDeviations(QVector<BeatInfo>& beats, float bpm, int sampleRate);
    static DeviationStats calculateDeviations(QVector<BeatInfo>& beats,
                                              float bpm,
                                              int sampleRate,
                                              const DeviationOptions& options);

    // Индексы долей с |deviation| строго больше порога. Доли с confidence ниже
    // minConfidence и с нечисловым deviation пропускаются.
    static QVector<int> findUnalignedBeats(const QVector<BeatInfo>& beats,
                                           float deviationThreshold = 0.02f,
                                           float minConfidence = 0.0f);

    // Методы для работы с предварительно определенным BPM
    static AnalysisResult createBeatGridFromBPM(const QVector<float>& samples,
                                              int sampleRate,
                                              float bpm,
                                              const AnalysisOptions& options = AnalysisOptions());

    // Выравнивание долей по сетке живёт в TimeStretchProcessor::alignBeatsToGrid:
    // это растяжение участков между долями, для него нужны и метки, и Rubber Band,
    // а BPMAnalyzer намеренно остаётся только анализом.

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
