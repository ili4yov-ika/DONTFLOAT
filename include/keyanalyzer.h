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
        Key key;
        float confidence;        // Уверенность определения (0-1)
        QString keyName;         // Название тональности (например, "C Major")
        float strength;          // Сила тональности
        bool isMajor;           // Мажорная или минорная
    };

    struct AnalysisResult {
        KeyInfo primaryKey;      // Основная тональность
        KeyInfo secondaryKey;    // Вторичная тональность (если есть)
        QVector<float> chromaVector; // Хроматический вектор
        float overallConfidence; // Общая уверенность
        bool hasKeyChange;       // Есть ли смена тональности
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

    // Вспомогательные методы
    static QString keyToString(Key key);
    static Key stringToKey(const QString& keyString);
    static bool isMajorKey(Key key);

private:
    // Методы для работы с qm-dsp
    static QVector<double> convertToDouble(const QVector<float>& samples);
    static QVector<float> convertToFloat(const QVector<double>& samples);
    
    // Методы анализа
    static QVector<float> extractChromaFeatures(const QVector<float>& samples, 
                                               int sampleRate,
                                               int frameSize, 
                                               int hopSize);
    
    static KeyInfo detectKeyFromChroma(const QVector<float>& chromaVector);
    
    static QVector<KeyInfo> detectKeyChanges(const QVector<QVector<float>>& chromaFrames,
                                            float threshold);
};

#endif // KEYANALYZER_H
