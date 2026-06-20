#ifndef GIADA_PITCHGRID_ENGINE_H
#define GIADA_PITCHGRID_ENGINE_H

/**
 * Адаптация логики geWaveform (Giada sample editor) для питч-сетки DONTFLOAT.
 * Copyright (C) 2010-2026 Giovanni A. Zuliani | Monocasual Laboratories — GPLv3
 * Адаптация: DONTFLOAT project, 2026.
 *
 * Используются: пиковая отрисовка волны (alloc), вертикальная сетка, snap к сетке.
 */

#include <QtCore/QVector>

namespace GiadaPitchGridEngine {

struct Viewport {
    float samplesPerPixel = 1.0f;
    int visibleSamples = 0;
    int maxStartSample = 0;
    int startSample = 0;

    static Viewport compute(qint64 totalSamples, int widthPx, float zoomLevel, float horizontalOffset);
    float sampleToPixelX(qint64 sample) const;
    qint64 pixelToSample(int x) const;
};

struct WaveformPeaks {
    QVector<int> upper;
    QVector<int> lower;
    int pixelWidth = 0;
    float ratio = 1.0f;

    bool isEmpty() const { return pixelWidth <= 0 || upper.isEmpty(); }
};

struct BeatLine {
    float x = 0.0f;
    bool isBarLine = false;
};

/** Пиковая огибающая волны (Giada geWaveform::alloc). */
WaveformPeaks buildWaveformPeaks(const QVector<QVector<float>>& channels,
                                 int pixelWidth,
                                 int widgetHeight);

/** Вертикальные линии тактовой сетки в видимой области (как WaveformView::drawBeatLines). */
QVector<BeatLine> visibleBeatLines(const Viewport& viewport,
                                   float bpm,
                                   int beatsPerBar,
                                   int sampleRate,
                                   qint64 gridStartSample,
                                   int widthPx);

/** Snap позиции к ближайшей доле (Giada geWaveform::snap). */
qint64 snapToBeatGrid(qint64 sample,
                      float bpm,
                      int beatsPerBar,
                      int sampleRate,
                      qint64 gridStartSample,
                      bool enabled);

bool isBlackKey(int midiNote);

} // namespace GiadaPitchGridEngine

#endif // GIADA_PITCHGRID_ENGINE_H
