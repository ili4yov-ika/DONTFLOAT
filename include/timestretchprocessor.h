#ifndef TIMESTRETCHPROCESSOR_H
#define TIMESTRETCHPROCESSOR_H

#include <QVector>
#include <cmath>

/**
 * @brief Процессор для изменения времени аудио с сохранением высоты тона
 * 
 * Реализует алгоритм time stretching с pitch correction на основе
 * простого ресемплинга с интерполяцией для сохранения высоты тона.
 */
class TimeStretchProcessor
{
public:
    /**
     * @brief Применяет сжатие-растяжение к аудиосегменту
     * @param input Входной аудиосегмент
     * @param stretchFactor Коэффициент растяжения (>1.0 = растяжение, <1.0 = сжатие)
     * @return Обработанный аудиосегмент
     */
    static QVector<float> processSegment(const QVector<float>& input, float stretchFactor);
    
    /**
     * @brief Применяет сжатие-растяжение к многоканальному аудио
     * @param input Входные аудиоканалы
     * @param stretchFactor Коэффициент растяжения
     * @return Обработанные аудиоканалы
     */
    static QVector<QVector<float>> processChannels(const QVector<QVector<float>>& input, float stretchFactor);

private:
    /**
     * @brief Линейная интерполяция между двумя сэмплами
     */
    static float lerp(float a, float b, float t);
    
    /**
     * @brief Кубическая интерполяция для более плавного результата
     */
    static float cubicInterpolate(float y0, float y1, float y2, float y3, float t);
};

#endif // TIMESTRETCHPROCESSOR_H

