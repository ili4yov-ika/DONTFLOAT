#ifndef PITCHDETECTOR_H
#define PITCHDETECTOR_H

#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <functional>

/**
 * @brief Офлайн-детектор нот (f0 → сегменты нот) для пианоролла.
 *
 * Алгоритм: децимация до ~11 кГц → покадровая нормированная автокорреляция
 * (оценка f0) → медианное сглаживание → сегментация в ноты по стабильному
 * полутону с фильтром минимальной длительности.
 */
namespace PitchDetector {

/** Нота, найденная в аудио. Координаты — сэмплы исходного аудио. */
struct PitchNote {
    qint64 startSample = 0;   ///< Начало ноты (включительно)
    qint64 endSample = 0;     ///< Конец ноты (исключительно)
    int midiPitch = 0;        ///< Текущая (редактируемая) высота, MIDI
    int detectedPitch = 0;    ///< Высота, определённая анализом, MIDI
    float confidence = 0.0f;  ///< Уверенность 0..1 (средняя автокорреляция)
};

struct Options {
    float minFrequencyHz = 60.0f;    ///< Нижняя граница поиска f0
    float maxFrequencyHz = 1200.0f;  ///< Верхняя граница поиска f0
    float minRms = 0.01f;            ///< Порог тишины (RMS кадра)
    float minCorrelation = 0.35f;    ///< Порог «вокализованности» кадра
    int minNoteDurationMs = 70;      ///< Минимальная длительность ноты
};

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

/** Частота (Гц) → дробная MIDI-нота (69 = A4 = 440 Гц). */
float frequencyToMidi(float hz);

} // namespace PitchDetector

#endif // PITCHDETECTOR_H
