#include "../include/markerengine.h"

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

} // namespace MarkerUtils
