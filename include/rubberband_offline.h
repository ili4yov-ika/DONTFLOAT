#ifndef RUBBERBAND_OFFLINE_H
#define RUBBERBAND_OFFLINE_H

#include <QtCore/QVector>

/**
 * @brief Офлайн time-stretch одного канала через Rubber Band Library (GPL v2+).
 */
namespace RubberBandOffline {

QVector<float> stretchMono(const QVector<float>& input, float timeRatio, int sampleRate);

} // namespace RubberBandOffline

#endif // RUBBERBAND_OFFLINE_H
