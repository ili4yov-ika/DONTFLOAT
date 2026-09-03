#ifndef TIMESTRETCHPROCESSOR_H
#define TIMESTRETCHPROCESSOR_H

#include <QVector>
#include <QString>
#include <cmath>
#include <memory>
#include "markerengine.h"
#include "rubberband_offline.h"
#include "bpmanalyzer.h"

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
     * @brief Кэш обработанных сегментов между метками
     *
     * Растяжение считается по сегментам, а границы сегментов задают метки.
     * Сдвиг одной метки меняет два соседних сегмента, а остальные выходят
     * ровно теми же — их и берём готовыми, вместо того чтобы гнать через
     * Rubber Band весь трек на каждое движение мыши.
     *
     * Кэш переживает вызовы applyMarkerStretch и принадлежит вызывающему.
     * При смене исходного аудио его обязательно сбросить (setSourceGeneration
     * или clear): ключ описывает границы и коэффициент, но не сами сэмплы.
     *
     * Не потокобезопасен: одновременно с ним работает одна задача.
     */
    class SegmentCache
    {
    public:
        SegmentCache();
        ~SegmentCache();
        SegmentCache(const SegmentCache&) = delete;
        SegmentCache& operator=(const SegmentCache&) = delete;

        /** Сбрасывает кэш, если поколение исходного аудио сменилось. */
        void setSourceGeneration(quint64 generation);
        void clear();

        /** Счётчики попаданий и промахов за всё время жизни — для диагностики. */
        int hitCount() const;
        int missCount() const;
        /** Сколько сэмплов сейчас лежит в кэше (по всем каналам). */
        qint64 storedSamples() const;

    private:
        friend class TimeStretchProcessor;
        struct Impl;
        std::unique_ptr<Impl> d;
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
    /** Качество тонкомпенсации: предпросмотр считается быстрым движком. */
    using Quality = RubberBandOffline::Quality;

    static QVector<float> processSegment(const QVector<float>& input, float stretchFactor,
                                         bool preservePitch = true, int sampleRate = 44100,
                                         Quality quality = Quality::Final);

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
     * @param quality     Качество тонкомпенсации: предпросмотр берёт быстрый
     *                    движок R2, итоговое применение — R3.
     * @param cache       Кэш сегментов (nullptr — считать всё заново).
     *                    С ним правка одной метки пересчитывает два сегмента
     *                    вместо всей дорожки, см. SegmentCache.
     * @return StretchResult с новыми аудиоданными и метками
     */
    static StretchResult applyMarkerStretch(
        const QVector<QVector<float>>& audioData,
        const QVector<MarkerData>& markers,
        int sampleRate,
        bool preservePitch = true,
        SegmentCache* cache = nullptr,
        Quality quality = Quality::Final);

    /**
     * @brief Опции построения меток выравнивания долей
     */
    struct AlignmentOptions {
        // Максимальное число меток выравнивания (0 = без ограничений).
        // Если больше — отбираются наиболее важные (приоритет по отклонению).
        int maxMarkers;
        // Минимальное расстояние между метками (в сэмплах). Если ближе — схлопываются.
        qint64 minMarkerSpacing;
        // Коэффициент плавности: метки сглаживают выравнивание на соседние интервалы.
        // 0.0 = резкое выравнивание только неровной доли, 1.0 = распределить на соседей.
        float smoothingFactor;
        // Порог отклонения, ниже которого доля не корректируется (в долях интервала).
        float correctionThreshold;

        AlignmentOptions()
            : maxMarkers(0)
            , minMarkerSpacing(0)
            , smoothingFactor(0.0f)
            , correctionThreshold(0.0f)
        {}
    };

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
     * @param options             опции выравнивания (nullptr = параметры по умолчанию)
     * @return метки; пусто, если выравнивать нечего
     */
    static QVector<MarkerData> buildBeatAlignmentMarkers(
        const QVector<qint64>& beatPositions,
        double beatIntervalSamples,
        qint64 gridStartSample,
        qint64 totalSamples,
        int sampleRate,
        const AlignmentOptions* options = nullptr);

    /**
     * @brief Умное построение меток по BeatInfo с приоритизацией
     *
     * Отбирает наиболее важные доли для коррекции (см. BPMAnalyzer::selectBeatsForCorrection),
     * строит метки с учётом сглаживания и ограничения на количество.
     *
     * @param beats               информация о долях (с deviation и confidence)
     * @param bpm                 BPM трека
     * @param sampleRate          частота дискретизации
     * @param gridStartSample     опорная линия сетки
     * @param totalSamples        длина дорожки
     * @param options             опции выравнивания
     * @return метки выравнивания
     */
    static QVector<MarkerData> buildSmartAlignmentMarkers(
        const QVector<BPMAnalyzer::BeatInfo>& beats,
        float bpm,
        int sampleRate,
        qint64 gridStartSample,
        qint64 totalSamples,
        const AlignmentOptions& options = AlignmentOptions());

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
        bool preservePitch = true,
        const AlignmentOptions* options = nullptr);

    /**
     * @brief Умное выравнивание с учётом отклонений и приоритетов
     *
     * Использует BeatInfo (deviation, confidence, energy) для приоритетного
     * отбора долей. Строит метки только для наиболее важных коррекций.
     *
     * @param audioData           аудиоданные (каналы × сэмплы)
     * @param beats               информация о долях
     * @param bpm                 BPM трека
     * @param sampleRate          частота дискретизации
     * @param gridStartSample     опорная линия сетки
     * @param preservePitch       сохранять ли высоту тона
     * @param options             опции выравнивания
     * @return результат растяжения
     */
    static StretchResult alignBeatsToGridSmart(
        const QVector<QVector<float>>& audioData,
        const QVector<BPMAnalyzer::BeatInfo>& beats,
        float bpm,
        int sampleRate,
        qint64 gridStartSample,
        bool preservePitch = true,
        const AlignmentOptions& options = AlignmentOptions());

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
    static QVector<float> processWithPitchPreservation(const QVector<float>& input, float stretchFactor,
                                                       int sampleRate = 44100,
                                                       Quality quality = Quality::Final);

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

