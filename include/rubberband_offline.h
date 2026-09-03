#ifndef RUBBERBAND_OFFLINE_H
#define RUBBERBAND_OFFLINE_H

#include <QtCore/QVector>

/**
 * @brief Офлайн time-stretch одного канала через Rubber Band Library (GPL v2+).
 */
namespace RubberBandOffline {

/**
 * Какой движок Rubber Band брать.
 *
 * R3 (Finer) звучит лучше и стоит кратно дороже. Для предпросмотра, который
 * пересчитывается на каждую правку метки, эта разница не нужна — там важнее
 * успеть за рукой. Итоговое применение считается R3.
 */
enum class Quality {
    Preview,  ///< R2 (OptionEngineFaster) — быстрый предпросмотр
    Final     ///< R3 (OptionEngineFiner) — итоговое качество
};

QVector<float> stretchMono(const QVector<float>& input, float timeRatio, int sampleRate,
                           Quality quality = Quality::Final);

} // namespace RubberBandOffline

#endif // RUBBERBAND_OFFLINE_H
