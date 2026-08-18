#ifndef PITCHCORRECTION_H
#define PITCHCORRECTION_H

#include <QtCore/QVector>
#include "pitchdetector.h"

/**
 * @brief Офлайн-коррекция нот в аудио: высота и позиция.
 *
 * Для каждой ноты с midiPitch != detectedPitch сегмент сдвигается по высоте на
 * разницу полутонов без изменения длительности: time stretch с тонкомпенсацией
 * (Rubber Band R3) + ресемплинг обратно к исходной длине. Границы сегментов
 * сшиваются коротким кроссфейдом.
 *
 * Ноту, переставленную по времени (`PitchNote::isMovedInTime`), мало
 * перерисовать: её звук берётся из исходного отрезка (`sourceStart()`) и
 * кладётся на новое место, а старое освобождается. Иначе ноты A B C D,
 * переставленные в порядок C D A B, продолжали бы звучать как A B C D.
 */
namespace PitchCorrection {

/**
 * @param channels    Аудиоканалы (каналы × сэмплы)
 * @param notes       Ноты в координатах этих аудиоданных
 * @param sampleRate  Частота дискретизации
 * @return Обработанные каналы той же длины (или входные, если менять нечего)
 */
QVector<QVector<float>> apply(const QVector<QVector<float>>& channels,
                              const QVector<PitchDetector::PitchNote>& notes,
                              int sampleRate);

/** Есть ли правки, которые надо пересчитать: смена высоты или перенос ноты. */
bool hasPendingEdits(const QVector<PitchDetector::PitchNote>& notes);

} // namespace PitchCorrection

#endif // PITCHCORRECTION_H
