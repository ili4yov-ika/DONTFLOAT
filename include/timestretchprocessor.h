#ifndef TIMESTRETCHPROCESSOR_H
#define TIMESTRETCHPROCESSOR_H

#include <QVector>
#include <QString>
#include <cmath>
#include "markerengine.h"

/**
 * @brief Процессор для изменения времени аудио с сохранением высоты тона
 *
 * Реализует time stretching с тонкомпенсацией через Rubber Band Library (GPL v2+),
 * офлайн-движок R3 (OptionEngineFiner).
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
     * @brief Результат применения растяжения по меткам
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
        /**
         * Куда конец сегмента обязан прийти на выходе (сэмплы), -1 — не задано.
         * По нему длина сегмента подгоняется под **фактически** собранный выход:
         * иначе кроссфейд на каждом стыке съедает свои ~10 мс и метки уезжают
         * всё раньше и раньше — на длинной дорожке это десятки миллисекунд.
         */
        qint64 targetEndSample = -1;
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
    static QVector<float> processSegment(const QVector<float>& input, float stretchFactor, bool preservePitch = true, int sampleRate = 44100);

    /**
     * @brief Применяет сжатие-растяжение к многоканальному аудио
     * @param input Входные аудиоканалы
     * @param stretchFactor Коэффициент растяжения
     * @param preservePitch Сохранять ли высоту тона (по умолчанию true)
     * @return Обработанные аудиоканалы
     */
    static QVector<QVector<float>> processChannels(const QVector<QVector<float>>& input, float stretchFactor, bool preservePitch = true, int sampleRate = 44100);

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
     * @brief Метки выравнивания долей по сетке
     *
     * Каждая доля даёт метку: **откуда** (`originalPosition`) — её фактическая
     * позиция в звуке, **куда** (`position`) — ближайшая линия сетки. Сторона
     * важна: applyMarkerStretch режет исходник по `originalPosition`, поэтому
     * источником обязана быть реальная доля. Если перепутать, растяжение уводит
     * долю ещё дальше от сетки вместо того, чтобы поставить её на место.
     *
     * Начало и конец закрепляются метками «сам в себя», так что общая длина
     * дорожки не меняется — клип в DAW остаётся той же длины.
     *
     * @param beatPositions       позиции долей в сэмплах (сортируются сами)
     * @param beatIntervalSamples интервал сетки в сэмплах (60*sr/BPM)
     * @param gridStartSample     опорная линия сетки
     * @param totalSamples        длина дорожки
     * @param sampleRate          частота дискретизации
     * @return метки; пусто, если выравнивать нечего
     */
    static QVector<MarkerData> buildBeatAlignmentMarkers(
        const QVector<qint64>& beatPositions,
        double beatIntervalSamples,
        qint64 gridStartSample,
        qint64 totalSamples,
        int sampleRate);

    /**
     * @brief Ставит доли на сетку BPM растяжением участков между ними
     *
     * Обёртка над buildBeatAlignmentMarkers + applyMarkerStretch: доли едут на
     * ближайшие линии сетки, звук между ними тянется/жмётся с сохранением
     * высоты тона. Все каналы обрабатываются вместе, поэтому не расходятся.
     *
     * @return исходные данные без изменений, если выравнивать нечего
     */
    static StretchResult alignBeatsToGrid(
        const QVector<QVector<float>>& audioData,
        const QVector<qint64>& beatPositions,
        float bpm,
        int sampleRate,
        qint64 gridStartSample,
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

    /** ~5 мин на канал — выше порога realtime-превью волны отключается */
    static qint64 maxRealtimePreviewSamples(int sampleRate);

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
     * @brief Тонкомпенсация через Rubber Band (офлайн stretch)
     */
    static QVector<float> processWithPitchPreservation(const QVector<float>& input, float stretchFactor, int sampleRate = 44100);

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

