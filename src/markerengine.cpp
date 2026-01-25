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
    , dragStartSample(0)
{}

Marker::Marker(qint64 pos, int sampleRate)
    : MarkerData(pos, sampleRate)
    , isDragging(false)
    , dragStartSample(0)
{}

Marker::Marker(qint64 pos, bool fixed, int sampleRate)
    : MarkerData(pos, fixed, sampleRate)
    , isDragging(false)
    , dragStartSample(0)
{}

Marker::Marker(qint64 pos, bool fixed, bool endMarker, int sampleRate)
    : MarkerData(pos, fixed, endMarker, sampleRate)
    , isDragging(false)
    , dragStartSample(0)
{}
