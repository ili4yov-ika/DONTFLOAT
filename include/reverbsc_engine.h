/**
 * reverbsc_engine.h — C++ обёртка над алгоритмом ReverbSC из LMMS/Soundpipe.
 *
 * Алгоритм: Sean Costello / Istvan Varga (Csound opcode "reverbsc"), 1999/2005.
 * Адаптирован для LMMS Paul Batchelor (GPL-2), портирован здесь без изменений алгоритма.
 *
 * Исходники: thirdparty/lmms/plugins/ReverbSC/{base.c,revsc.c,dcblock.c}
 * Скомпилированы как отдельные .c-файлы в CMakeLists (LMMS_REVERBSC_SOURCES).
 */

#ifndef REVERBSC_ENGINE_H
#define REVERBSC_ENGINE_H

#include <vector>
#include <cstring>

// C-интерфейс SoundPipe (из LMMS)
extern "C" {
#include "../thirdparty/lmms/plugins/ReverbSC/base.h"
#include "../thirdparty/lmms/plugins/ReverbSC/revsc.h"
#include "../thirdparty/lmms/plugins/ReverbSC/dcblock.h"
}

namespace ReverbEngine {

/// Параметры ревербератора
struct ReverbParams {
    float feedback  = 0.92f;   ///< Время затухания 0..1 (0.9–0.98 — рабочий диапазон)
    float lpFreq    = 8000.f;  ///< LP-фильтр на входе/выходе (Гц), 200–20000
    float wet       = 0.25f;   ///< Доля реверберации в финальном миксе 0..1
    bool  enabled   = false;   ///< Включён ли эффект
};

/**
 * Применяет реверберацию к многоканальному аудио.
 *
 * Работает с mono (1 канал) или stereo (2 канала).
 * Для mono использует одинаковые данные для L и R.
 * Результат содержит то же количество каналов, что и вход.
 *
 * @param data        Входные аудиоданные [канал][сэмпл] (float в [-1, 1])
 * @param sampleRate  Частота дискретизации
 * @param params      Параметры ревербератора
 * @return            Обработанные данные (тот же размер)
 */
inline std::vector<std::vector<float>> applyReverb(
        const std::vector<std::vector<float>>& data,
        int sampleRate,
        const ReverbParams& params)
{
    if (!params.enabled || data.empty() || data[0].empty()) {
        return data;
    }

    const int numChannels = static_cast<int>(data.size());
    const int numSamples  = static_cast<int>(data[0].size());

    // Настраиваем sp_data
    sp_data sp{};
    sp.sr    = static_cast<uint32_t>(sampleRate);
    sp.nchan = 2;
    sp.rand  = 1234;

    // Создаём и инициализируем revsc
    sp_revsc* revsc = nullptr;
    sp_revsc_create(&revsc);
    sp_revsc_init(&sp, revsc);
    revsc->feedback = params.feedback;
    revsc->lpfreq   = params.lpFreq;

    // DC-блокеры на выходах
    sp_dcblock* dcL = nullptr;
    sp_dcblock* dcR = nullptr;
    sp_dcblock_create(&dcL);
    sp_dcblock_create(&dcR);
    sp_dcblock_init(&sp, dcL, 1);
    sp_dcblock_init(&sp, dcR, 1);

    // Копируем входные данные как результат (wet mix будет добавлен поверх dry)
    std::vector<std::vector<float>> out(numChannels,
                                        std::vector<float>(numSamples, 0.f));

    const float wet = params.wet;
    const float dry = 1.f - wet;

    // Левый и правый каналы (моно → дублируем в оба)
    const std::vector<float>& chL = data[0];
    const std::vector<float>& chR = (numChannels >= 2) ? data[1] : data[0];

    for (int i = 0; i < numSamples; ++i) {
        float inL = chL[i];
        float inR = chR[i];
        float revL = 0.f, revR = 0.f;

        sp_revsc_compute(&sp, revsc, &inL, &inR, &revL, &revR);

        // DC-блокер (убирает DC-drift после ревербератора)
        float outL = 0.f, outR = 0.f;
        sp_dcblock_compute(&sp, dcL, &revL, &outL);
        sp_dcblock_compute(&sp, dcR, &revR, &outR);

        out[0][i] = dry * inL + wet * outL;
        if (numChannels >= 2) {
            out[1][i] = dry * inR + wet * outR;
        }
    }

    // Освобождаем ресурсы
    sp_revsc_destroy(&revsc);
    sp_dcblock_destroy(&dcL);
    sp_dcblock_destroy(&dcR);

    return out;
}

/// Перегрузка для QVector<QVector<float>> (Qt-типы)
template<typename QVectorFloat>
inline QVectorFloat applyReverbQt(const QVectorFloat& data,
                                   int sampleRate,
                                   const ReverbParams& params)
{
    if (!params.enabled || data.isEmpty() || data[0].isEmpty()) {
        return data;
    }

    // Конвертируем в std::vector
    std::vector<std::vector<float>> stdData;
    stdData.reserve(data.size());
    for (const auto& ch : data) {
        stdData.emplace_back(ch.constData(), ch.constData() + ch.size());
    }

    // Применяем реверберацию
    auto stdOut = applyReverb(stdData, sampleRate, params);

    // Конвертируем обратно в QVector
    QVectorFloat result;
    result.reserve(data.size());
    for (const auto& ch : stdOut) {
        typename QVectorFloat::value_type qch;
        qch.resize(static_cast<int>(ch.size()));
        std::copy(ch.begin(), ch.end(), qch.begin());
        result.append(qch);
    }
    return result;
}

} // namespace ReverbEngine

#endif // REVERBSC_ENGINE_H
