#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>
#include <QtMultimedia/QAudioBuffer>

class WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(QWidget *parent = nullptr);

    void setAudioData(const QVector<QVector<float>>& channels);
    void setBPM(float bpm);
    void setZoomLevel(float zoom);
    void setSampleRate(int rate);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawGrid(QPainter& painter);
    void drawBeatLines(QPainter& painter);
    QPointF sampleToPoint(int sampleIndex, float value, const QRectF& rect) const;

    QVector<QVector<float>> audioChannels;
    float bpm;
    float zoomLevel;
    float horizontalOffset;
    bool isDragging;
    QPoint lastMousePos;
    int sampleRate;

    static const QColor waveformColor;
    static const QColor beatLineColor;
    static const QColor barLineColor;
    static const int minZoom;
    static const int maxZoom;
};

#endif // WAVEFORMVIEW_H 