#ifndef UICONSTANTS_H
#define UICONSTANTS_H

namespace UiConstants {

constexpr int kScrollBarWidthPx = 16;

/** Горизонтальные отступы области таймлайна (волна, скролл, питч-сетка). */
constexpr int kTimelineHorizontalMarginPx = 2;

constexpr int kHorizontalScrollBarHeightPx = 20;
constexpr int kHorizontalScrollBarHorizontalMarginPx = 10;
constexpr int kHorizontalScrollBarTopMarginPx = 2;
constexpr int kHorizontalScrollBarBottomMarginPx = 3;

constexpr int kHorizontalScrollBarContainerHeightPx =
    kHorizontalScrollBarTopMarginPx + kHorizontalScrollBarHeightPx + kHorizontalScrollBarBottomMarginPx;

constexpr int kDefaultSplitterWaveformHeight = 450;
constexpr int kDefaultSplitterPitchGridHeight = 150;

constexpr int kWaveformContainerMinHeight = 150;
constexpr int kPitchGridContainerMinHeight = 80;

/** Вертикальный скролл питч-сетки: 90% непрозрачности в покое, до 50% у каретки. */
constexpr int kPitchGridScrollBarFadeRangePx = 10;
constexpr int kPitchGridScrollBarIdleAlpha = 230;   // 90% of 255
constexpr int kPitchGridScrollBarMinAlpha = 128;    // 50% of 255
constexpr int kPitchLabelAlpha = 230;               // 90% непрозрачности подписей нот

constexpr int kScrollBarNearCursorDistancePx = 50;
constexpr int kScrollBarMediumCursorDistancePx = 100;
constexpr int kScrollBarAlphaBase = 128;
constexpr int kScrollBarAlphaNear = 13;   // ~5% of 255
constexpr int kScrollBarAlphaMedium = 64; // ~25% of 255

constexpr float kOnsetDetectionThresholdRatio = 0.3f;
constexpr int kOnsetMinDistanceSampleRateDivisor = 10; // sampleRate / 10 → ~100 ms

} // namespace UiConstants

#endif // UICONSTANTS_H
