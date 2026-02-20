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
#include "bpmanalyzer.h"

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

        // Настройки для отклонений долей (beat deviations)
        bool showBeatDeviations;        // Показывать треугольные области отклонений
        bool showBeatWaveform;          // Показывать силуэт ударных поверх волны
        bool beatsAligned;              // Были ли доли выровнены (яркие/бледные цвета)
        float stripeWidth;              // Ширина диагональной полоски
        float stripeSpacing;            // Расстояние между полосками
        int deviationAlpha;             // Прозрачность областей отклонений (0-255)
        float deviationThreshold;       // Порог значимости отклонения (0-1, например 0.02 = 2%)
        float beatWaveformOpacity;      // Прозрачность силуэта ударных (0-1)

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
            , showBeatDeviations(true)      // Новые настройки
            , showBeatWaveform(true)
            , beatsAligned(false)
            , stripeWidth(4.0f)
            , stripeSpacing(3.0f)
            , deviationAlpha(100)
            , deviationThreshold(0.005f)  // Уменьшен порог с 2% до 0.5% для тестирования
            , beatWaveformOpacity(0.6f)
        {}
    };

    // Цветовая схема для отклонений долей
    struct BeatDeviationColors {
        QColor stretchedAligned;        // Растянутые выровненные (яркий красный)
        QColor stretchedUnaligned;      // Растянутые невыровненные (бледно-красный)
        QColor compressedAligned;       // Сжатые выровненные (яркий синий)
        QColor compressedUnaligned;     // Сжатые невыровненные (бледно-синий)
        QColor beatLineColor;           // Цвет вертикальной линии на позиции бита

        BeatDeviationColors()
            : stretchedAligned(220, 60, 60, 180)   // Красный (растянутые, выровненные)
            , stretchedUnaligned(200, 100, 100, 180) // Красный, менее насыщенный (не выровненные)
            , compressedAligned(60, 60, 220, 180)   // Синий (сжатые, выровненные)
            , compressedUnaligned(100, 100, 200, 180) // Синий, менее насыщенный (не выровненные)
            , beatLineColor(255, 255, 255, 120)
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

    // Новые методы для отклонений долей
    static void drawBeatDeviations(QPainter& painter,
                                   const QVector<BPMAnalyzer::BeatInfo>& beats,
                                   const QRectF& rect,
                                   float bpm,
                                   int sampleRate,
                                   float samplesPerPixel,
                                   int startSample,
                                   const VisualizationSettings& settings,
                                   const BeatDeviationColors& colors = BeatDeviationColors());

    static void drawBeatWaveform(QPainter& painter,
                                const QVector<float>& samples,
                                const QVector<BPMAnalyzer::BeatInfo>& beats,
                                const QRectF& rect,
                                int sampleRate,
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

    // Вспомогательные методы для отклонений долей
    static QPixmap createDiagonalPattern(float stripeWidth,
                                        float stripeSpacing,
                                        const QColor& color,
                                        bool isStretched);

    // Настройки визуализации отклонений (ширина треугольников)
    static constexpr float DEVIATION_TRIANGLE_MAX_WIDTH_PX = 36.0f;  // 100%
    static constexpr float DEVIATION_TRIANGLE_MIN_WIDTH_RATIO = 0.85f; // мин = 85% от макс

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
