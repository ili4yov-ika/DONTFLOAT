#ifndef TIMESTRETCHPROCESSOR_H
#define TIMESTRETCHPROCESSOR_H

#include <QVector>
#include <QString>
#include <cmath>
#include "markerengine.h"

/**
 * @brief Процессор для изменения времени аудио с сохранением высоты тона
 *
 * Реализует алгоритм time stretching с pitch correction на основе
 * улучшенного ресемплинга с интерполяцией для сохранения высоты тона.
 * Также предоставляет высокоуровневые методы для работы с метками.
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
     * @brief Результат применения сжатия/растяжения по меткам
     */
    struct StretchResult {
        QVector<QVector<float>> audioData; ///< Обработанные аудиоданные
        QVector<MarkerData> newMarkers;    ///< Обновлённые метки под новую длину
    };

    /**
     * @brief Информация о сегменте между метками
     */
    struct StretchSegment {
        qint64 startSample;      ///< Начало сегмента в исходных данных
        qint64 endSample;        ///< Конец сегмента в исходных данных
        float stretchFactor;     ///< Коэффициент растяжения
        bool preservePitch;      ///< Сохранять ли pitch
    };

    // ========================================================================
    // НИЗКОУРОВНЕВЫЕ МЕТОДЫ (существующие)
    // ========================================================================

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

    // ========================================================================
    // ВЫСОКОУРОВНЕВЫЕ МЕТОДЫ ДЛЯ РАБОТЫ С МЕТКАМИ (новые)
    // ========================================================================

    /**
     * @brief Применяет сжатие/растяжение ко всему аудио на основе меток
     *
     * Логика полностью перенесена из MarkerStretchEngine.
     *
     * @param audioData   Исходные аудиоданные (каналы x сэмплы)
     * @param markers     Текущие метки (position/originalPosition)
     * @param sampleRate  Частота дискретизации
     * @return StretchResult с новыми аудиоданными и метками
     */
    static StretchResult applyMarkerStretch(
        const QVector<QVector<float>>& audioData,
        const QVector<MarkerData>& markers,
        int sampleRate,
        bool preservePitch = true);

    /**
     * @brief Вычисляет сегменты для обработки на основе меток
     *
     * @param markers     Метки (автоматически сортируются по originalPosition)
     * @param audioSize   Размер аудио в сэмплах
     * @return Список сегментов с коэффициентами растяжения
     */
    static QVector<StretchSegment> calculateSegments(
        const QVector<MarkerData>& markers,
        qint64 audioSize,
        bool preservePitch = true);

    /**
     * @brief Валидирует метки перед обработкой
     *
     * Проверяет:
     * - Минимальное количество меток (>= 2)
     * - Границы позиций меток
     * - Коэффициенты растяжения в допустимых пределах (>= 0.1)
     *
     * @param markers     Метки для валидации
     * @param audioSize   Размер аудио в сэмплах
     * @param errorMsg    [out] Сообщение об ошибке (если есть)
     * @return true если метки валидны
     */
    static bool validateMarkers(
        const QVector<MarkerData>& markers,
        qint64 audioSize,
        QString* errorMsg = nullptr);

    /**
     * @brief Вычисляет коэффициент растяжения между двумя метками
     *
     * @param startMarker  Начальная метка
     * @param endMarker    Конечная метка
     * @return Коэффициент растяжения (>1.0 = растяжение, <1.0 = сжатие)
     */
    static float calculateStretchFactor(
        const MarkerData& startMarker,
        const MarkerData& endMarker);

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

