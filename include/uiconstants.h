#ifndef UICONSTANTS_H
#define UICONSTANTS_H

namespace UiConstants {

constexpr int kScrollBarWidthPx = 16;

constexpr int kDefaultSplitterWaveformHeight = 450;
constexpr int kDefaultSplitterPitchGridHeight = 150;

constexpr int kWaveformContainerMinHeight = 150;
constexpr int kPitchGridContainerMinHeight = 80;

constexpr int kScrollBarNearCursorDistancePx = 50;
constexpr int kScrollBarMediumCursorDistancePx = 100;
constexpr int kScrollBarAlphaBase = 128;
constexpr int kScrollBarAlphaNear = 13;   // ~5% of 255
constexpr int kScrollBarAlphaMedium = 64; // ~25% of 255

constexpr float kOnsetDetectionThresholdRatio = 0.3f;
constexpr int kOnsetMinDistanceSampleRateDivisor = 10; // sampleRate / 10 → ~100 ms

} // namespace UiConstants

#endif // UICONSTANTS_H
