#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QToolTip>
#include <QtGui/QFontMetrics>
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
    void setPlaybackPosition(qint64 position);
    void setHorizontalOffset(float offset);
    void setTimeDisplayMode(bool showTime);
    void setBarsDisplayMode(bool showBars);
    int getSampleRate() const { return sampleRate; }
    float getHorizontalOffset() const { return horizontalOffset; }
    float getZoomLevel() const { return zoomLevel; }
    const QVector<QVector<float>>& getAudioData() const { return audioChannels; }

signals:
    void positionChanged(qint64 position); // Сигнал для обновления позиции воспроизведения
    void zoomChanged(float zoom); // Сигнал для обновления скроллбара

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawGrid(QPainter& painter);
    void drawBeatLines(QPainter& painter);
    void drawPlaybackCursor(QPainter& painter);
    void drawBarMarkers(QPainter& painter, const QRectF& rect);
    QPointF sampleToPoint(int sampleIndex, float value, const QRectF& rect) const;
    void adjustHorizontalOffset(float delta);
    void adjustZoomLevel(float delta);
    QString getPositionText(qint64 position) const;
    QString getBarText(float beatPosition) const;

    QVector<QVector<float>> audioChannels;
    float bpm;
    float zoomLevel;
    float horizontalOffset;
    bool isDragging;
    QPoint lastMousePos;
    int sampleRate;
    qint64 playbackPosition;
    float scrollStep;      // Шаг прокрутки
    float zoomStep;        // Шаг масштабирования
    bool showTimeDisplay;
    bool showBarsDisplay;

    static const QColor waveformColor;
    static const QColor beatLineColor;
    static const QColor barLineColor;
    static const QColor cursorColor;
    static const QColor markerTextColor;
    static const QColor markerBackgroundColor;
    static const int minZoom;
    static const int maxZoom;
    static const int markerHeight;
    static const int markerSpacing;
};

#endif // WAVEFORMVIEW_H 