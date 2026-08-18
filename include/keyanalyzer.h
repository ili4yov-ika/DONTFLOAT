#ifndef KEYANALYZER_H
#define KEYANALYZER_H

#include <QVector>
#include <QString>
#include <memory>

// Forward declarations for qm-dsp integration
class GetKeyMode;
class Chromagram;

class KeyAnalyzer
{
public:
    enum Key {
        C_MAJOR, C_MINOR,
        C_SHARP_MAJOR, C_SHARP_MINOR,
        D_MAJOR, D_MINOR,
        D_SHARP_MAJOR, D_SHARP_MINOR,
        E_MAJOR, E_MINOR,
        F_MAJOR, F_MINOR,
        F_SHARP_MAJOR, F_SHARP_MINOR,
        G_MAJOR, G_MINOR,
        G_SHARP_MAJOR, G_SHARP_MINOR,
        A_MAJOR, A_MINOR,
        A_SHARP_MAJOR, A_SHARP_MINOR,
        B_MAJOR, B_MINOR,
        UNKNOWN_KEY
    };

    struct KeyInfo {
        Key key = UNKNOWN_KEY;
        float confidence = 0.0f;  // Уверенность определения (0-1)
        QString keyName;          // Название тональности (например, "C Major")
        float strength = 0.0f;    // Сила тональности
        bool isMajor = false;     // Мажорная или минорная
    };

    struct AnalysisResult {
        KeyInfo primaryKey;      // Основная тональность
        KeyInfo secondaryKey;    // Вторичная тональность (модуляция), UNKNOWN_KEY если нет
        QVector<float> chromaVector; // Хроматический вектор
        float overallConfidence = 0.0f; // Общая уверенность
        bool hasKeyChange = false;      // Есть ли смена тональности
        QVector<KeyInfo> keyChanges; // Позиции смены тональности
    };

    struct AnalysisOptions {
        float tuningFrequency;   // Частота настройки (обычно 440 Гц)
        int frameSize;          // Размер кадра для анализа
        int hopSize;            // Размер шага между кадрами
        bool detectKeyChanges;  // Определять ли смены тональности
        float keyChangeThreshold; // Порог для определения смены тональности

        AnalysisOptions() 
            : tuningFrequency(440.0f)
            , frameSize(4096)
            , hopSize(2048)
            , detectKeyChanges(true)
            , keyChangeThreshold(0.3f)
        {}
    };

    static AnalysisResult analyzeKey(const QVector<float>& samples, 
                                   int sampleRate,
                                   const AnalysisOptions& options = AnalysisOptions());
    
    // Методы интеграции с qm-dsp
    static AnalysisResult analyzeKeyUsingQM(const QVector<float>& samples, 
                                          int sampleRate,
                                          const AnalysisOptions& options);

    // ---- Потактовый анализ модуляции (смен тональности), как в Melodyne ----

    /// Параметры тактовой сетки (совпадают с WaveformView / PianoRollEngine).
    struct BarGrid {
        float bpm = 120.0f;
        int beatsPerBar = 4;          // 4/4->4, 3/4->3, 2/4->2, 1/4->1, 6/8->6, 12/8->12
        qint64 gridStartSample = 0;   // опорный сэмпл начала такта 1
    };

    /// Тональность одного такта.
    struct BarKey {
        int barIndex = 0;             // 0-based, отсчёт от gridStartSample
        qint64 startSample = 0;
        qint64 endSample = 0;
        KeyInfo key;
    };

    /// Непрерывный участок из тактов с одной тональностью (регион модуляции).
    struct KeyRegion {
        int startBar = 0;             // включительно, 0-based
        int endBar = 0;               // включительно
        qint64 startSample = 0;
        qint64 endSample = 0;
        KeyInfo key;
    };

    struct PerBarKeyResult {
        KeyInfo primaryKey;                 // доминирующая тональность (по числу тактов)
        QVector<BarKey> bars;               // по одной записи на проанализированный такт
        QVector<KeyRegion> regions;         // соседние такты с одинаковой тональностью объединены
        bool hasModulation = false;         // обнаружено больше одной тональности
    };

    /// Длина такта в сэмплах для заданной сетки (как в WaveformView::drawBarMarkers).
    static double samplesPerBar(const BarGrid& grid, int sampleRate);

    /// Потактово определяет тональность и группирует такты в регионы модуляции.
    static PerBarKeyResult analyzeKeyPerBar(const QVector<float>& samples,
                                            int sampleRate,
                                            const BarGrid& grid,
                                            const AnalysisOptions& options = AnalysisOptions());

    /// Объединяет соседние такты с одинаковой тональностью в регионы.
    static QVector<KeyRegion> mergeBarsIntoRegions(const QVector<BarKey>& bars);

    /// Собирает итог по готовым потактовым тональностям: доминирующая
    /// тональность, регионы модуляции и признак смены тональности.
    /// Общий хвост для анализа звука и для нот референсного MIDI.
    static PerBarKeyResult summarizeBarKeys(const QVector<BarKey>& bars);

    /// Наиболее частая тональность тактов, отличная от excludeKey (тональность
    /// модуляции для отображения во втором поле над пианороллом).
    /// Возвращает KeyInfo с key == UNKNOWN_KEY, если другой тональности нет.
    static KeyInfo dominantModulationKey(const PerBarKeyResult& perBar, Key excludeKey);

    // Вспомогательные методы
    static QString keyToString(Key key);
    static Key stringToKey(const QString& keyString);
    static bool isMajorKey(Key key);

    /// Хроматический вектор (12 полутонов) через алгоритм Гёрцеля.
    /// Самодостаточный анализ высоты — не зависит от qm-dsp, поэтому детерминирован в тестах.
    static QVector<float> computeChromaGoertzel(const QVector<float>& samples, int sampleRate);

    /// Тональность по готовому хроматическому вектору (12 полутонов).
    /// Публично: тем же способом определяется тональность референсного MIDI,
    /// где хрома строится не из звука, а из длительностей нот.
    static KeyInfo detectKeyFromChroma(const QVector<float>& chromaVector);

private:
    // Методы для работы с qm-dsp
    static QVector<double> convertToDouble(const QVector<float>& samples);
    static QVector<float> convertToFloat(const QVector<double>& samples);
    
    // Методы анализа
    static QVector<float> extractChromaFeatures(const QVector<float>& samples, 
                                               int sampleRate,
                                               int frameSize, 
                                               int hopSize);
    
    static QVector<KeyInfo> detectKeyChanges(const QVector<QVector<float>>& chromaFrames,
                                            float threshold);

    /// Вторая по времени звучания тональность трека (модуляция):
    /// гистограмма покадровых тональностей, порог доли кадров.
    static KeyInfo pickSecondaryKey(const QVector<QVector<float>>& chromaFrames,
                                    Key primaryKey);
};

#endif // KEYANALYZER_H
