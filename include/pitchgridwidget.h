#ifndef PITCHGRIDWIDGET_H
#define PITCHGRIDWIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include "giada_pitchgrid_engine.h"

class PitchGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PitchGridWidget(QWidget *parent = nullptr);

    void setAudioData(const QVector<QVector<float>>& data);
    void setSampleRate(int rate);
    void setPlaybackPosition(qint64 position);
    void setCursorPosition(float xPosition);
    void setHorizontalOffset(float offset);
    void setVerticalOffset(float offset);
    void setZoomLevel(float zoom);
    void setTimelineReferenceWidth(int widthPx);
    void setTimelineSampleCount(qint64 samples);
    float playbackCursorContentX() const;
    void setBPM(float bpm);
    void setBeatsPerBar(int beatsPerBar);
    void setGridStartSample(qint64 sample);
    void setPitchRange(int minPitch, int maxPitch);
    void setColorScheme(const QString& scheme);
    void setBeatGridSnapEnabled(bool enabled);

signals:
    void positionChanged(qint64 position);
    void pitchChanged(int pitch);
    void horizontalOffsetChanged(float offset);
    void verticalOffsetChanged(float offset);
    void timelineZoomRequested(int angleDeltaY, float timelinePixelX);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void rebuildPeaksIfNeeded();
    void drawPianoRollBackground(QPainter& painter, const QRect& rect) const;
    void drawBeatGrid(QPainter& painter, const QRect& rect) const;
    void drawWaveformPeaks(QPainter& painter, const QRect& rect) const;
    void drawPitchLabels(QPainter& painter, const QRect& rect) const;
    void drawPlaybackCursor(QPainter& painter, const QRect& rect) const;
    void drawSelectedPitchRow(QPainter& painter, const QRect& rect) const;

    QString getPitchName(int midiNote) const;
    int getPitchFromY(int y, const QRect& rect) const;
    qint64 getPositionFromX(int x, const QRect& rect) const;
    GiadaPitchGridEngine::Viewport currentViewport() const;
    void adjustHorizontalOffset(float delta);
    void adjustVerticalOffset(float delta);
    int pitchContentHeightPx() const;
    int maxVerticalScrollPx() const;
    int verticalScrollPixels() const;
    int timelineContentWidthPx() const;
    int timelineReferenceWidth() const;
    float timelineToContentX(float timelineX) const;
    float contentToTimelineX(float contentX) const;
    qint64 effectiveTimelineSamples() const;

    QVector<QVector<float>> audioData;
    GiadaPitchGridEngine::WaveformPeaks waveformPeaks;
    int peaksBuildWidth = 0;
    int peaksBuildHeight = 0;

    int sampleRate;
    qint64 playbackPosition;
    qint64 gridStartSample;
    float cursorXPosition;
    float horizontalOffset;
    float verticalOffset;
    float zoomLevel;
    int timelineReferenceWidthPx;
    qint64 timelineSampleCount;
    float bpm;
    int beatsPerBar;
    int minPitch;
    int maxPitch;
    bool isDragging;
    bool isRightMousePanning;
    bool beatGridSnap;
    QPoint lastMousePos;
    QString colorScheme;
    int selectedPitch;

    QColor backgroundColor;
    QColor whiteKeyColor;
    QColor blackKeyColor;
    QColor gridColor;
    QColor beatGridColor;
    QColor barLineColor;
    QColor waveformColor;
    QColor cursorColor;
    QColor pitchLabelColor;
    QColor selectionColor;

    static const int pitchHeight = 16;
    static const int kPitchLabelColumnWidthPx = 36;
    static const int minVisiblePitchRows = 4;
    static const int minPitchDefault = 36;  // C2
    static const int maxPitchDefault = 84;  // C6
};

#endif // PITCHGRIDWIDGET_H
