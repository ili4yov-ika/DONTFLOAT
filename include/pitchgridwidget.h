#ifndef PITCHGRIDWIDGET_H
#define PITCHGRIDWIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtCore/QTimer>
#include <QtMultimedia/QAudioBuffer>

class PitchGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PitchGridWidget(QWidget *parent = nullptr);

    void setAudioData(const QVector<QVector<float>>& data);
    void setSampleRate(int rate);
    void setPlaybackPosition(qint64 position); // position в миллисекундах
    void setHorizontalOffset(float offset);
    void setVerticalOffset(float offset);
    void setZoomLevel(float zoom);
    void setBPM(float bpm);
    void setBeatsPerBar(int beatsPerBar);
    void setPitchRange(int minPitch, int maxPitch);
    void setColorScheme(const QString& scheme);

signals:
    void positionChanged(qint64 position); // Сигнал для обновления позиции воспроизведения
    void pitchChanged(int pitch); // Сигнал для изменения выбранного питча

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void drawPitchGrid(QPainter& painter, const QRect& rect);
    void drawPlaybackCursor(QPainter& painter, const QRect& rect);
    void drawPitchLabels(QPainter& painter, const QRect& rect);
    void drawTimeGrid(QPainter& painter, const QRect& rect);
    QPointF sampleToPoint(int sampleIndex, int pitch, const QRectF& rect) const;
    QString getPitchName(int midiNote) const;
    int getPitchFromY(int y, const QRect& rect) const;
    qint64 getPositionFromX(int x, const QRect& rect) const;

    QVector<QVector<float>> audioData;
    int sampleRate;
    qint64 playbackPosition;
    float horizontalOffset;
    float verticalOffset;
    float zoomLevel;
    float bpm;
    int beatsPerBar;
    int minPitch;
    int maxPitch;
    bool isDragging;
    QPoint lastMousePos;
    QString colorScheme;
    int selectedPitch;
    
    // Цвета
    QColor backgroundColor;
    QColor gridColor;
    QColor cursorColor;
    QColor pitchLabelColor;
    QColor timeLabelColor;
    QColor selectionColor;

    static const int pitchHeight = 20; // Высота одной ноты в пикселях
    static const int timeGridSpacing = 100; // Расстояние между временными метками в пикселях
    static const int minPitchDefault = 21; // A0
    static const int maxPitchDefault = 108; // C8
};

#endif // PITCHGRIDWIDGET_H 