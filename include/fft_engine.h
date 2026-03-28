/**
 * fft_engine.h — самодостаточный Cooley-Tukey FFT для спектрограммы DONTFLOAT.
 * Алгоритм оконных функций адаптирован из LMMS fft_helpers.cpp
 * (Copyright (c) 2008-2012 Tobias Doerffel, 2019 Martin Pavelek, GPL-2).
 *
 * Требования: C++17, <cmath>, <complex>, <vector>, <algorithm>
 */

#ifndef FFT_ENGINE_H
#define FFT_ENGINE_H

#include <cmath>
#include <complex>
#include <vector>
#include <algorithm>

namespace DFEngine {

// ─── Типы ────────────────────────────────────────────────────────────────────
using Complex = std::complex<float>;

// ─── Оконные функции (идентичны LMMS FFTWindow) ──────────────────────────────
enum class WindowFunction {
    Rectangular = 0,
    BlackmanHarris,   // лучшее подавление боковых лепестков — рекомендуется
    Hamming,
    Hanning
};

/// Предвычисляет оконную функцию длиной `length` (с нормировкой амплитуды).
inline void precomputeWindow(std::vector<float>& w, unsigned length,
                             WindowFunction type, bool normalize = true)
{
    w.resize(length);
    constexpr float pi2 = 6.28318530717958647692f;

    float a0 = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f;
    switch (type) {
    case WindowFunction::Rectangular:
        for (unsigned i = 0; i < length; ++i) w[i] = 1.f;
        return;
    case WindowFunction::BlackmanHarris:
        a0 = 0.35875f; a1 = 0.48829f; a2 = 0.14128f; a3 = 0.01168f; break;
    case WindowFunction::Hamming:
        a0 = 0.54f; a1 = 0.46f; break;
    case WindowFunction::Hanning:
        a0 = 0.50f; a1 = 0.50f; break;
    }

    float gain = 0.f;
    for (unsigned i = 0; i < length; ++i) {
        w[i] = a0
             - a1 * std::cos(pi2 * i / (float(length) - 1.f))
             + a2 * std::cos(2.f * pi2 * i / (float(length) - 1.f))
             - a3 * std::cos(3.f * pi2 * i / (float(length) - 1.f));
        gain += w[i];
    }
    if (normalize && gain > 0.f) {
        gain /= float(length);
        for (float& v : w) v /= gain;
    }
}

// ─── Cooley-Tukey FFT (radix-2 in-place DIT) ─────────────────────────────────
/// Возвращает наименьшую степень 2, >= n.
inline unsigned nextPow2(unsigned n)
{
    if (n == 0) return 1;
    --n;
    n |= n >> 1; n |= n >> 2; n |= n >> 4; n |= n >> 8; n |= n >> 16;
    return n + 1;
}

/// In-place FFT (DIT). `buf` должен иметь размер степени 2.
inline void fft(std::vector<Complex>& buf)
{
    const unsigned N = static_cast<unsigned>(buf.size());
    if (N < 2) return;

    // bit-reversal permutation
    for (unsigned i = 1, j = 0; i < N; ++i) {
        unsigned bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }

    // butterfly stages
    for (unsigned len = 2; len <= N; len <<= 1) {
        float angle = -6.28318530717958647692f / float(len);
        Complex wlen(std::cos(angle), std::sin(angle));
        for (unsigned i = 0; i < N; i += len) {
            Complex w(1.f, 0.f);
            for (unsigned j = 0; j < len / 2; ++j) {
                Complex u = buf[i + j];
                Complex v = buf[i + j + len/2] * w;
                buf[i + j]           = u + v;
                buf[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// ─── Real-FFT удобная обёртка ────────────────────────────────────────────────
/**
 * Вычисляет спектр вещественного сигнала.
 *
 * @param samples      входные сэмплы
 * @param count        число отсчётов (может быть не степенью 2)
 * @param window       предвычисленное окно (size == count)
 * @param zeroPadFactor zero-padding: итоговый FFT-блок = nextPow2(count) * zeroPadFactor
 * @param magnitudes   выходной массив амплитуд, size = fftSize/2+1
 */
inline void realFFT(const float* samples, unsigned count,
                    const std::vector<float>& window,
                    unsigned zeroPadFactor,
                    std::vector<float>& magnitudes)
{
    const unsigned fftSize = nextPow2(count) * zeroPadFactor;

    std::vector<Complex> buf(fftSize, Complex(0.f, 0.f));
    for (unsigned i = 0; i < count && i < fftSize; ++i) {
        float w = (i < window.size()) ? window[i] : 1.f;
        buf[i] = Complex(samples[i] * w, 0.f);
    }

    fft(buf);

    const unsigned binCount = fftSize / 2 + 1;
    magnitudes.resize(binCount);
    for (unsigned k = 0; k < binCount; ++k) {
        magnitudes[k] = std::abs(buf[k]);
    }
}

// ─── Нормализация ────────────────────────────────────────────────────────────
/// Нормализует массив амплитуд к [0,1] по максимуму.
inline void normalizeLinear(std::vector<float>& mag)
{
    float mx = *std::max_element(mag.begin(), mag.end());
    if (mx > 0.f)
        for (float& v : mag) v /= mx;
}

/**
 * Нормализует в логарифмическом (dB) масштабе.
 * floorDb — нижняя граница (например -90 dB → 0).
 */
inline void normalizeDb(std::vector<float>& mag, float floorDb = -90.f)
{
    float mx = *std::max_element(mag.begin(), mag.end());
    if (mx <= 0.f) { std::fill(mag.begin(), mag.end(), 0.f); return; }

    for (float& v : mag) {
        float db = (v > 0.f) ? (20.f * std::log10(v / mx)) : floorDb;
        // отображение [floorDb, 0] → [0, 1]
        v = std::clamp((db - floorDb) / (-floorDb), 0.f, 1.f);
    }
}

// ─── Log-частотная перемаппинг ───────────────────────────────────────────────
/**
 * Сжимает `freqBinsIn` линейных бинов в `freqBinsOut` логарифмических полос.
 * Частотный диапазон: [minFreq, nyquist].
 * Адаптировано по идее LMMS compressbands().
 */
inline void logFreqCompress(const std::vector<float>& linear,
                            std::vector<float>& log_out,
                            unsigned freqBinsOut,
                            float sampleRate,
                            float minFreqHz = 20.f)
{
    const unsigned N = static_cast<unsigned>(linear.size());
    if (N < 2) return;
    log_out.assign(freqBinsOut, 0.f);

    const float nyquist  = sampleRate * 0.5f;
    const float logMin   = std::log2(minFreqHz);
    const float logMax   = std::log2(nyquist);
    const float logRange = logMax - logMin;

    // для каждой выходной log-полосы
    for (unsigned y = 0; y < freqBinsOut; ++y) {
        // нижняя и верхняя частоты полосы
        float logLo = logMin + logRange * float(y)       / float(freqBinsOut);
        float logHi = logMin + logRange * float(y + 1)   / float(freqBinsOut);
        float freqLo = std::pow(2.f, logLo);
        float freqHi = std::pow(2.f, logHi);

        // перевод в индексы линейных бинов
        int binLo = int(freqLo / nyquist * float(N - 1));
        int binHi = int(freqHi / nyquist * float(N - 1));
        if (binLo < 0) binLo = 0;
        if (binHi >= (int)N) binHi = (int)N - 1;
        if (binHi < binLo) binHi = binLo;

        // максимум в диапазоне (лучше среднего для спектрограммы)
        float peak = 0.f;
        for (int b = binLo; b <= binHi; ++b) {
            if (linear[b] > peak) peak = linear[b];
        }
        log_out[y] = peak;
    }
}

} // namespace DFEngine

#endif // FFT_ENGINE_H
