#include "../include/markerengine.h"

#include "../include/uiconstants.h"

#include <cmath>

// ============================================================================
// MarkerData - базовая структура без UI
// ============================================================================

MarkerData::MarkerData()
    : position(0)
    , originalPosition(0)
    , timeMs(0)
    , originalTimeMs(0)
    , isFixed(false)
    , isEndMarker(false)
{}

MarkerData::MarkerData(qint64 pos, int sampleRate)
    : position(pos)
    , originalPosition(pos)
    , isFixed(false)
    , isEndMarker(false)
{
    timeMs = TimeUtils::samplesToMs(pos, sampleRate);
    originalTimeMs = timeMs;
}

MarkerData::MarkerData(qint64 pos, bool fixed, int sampleRate)
    : position(pos)
    , originalPosition(pos)
    , isFixed(fixed)
    , isEndMarker(false)
{
    timeMs = TimeUtils::samplesToMs(pos, sampleRate);
    originalTimeMs = timeMs;
}

MarkerData::MarkerData(qint64 pos, bool fixed, bool endMarker, int sampleRate)
    : position(pos)
    , originalPosition(pos)
    , isFixed(fixed)
    , isEndMarker(endMarker)
{
    timeMs = TimeUtils::samplesToMs(pos, sampleRate);
    originalTimeMs = timeMs;
}

void MarkerData::updateTimeFromSamples(int sampleRate)
{
    timeMs = TimeUtils::samplesToMs(position, sampleRate);
    originalTimeMs = TimeUtils::samplesToMs(originalPosition, sampleRate);
}

void MarkerData::updateSamplesFromTime(int sampleRate)
{
    position = TimeUtils::msToSamples(timeMs, sampleRate);
    originalPosition = TimeUtils::msToSamples(originalTimeMs, sampleRate);
}

// ============================================================================
// Marker - расширенная структура с UI
// ============================================================================

Marker::Marker()
    : MarkerData()
    , isDragging(false)
    , isSelected(false)
    , dragStartSample(0)
{}

Marker::Marker(qint64 pos, int sampleRate)
    : MarkerData(pos, sampleRate)
    , isDragging(false)
    , isSelected(false)
    , dragStartSample(0)
{}

Marker::Marker(qint64 pos, bool fixed, int sampleRate)
    : MarkerData(pos, fixed, sampleRate)
    , isDragging(false)
    , isSelected(false)
    , dragStartSample(0)
{}

Marker::Marker(qint64 pos, bool fixed, bool endMarker, int sampleRate)
    : MarkerData(pos, fixed, endMarker, sampleRate)
    , isDragging(false)
    , isSelected(false)
    , dragStartSample(0)
{}

// ============================================================================
// MarkerUtils
// ============================================================================

namespace MarkerUtils {

QVector<MarkerData> toMarkerData(const QVector<Marker>& markers)
{
    QVector<MarkerData> out;
    out.reserve(markers.size());
    for (const Marker& m : markers) {
        out.append(static_cast<const MarkerData&>(m));
    }
    return out;
}

QVector<Marker> toMarkers(const QVector<MarkerData>& markerData)
{
    QVector<Marker> out;
    out.reserve(markerData.size());
    for (const MarkerData& md : markerData) {
        Marker m;
        static_cast<MarkerData&>(m) = md;
        out.append(m);
    }
    return out;
}

bool positionsMatch(const QVector<Marker>& current, const QVector<MarkerData>& snapshot)
{
    if (current.size() != snapshot.size()) {
        return false;
    }
    for (const MarkerData& snap : snapshot) {
        bool found = false;
        for (const Marker& m : current) {
            if (m.originalPosition == snap.originalPosition
                && m.isEndMarker == snap.isEndMarker
                && m.isFixed == snap.isFixed) {
                if (m.position != snap.position) {
                    return false;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

QVector<qint64> detectOnsetSamples(const QVector<QVector<float>>& channels, int sampleRate)
{
    if (channels.isEmpty() || channels[0].isEmpty() || sampleRate <= 0) {
        return {};
    }

    // --- Моно-сигнал ---
    const int numCh = channels.size();
    const int numSamples = channels[0].size();
    QVector<float> mono(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        double sum = 0.0;
        for (int ch = 0; ch < numCh; ++ch) {
            if (i < channels[ch].size()) {
                sum += channels[ch][i];
            }
        }
        mono[i] = static_cast<float>(sum / qMax(1, numCh));
    }

    // --- Огибающая и её нарастание (простая onset-функция) ---
    QVector<float> env(numSamples);
    const float alpha = 0.99f; // экспоненциальное сглаживание
    env[0] = std::fabs(mono[0]);
    for (int i = 1; i < numSamples; ++i) {
        const float x = std::fabs(mono[i]);
        env[i] = qMax(x, env[i - 1] * alpha);
    }

    QVector<float> diff(numSamples);
    diff[0] = 0.0f;
    float maxDiff = 0.0f;
    for (int i = 1; i < numSamples; ++i) {
        float d = env[i] - env[i - 1];
        if (d < 0.0f) {
            d = 0.0f;
        }
        diff[i] = d;
        if (d > maxDiff) {
            maxDiff = d;
        }
    }

    if (maxDiff <= 0.0f) {
        return {};
    }

    const float threshold = maxDiff * UiConstants::kOnsetDetectionThresholdRatio;
    const int minDistanceSamples =
        qMax(1, sampleRate / UiConstants::kOnsetMinDistanceSampleRateDivisor);

    QVector<qint64> onsets;
    onsets.reserve(256);
    int lastOnsetIdx = -minDistanceSamples;
    for (int i = 1; i < numSamples - 1; ++i) {
        if (diff[i] < threshold) {
            continue;
        }
        // простой локальный максимум
        if (diff[i] < diff[i - 1] || diff[i] <= diff[i + 1]) {
            continue;
        }
        if (i - lastOnsetIdx < minDistanceSamples) {
            continue;
        }
        onsets.append(i);
        lastOnsetIdx = i;
    }
    return onsets;
}

} // namespace MarkerUtils
