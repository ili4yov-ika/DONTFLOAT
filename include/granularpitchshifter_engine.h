/**
 * granularpitchshifter_engine.h
 *
 * Гранулярный питч-шифтер, адаптированный из LMMS GranularPitchShifterEffect.
 * Copyright (c) 2024 Lost Robot <r94231/at/gmail/dot/com>, GPL-2.
 *
 * Алгоритм: гранулы читают из кольцевого буфера со скоростью 2^(pitch/12);
 * каждая гrana огибается косинусоидальным окном (аппроксимация Signalsmith),
 * Hermite-интерполяция обеспечивает качество.
 *
 * Зависимости: C++17, <cmath>, <vector>, <array>, <algorithm>
 */

#ifndef GRANULAR_PITCH_SHIFTER_ENGINE_H
#define GRANULAR_PITCH_SHIFTER_ENGINE_H

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

namespace GranularEngine {

// ─── Параметры ───────────────────────────────────────────────────────────────
struct Params {
    float pitchSemitones = 0.f;   ///< Сдвиг высоты тона в полутонах (-24..+24)
    float grainHz        = 8.f;   ///< Частота генерации гран (4..40 Гц)
    float shape          = 0.5f;  ///< Форма огибающей 0..1 (0 = equal-gain, 1 = equal-power)
    float jitter         = 0.f;   ///< Питч-рандомизация гран (0..1 октавы)
    float wet            = 1.f;   ///< Wet mix 0..1
    bool  prefilter      = true;  ///< Предфильтр против алиасинга
    bool  enabled        = false; ///< Включён ли эффект
};

// ─── Утилиты ────────────────────────────────────────────────────────────────

// Константы, совместимые с C++17 (без <numbers>)
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kSqrt2 = 1.41421356237309504880f;

/// Hermite (Catmull-Rom) интерполяция — как в оригинале LMMS
inline float hermite(float v0, float v1, float v2, float v3, float f) noexcept
{
    const float a = (-v0 + 3.f*v1 - 3.f*v2 + v3) * 0.5f;
    const float b =  v0 - 2.5f*v1 + 2.f*v2 - 0.5f*v3;
    const float c = (-v0 + v2) * 0.5f;
    return ((a * f + b) * f + c) * f + v1;
}

/// Аппроксимация cosine half-window (Signalsmith, как в LMMS)
inline float cosHalfWindow(float x, float k) noexcept
{
    const float A = x * (1.f - x);
    const float B = A * (1.f + k * A);
    const float C = B + x;
    return C * C;
}

/// k-коэффициент для cosHalfWindow (transition между equal-gain и equal-power)
inline float cosWindowK(float p) noexcept
{
    return -6.0026608f + p * (6.8773512f - 1.5838104f * p);
}

/// Простой LCG-PRNG [−1, +1]
inline float lcgRand(uint32_t& state) noexcept
{
    state = 1664525u * state + 1013904223u;
    return (static_cast<int32_t>(state) / 2147483648.f);
}

// ─── Предфильтр (2nd-order Butterworth LP) — как в LMMS PrefilterLowpass ────
struct PrefilterLP {
    float v0z = 0.f, v1 = 0.f, v2 = 0.f;
    float g1 = 0.f, g2 = 0.f, g3 = 0.f, g4 = 0.f;

    void setCoefs(float sampleRate, float cutoff) noexcept
    {
        const float g    = std::tan(kPi * cutoff / sampleRate);
        const float sqrt2 = kSqrt2;
        const float ginv = g / (1.f + g * (g + sqrt2));
        g1 = ginv;
        g2 = 2.f * (g + sqrt2) * ginv;
        g3 = g * ginv;
        g4 = 2.f * ginv;
    }

    float process(float in) noexcept
    {
        const float v1z = v1;
        const float v3  = in + v0z - 2.f * v2;
        v1  += g1 * v3 - g2 * v1z;
        v2  += g3 * v3 + g4 * v1z;
        v0z  = in;
        return v2;
    }
};

// ─── Гранула ──────────────────────────────────────────────────────────────────
struct Grain {
    std::array<double, 2> readPoint;
    double grainSpeed;   // скорость чтения из ring buffer = 2^(pitch/12) * jitterFactor
    double phase;        // 0..1 (жизнь гранулы)
    double phaseInc;     // 1/sizeSamples
};

// ─── Движок ──────────────────────────────────────────────────────────────────
class Engine {
public:
    void init(int sampleRate, int totalSamples)
    {
        sr = float(sampleRate);
        nyquist = sr * 0.5f;
        dcCoeff = std::exp(-2.f * kPi * 7.f / sr);

        // Кольцевой буфер: 5 секунд или длина трека (выбираем меньшее + запас)
        int ringLen = int(sr * 5.f);
        if (totalSamples > 0 && totalSamples < ringLen) ringLen = totalSamples + int(sr);
        ringBuf.assign(ringLen, {0.f, 0.f});
        ringLen_ = ringLen;

        writePoint = 0;
        timeSinceLastGrain = 999999;
        grains.clear();
        grains.reserve(16);

        dcVal = {0.f, 0.f};
        speed = {1., 1.};
        prefilter[0] = PrefilterLP{};
        prefilter[1] = PrefilterLP{};
        rng = 12345u;
        nextWait = 1.f;
    }

    /// Обрабатывает один стерео-сэмпл (inL/inR) → outL/outR, wet=1
    void process(float inL, float inR,
                 float grainHz, double pitchSpeed, float shape,
                 float jitter, bool usePrefilter,
                 float& outL, float& outR)
    {
        const float shapeK     = cosWindowK(shape);
        const int   sizeSamples = std::max(1, int(sr / grainHz));
        const float waitMult    = sizeSamples * 0.5f; // density=1 → одна гранула на grain

        // ── предфильтр (против алиасинга при сдвиге вверх) ──────────────────
        float filtL = inL, filtR = inR;
        if (usePrefilter) {
            float cutoff = float(std::min(double(nyquist) / pitchSpeed, double(nyquist))) * 0.96f;
            prefilter[0].setCoefs(sr, cutoff);
            prefilter[1].setCoefs(sr, cutoff);
            filtL = prefilter[0].process(inL);
            filtR = prefilter[1].process(inR);
        }

        // ── запись в кольцевой буфер ─────────────────────────────────────────
        ringBuf[writePoint] = {filtL, filtR};

        // ── порождаем новую гранулу если время ────────────────────────────────
        if (++timeSinceLastGrain >= int(nextWait * waitMult)) {
            timeSinceLastGrain = 0;
            const float r1 = lcgRand(rng);
            nextWait = std::exp2(r1 * 0.1f); // небольшой twitch

            const float jFactor = std::exp2(lcgRand(rng) * jitter);
            const double gs = pitchSpeed / jFactor;

            const int latency = std::max(3, int((gs - 1.) * sizeSamples + 3.));
            int rp = writePoint - latency;
            if (rp < 0) rp += ringLen_;

            Grain g;
            g.readPoint = {double(rp), double(rp)};
            g.grainSpeed = gs;
            g.phase = 0.;
            g.phaseInc = 1. / sizeSamples;
            grains.push_back(g);
        }

        // ── обрабатываем активные гранулы ─────────────────────────────────────
        float sL = 0.f, sR = 0.f;
        for (int i = 0; i < (int)grains.size(); ) {
            auto& g = grains[i];
            g.phase += g.phaseInc;
            if (g.phase >= 1.0) {
                grains.erase(grains.begin() + i);
                continue;
            }

            // прочитать из ring buffer с Hermite-интерполяцией
            auto readCh = [&](double rp, int ch) -> float {
                int ri = int(rp) % ringLen_;
                if (ri < 0) ri += ringLen_;
                double frac = rp - int(rp);

                auto at = [&](int idx) -> float {
                    idx = ((idx % ringLen_) + ringLen_) % ringLen_;
                    return ringBuf[idx][ch];
                };
                return hermite(at(ri-1), at(ri), at(ri+1), at(ri+2), float(frac));
            };

            // огибающая
            const float fadePos = std::clamp(
                (-std::abs(-2.f * float(g.phase) + 1.f) + 0.5f) + 0.5f, 0.f, 1.f);
            const float env = cosHalfWindow(fadePos, shapeK);

            sL += readCh(g.readPoint[0], 0) * env;
            sR += readCh(g.readPoint[1], 1) * env;

            g.readPoint[0] += g.grainSpeed;
            g.readPoint[1] += g.grainSpeed;
            if (g.readPoint[0] >= ringLen_) g.readPoint[0] -= ringLen_;
            if (g.readPoint[1] >= ringLen_) g.readPoint[1] -= ringLen_;

            ++i;
        }

        // ── DC removal ───────────────────────────────────────────────────────
        sL -= (dcVal[0] = (1.f - dcCoeff) * sL + dcCoeff * dcVal[0]);
        sR -= (dcVal[1] = (1.f - dcCoeff) * sR + dcCoeff * dcVal[1]);

        if (++writePoint >= ringLen_) writePoint = 0;

        outL = sL;
        outR = sR;
    }

private:
    std::vector<std::array<float, 2>> ringBuf;
    std::vector<Grain> grains;
    std::array<PrefilterLP, 2> prefilter;
    std::array<float, 2> dcVal = {0.f, 0.f};
    std::array<double, 2> speed = {1., 1.};

    float    sr = 44100.f;
    float    nyquist = 22050.f;
    float    dcCoeff = 0.f;
    float    nextWait = 1.f;
    int      ringLen_ = 0;
    int      writePoint = 0;
    int      timeSinceLastGrain = 999999;
    uint32_t rng = 12345u;
};

// ─── Публичный API (Qt-типы) ──────────────────────────────────────────────────

/**
 * Применяет гранулярный питч-шифтинг к многоканальному аудио.
 *
 * @param data       Входные данные [канал][сэмпл]
 * @param sampleRate Частота дискретизации
 * @param params     Параметры
 */
template<typename QVectorFloat>
inline QVectorFloat applyPitchShiftQt(const QVectorFloat& data,
                                      int sampleRate,
                                      const Params& params)
{
    if (!params.enabled || data.isEmpty() || data[0].isEmpty()) {
        return data;
    }

    const int numCh     = data.size();
    const int numSamples = data[0].size();
    const double pitchSpeed = std::exp2(params.pitchSemitones / 12.0);

    // Создаём и инициализируем движок
    Engine eng;
    eng.init(sampleRate, numSamples);

    // Прогрев: даём гранулам заполнить буфер прежде чем читать
    const int warmupSamples = std::min(int(sampleRate * 0.2f), numSamples / 4);

    QVectorFloat result;
    result.resize(numCh);
    for (int ch = 0; ch < numCh; ++ch) {
        result[ch].resize(numSamples, 0.f);
    }

    const float inL0 = numCh > 0 ? data[0][0] : 0.f;
    const float inR0 = numCh > 1 ? data[1][0] : inL0;
    for (int i = 0; i < warmupSamples; ++i) {
        float dummy1 = 0.f, dummy2 = 0.f;
        eng.process(inL0, inR0,
                    params.grainHz, pitchSpeed,
                    params.shape, params.jitter,
                    params.prefilter, dummy1, dummy2);
    }

    // Основной проход
    const float wet = params.wet;
    const float dry = 1.f - wet;

    for (int i = 0; i < numSamples; ++i) {
        const float inL = data[0][i];
        const float inR = numCh > 1 ? data[1][i] : inL;
        float outL = 0.f, outR = 0.f;
        eng.process(inL, inR,
                    params.grainHz, pitchSpeed,
                    params.shape, params.jitter,
                    params.prefilter, outL, outR);

        result[0][i] = dry * inL + wet * outL;
        if (numCh > 1) result[1][i] = dry * inR + wet * outR;
    }

    return result;
}

} // namespace GranularEngine

#endif // GRANULAR_PITCH_SHIFTER_ENGINE_H
