#ifndef TIMESTRETCHPROCESSOR_H
#define TIMESTRETCHPROCESSOR_H

#include <QVector>
#include <cmath>

/**
 * @brief Процессор для изменения времени аудио с сохранением высоты тона
 * 
 * Реализует алгоритм time stretching с pitch correction на основе
 * улучшенного ресемплинга с интерполяцией для сохранения высоты тона.
 */
class TimeStretchProcessor
{
public:
    /**
     * @brief Режимы обработки
     */
    enum ProcessingMode {
        SimpleInterpolation,  // Простая интерполяция (быстро, но меняет pitch)
        PitchPreserving      // Тонкомпенсация (медленнее, но сохраняет pitch)
    };
    
    /**
     * @brief Применяет сжатие-растяжение к аудиосегменту
     * @param input Входной аудиосегмент
     * @param stretchFactor Коэффициент растяжения (>1.0 = растяжение, <1.0 = сжатие)
     * @param preservePitch Сохранять ли высоту тона (по умолчанию true)
     * @return Обработанный аудиосегмент
     */
    static QVector<float> processSegment(const QVector<float>& input, float stretchFactor, bool preservePitch = true);
    
    /**
     * @brief Применяет сжатие-растяжение к многоканальному аудио
     * @param input Входные аудиоканалы
     * @param stretchFactor Коэффициент растяжения
     * @param preservePitch Сохранять ли высоту тона (по умолчанию true)
     * @return Обработанные аудиоканалы
     */
    static QVector<QVector<float>> processChannels(const QVector<QVector<float>>& input, float stretchFactor, bool preservePitch = true);

private:
    /**
     * @brief Простая интерполяция (быстрая, но меняет pitch)
     */
    static QVector<float> processWithSimpleInterpolation(const QVector<float>& input, float stretchFactor);
    
    /**
     * @brief Обработка с сохранением pitch через изменение частоты дискретизации
     */
    static QVector<float> processWithPitchPreservation(const QVector<float>& input, float stretchFactor);
    
    /**
     * @brief Линейная интерполяция между двумя сэмплами
     */
    static float lerp(float a, float b, float t);
    
    /**
     * @brief Кубическая интерполяция для более плавного результата
     */
    static float cubicInterpolate(float y0, float y1, float y2, float y3, float t);
    
    /**
     * @brief Ресемплинг аудио с изменением частоты дискретизации
     * @param input Входной сигнал
     * @param inputSampleRate Исходная частота дискретизации
     * @param outputSampleRate Целевая частота дискретизации
     * @return Ресемплированный сигнал
     */
    static QVector<float> resample(const QVector<float>& input, float inputSampleRate, float outputSampleRate);
};

#endif // TIMESTRETCHPROCESSOR_H

