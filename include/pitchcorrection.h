#ifndef PITCHCORRECTION_H
#define PITCHCORRECTION_H

#include <QtCore/QVector>
#include "pitchdetector.h"

/**
 * @brief Офлайн-коррекция высоты нот в аудио.
 *
 * Для каждой ноты с midiPitch != detectedPitch сегмент сдвигается по высоте на
 * разницу полутонов без изменения длительности: time stretch с тонкомпенсацией
 * (Rubber Band R3) + ресемплинг обратно к исходной длине. Границы сегментов
 * сшиваются коротким кроссфейдом.
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

/** Есть ли хотя бы одна нота, требующая коррекции (midiPitch != detectedPitch). */
bool hasPendingEdits(const QVector<PitchDetector::PitchNote>& notes);

} // namespace PitchCorrection

#endif // PITCHCORRECTION_H
