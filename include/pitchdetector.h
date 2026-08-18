#ifndef PITCHDETECTOR_H
#define PITCHDETECTOR_H

#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <functional>

/**
 * @brief Офлайн-детектор нот (f0 → сегменты нот) для пианоролла.
 *
 * Алгоритм: децимация под верхнюю границу диапазона → покадровая разностная
 * функция с кумулятивной нормировкой (YIN/CMNDF) и параболическим уточнением
 * минимума → медианное сглаживание → сегментация в ноты по стабильному
 * полутону с фильтром минимальной длительности.
 *
 * Кумулятивная нормировка берёт первый минимум ниже порога, а не глобальный:
 * именно это отсекает кратные периоды (ложные ноты на октаву-две вниз).
 */
namespace PitchDetector {

/** Нота, найденная в аудио. Координаты — сэмплы исходного аудио. */
struct PitchNote {
    qint64 startSample = 0;   ///< Начало ноты на таймлайне (включительно)
    qint64 endSample = 0;     ///< Конец ноты на таймлайне (исключительно)
    float midiPitch = 60.0f;      ///< Текущая (редактируемая) высота, MIDI (дробная — cents)
    float detectedPitch = 60.0f;  ///< Высота, определённая анализом, MIDI
    float confidence = 0.0f;  ///< Уверенность 0..1 (средняя автокорреляция)

    /**
     * Откуда берётся звук ноты — её место в исходном аудио.
     *
     * Пока ноту не двигали по времени, поля равны −1: звук лежит там же, где
     * нота нарисована. После сдвига по горизонтали (`startSample` изменился)
     * здесь остаётся исходный отрезок, и коррекция переносит звук оттуда на
     * новое место — иначе перестановка нот была бы не слышна.
     */
    qint64 sourceStartSample = -1;
    qint64 sourceEndSample = -1;

    /** Начало отрезка исходного аудио для этой ноты. */
    qint64 sourceStart() const { return sourceStartSample >= 0 ? sourceStartSample : startSample; }
    /** Конец отрезка исходного аудио для этой ноты. */
    qint64 sourceEnd() const { return sourceEndSample >= 0 ? sourceEndSample : endSample; }
    /** Ноту передвинули по времени (звук нужно перенести). */
    bool isMovedInTime() const { return sourceStart() != startSample; }
};

struct Options {
    /**
     * Нижняя граница поиска f0. По умолчанию 27.5 Гц (A0 — низ фортепиано).
     *
     * Внимание: окно анализа строится как два периода этой частоты, поэтому
     * снижение границы напрямую ухудшает временно́е разрешение. Например 16 Гц
     * требует окна ~200 мс, и ноты короче него будут смазаны. Опускайте порог
     * только для материала с заведомо длинными низкими нотами.
     */
    float minFrequencyHz = 27.5f;
    float maxFrequencyHz = 1200.0f;  ///< Верхняя граница поиска f0
    float minRms = 0.01f;            ///< Порог тишины (RMS кадра)
    float minCorrelation = 0.35f;    ///< Порог «вокализованности» кадра
    int minNoteDurationMs = 70;      ///< Минимальная длительность ноты

    /**
     * Эталон строя: частота ноты A4 в герцах. 440 — ISO 16, но встречаются
     * 432 («вердиевский»), 415 (барочный), 442–444 (многие оркестры).
     *
     * Сдвигает всю шкалу: при 432 Гц тон в 432 Гц читается как ровно A4, а не
     * как A4 −32 цента. На разницу midiPitch − detectedPitch, по которой
     * работает коррекция, эталон не влияет — обе величины в одном строе.
     */
    float referenceHz = 440.0f;
};

/** Стандартные строи для UI: подпись + частота A4. */
struct TuningStandard {
    const char* name;
    float hz;
};
const TuningStandard* tuningStandards(int& count);

/**
 * @brief Находит ноты в моно-сигнале.
 * @param mono        Моно-сэмплы (float, -1..1)
 * @param sampleRate  Частота дискретизации исходного аудио
 * @param options     Параметры детектора
 * @param onProgress  Прогресс 0..100 (вызывается из рабочего потока)
 */
QVector<PitchNote> detectNotes(const QVector<float>& mono,
                               int sampleRate,
                               const Options& options = Options(),
                               const std::function<void(int)>& onProgress = {});

/** Частота (Гц) → дробная MIDI-нота (69 = A4 при заданном эталоне строя). */
float frequencyToMidi(float hz, float referenceHz = 440.0f);

/** Дробная MIDI-нота → частота (Гц) при заданном эталоне строя. */
float midiToFrequency(float midi, float referenceHz = 440.0f);

} // namespace PitchDetector

#endif // PITCHDETECTOR_H
