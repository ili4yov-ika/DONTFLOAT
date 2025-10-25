#ifndef BEATVISUALIZER_H
#define BEATVISUALIZER_H

#include <QVector>
#include <QPair>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QPainter>
#include <QString>
#include <QSet>
#include <memory>

// Forward declaration для Essentia интеграции
namespace essentia {
    class Algorithm;
    class Pool;
}

class BeatVisualizer
{
public:
    // Перечисление типов ударных (должно быть объявлено до использования)
    enum BeatType {
        KickDrum,      // Бас-барабан
        SnareDrum,     // Малый барабан
        HiHat,         // Хай-хэт
        Crash,         // Крэш
        Ride,          // Райд
        Tom,           // Том
        Unknown        // Неопределенный
    };

    // Расширенная информация об ударном
    struct BeatMarker {
        qint64 position;           // Позиция в сэмплах
        float confidence;          // Уверенность (0-1)
        float energy;             // Энергия ударного (0-1)
        float frequency;          // Основная частота ударного
        BeatType type;            // Тип ударного
        QColor color;            // Цвет маркера
        float size;              // Размер маркера
        bool isAccent;           // Является ли акцентом
    };

    // Настройки визуализации
    struct VisualizationSettings {
        bool showBeatMarkers;        // Показывать маркеры ударных
        bool showBeatTypes;          // Показывать типы ударных
        bool showEnergyLevels;       // Показывать уровни энергии
        bool showFrequencyInfo;     // Показывать частотную информацию
        bool useColorCoding;        // Использовать цветовое кодирование
        bool useSizeCoding;         // Использовать размерное кодирование
        bool showSpectrogram;       // Показывать спектрограмму
        float markerOpacity;        // Прозрачность маркеров (0-1)
        float spectrogramOpacity;   // Прозрачность спектрограммы (0-1)
        int spectrogramHeight;     // Высота спектрограммы в пикселях
        
        VisualizationSettings() 
            : showBeatMarkers(true)
            , showBeatTypes(true)
            , showEnergyLevels(true)
            , showFrequencyInfo(false)
            , useColorCoding(true)
            , useSizeCoding(true)
            , showSpectrogram(false)
            , markerOpacity(0.8f)
            , spectrogramOpacity(0.6f)
            , spectrogramHeight(100)
        {}
    };

    // Результат анализа ударных
    struct AnalysisResult {
        QVector<BeatMarker> beats;
        QVector<QVector<float>> spectrogram;  // Спектрограмма [time][frequency]
        QVector<float> frequencyBins;          // Частотные бины
        QVector<float> timeFrames;            // Временные кадры
        float analysisConfidence;             // Общая уверенность анализа
        bool hasComplexRhythm;                // Есть ли сложный ритм
        float averageEnergy;                   // Средняя энергия
    };

    // Конструктор
    // Статический класс - конструктор и деструктор не нужны
    // BeatVisualizer();
    // ~BeatVisualizer();

    // Основные методы анализа
    static AnalysisResult analyzeBeats(const QVector<QVector<float>>& audioData,
                                      int sampleRate,
                                      const VisualizationSettings& settings = VisualizationSettings());

    // Методы для работы с Essentia
    static AnalysisResult analyzeWithEssentia(const QVector<QVector<float>>& audioData,
                                            int sampleRate,
                                            const VisualizationSettings& settings);

    // Методы классификации ударных
    static BeatType classifyBeatType(float frequency, float energy, float duration);
    static QColor getBeatTypeColor(BeatType type);
    static QString getBeatTypeName(BeatType type);

    // Методы визуализации
    static void drawBeatMarkers(QPainter& painter, 
                               const QVector<BeatMarker>& beats,
                               const QRectF& rect,
                               float samplesPerPixel,
                               int startSample,
                               const VisualizationSettings& settings);

    static void drawSpectrogram(QPainter& painter,
                               const QVector<QVector<float>>& spectrogram,
                               const QRectF& rect,
                               const VisualizationSettings& settings);

    static void drawBeatEnergy(QPainter& painter,
                              const QVector<BeatMarker>& beats,
                              const QRectF& rect,
                              float samplesPerPixel,
                              int startSample,
                              const VisualizationSettings& settings);

    // Вспомогательные методы
    static QVector<float> calculateFrequencySpectrum(const QVector<float>& samples,
                                                     int sampleRate,
                                                     int windowSize = 1024);

    static float calculateBeatEnergy(const QVector<float>& samples,
                                   int position,
                                   int windowSize);

    static QVector<QPair<int, float>> detectOnsets(const QVector<float>& samples,
                                                   int sampleRate,
                                                   float threshold = 0.1f);

    // Настройки цветов
    static QColor getKickDrumColor() { return QColor(255, 100, 100); }    // Красный
    static QColor getSnareDrumColor() { return QColor(100, 255, 100); }   // Зеленый
    static QColor getHiHatColor() { return QColor(100, 100, 255); }        // Синий
    static QColor getCrashColor() { return QColor(255, 255, 100); }        // Желтый
    static QColor getRideColor() { return QColor(255, 100, 255); }          // Пурпурный
    static QColor getTomColor() { return QColor(100, 255, 255); }           // Голубой
    static QColor getUnknownColor() { return QColor(200, 200, 200); }       // Серый

private:
    // Внутренние методы анализа
    static QVector<BeatMarker> detectBeatTypes(const QVector<float>& samples,
                                              int sampleRate,
                                              const QVector<QPair<int, float>>& onsets);

    static QVector<QVector<float>> generateSpectrogram(const QVector<float>& samples,
                                                      int sampleRate,
                                                      int windowSize = 1024,
                                                      int hopSize = 512);

    static float calculateFrequencyContent(const QVector<float>& samples,
                                          int position,
                                          int windowSize,
                                          float targetFrequency,
                                          int sampleRate);

    // Настройки анализа
    static constexpr int DEFAULT_WINDOW_SIZE = 1024;
    static constexpr int DEFAULT_HOP_SIZE = 512;
    static constexpr float KICK_FREQ_MIN = 20.0f;    // Гц
    static constexpr float KICK_FREQ_MAX = 80.0f;     // Гц
    static constexpr float SNARE_FREQ_MIN = 150.0f;   // Гц
    static constexpr float SNARE_FREQ_MAX = 300.0f;   // Гц
    static constexpr float HIHAT_FREQ_MIN = 8000.0f;  // Гц
    static constexpr float HIHAT_FREQ_MAX = 20000.0f; // Гц
};

#endif // BEATVISUALIZER_H
