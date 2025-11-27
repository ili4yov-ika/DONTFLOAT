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
#include "bpmanalyzer.h"
#include "waveformcolors.h"
// #include "beatvisualizer.h"

// Структура для меток
struct Marker {
    qint64 position; // Позиция метки в сэмплах
    qint64 originalPosition; // Исходная позиция метки (для вычисления коэффициента)
    bool isDragging; // Флаг перетаскивания
    QPoint dragStartPos; // Начальная позиция перетаскивания
    qint64 dragStartSample; // Начальный сэмпл при перетаскивании
    bool isFixed; // Флаг неподвижности метки (нельзя перетаскивать)
    bool isEndMarker; // Флаг конечной метки (особая метка в конце таймлайна, можно двигать, но не создаются новые метки справа)
    
    Marker() : position(0), originalPosition(0), isDragging(false), isFixed(false), isEndMarker(false) {}
    Marker(qint64 pos) : position(pos), originalPosition(pos), isDragging(false), isFixed(false), isEndMarker(false) {}
    Marker(qint64 pos, bool fixed) : position(pos), originalPosition(pos), isDragging(false), isFixed(fixed), isEndMarker(false) {}
    Marker(qint64 pos, bool fixed, bool endMarker) : position(pos), originalPosition(pos), isDragging(false), isFixed(fixed), isEndMarker(endMarker) {}
};

class WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(QWidget *parent = nullptr);

    void setAudioData(const QVector<QVector<float>>& data);
    void setBeatInfo(const QVector<BPMAnalyzer::BeatInfo>& beats);
    void setGridStartSample(qint64 sample) { gridStartSample = sample; update(); }
    void setBPM(float bpm);
    void setSampleRate(int rate);
    int getSampleRate() const { return sampleRate; }
    void setPlaybackPosition(qint64 position); // position в миллисекундах
    qint64 getPlaybackPosition() const { return playbackPosition; }
    float getCursorXPosition() const; // Позиция каретки в пикселях
    void setHorizontalOffset(float offset);
    void setVerticalOffset(float offset);
    void setZoomLevel(float zoom);
    float getZoomLevel() const { return zoomLevel; }
    float getHorizontalOffset() const { return horizontalOffset; }
    float getVerticalOffset() const { return verticalOffset; }
    void setColorScheme(const QString& scheme);
    const QVector<QVector<float>>& getAudioData() const { return audioData; }
    void setLoopStart(qint64 position);
    void setLoopEnd(qint64 position);
    void setTimeDisplayMode(bool showTime);
    void setBarsDisplayMode(bool showBars);
    void setBeatsPerBar(int beats);
    int getBeatsPerBar() const { return beatsPerBar; }
    void setShowBeatDeviations(bool show);
    bool getShowBeatDeviations() const { return showBeatDeviations; }
    void setShowBeatWaveform(bool show);
    bool getShowBeatWaveform() const { return showBeatWaveform; }
    void setBeatsAligned(bool aligned) { beatsAligned = aligned; update(); }
    bool getBeatsAligned() const { return beatsAligned; }
    
    // Методы для работы с метками
    void addMarker(qint64 position);
    void removeMarker(int index);
    QVector<Marker> getMarkers() const { return markers; }
    void setMarkers(const QVector<Marker>& newMarkers) { markers = newMarkers; update(); }
    void clearMarkers() { markers.clear(); update(); }
    
    // Методы для применения сжатия-растяжения
    QVector<QVector<float>> applyTimeStretch(const QVector<Marker>& markers) const;
    
    // Новые методы для улучшенной визуализации ударных (временно отключены)
    /*
    void setBeatVisualizationSettings(const BeatVisualizer::VisualizationSettings& settings);
    BeatVisualizer::VisualizationSettings getBeatVisualizationSettings() const { return beatVisualizationSettings; }
    void analyzeBeats(); // Запуск анализа ударных
    */

signals:
    void positionChanged(qint64 position); // Сигнал для обновления позиции воспроизведения (в миллисекундах)
    void zoomChanged(float zoom); // Сигнал для обновления скроллбара
    void horizontalOffsetChanged(float offset); // Сигнал для обновления горизонтального скроллбара

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawBeatWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawWaveformChannel(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawGrid(QPainter& painter, const QRect& rect);
    void drawBeatLines(QPainter& painter, const QRect& rect);
    void drawPlaybackCursor(QPainter& painter, const QRect& rect);
    void drawBarMarkers(QPainter& painter, const QRectF& rect);
    void drawBeatDeviations(QPainter& painter, const QRectF& rect);
    void drawLoopMarkers(QPainter& painter, const QRect& rect);
    void drawMarkers(QPainter& painter, const QRect& rect);
    QPointF sampleToPoint(int sampleIndex, float value, const QRectF& rect) const;
    int getMarkerIndexAt(const QPoint& pos) const; // Получить индекс метки под курсором
    QRectF getMarkerDiamondRect(qint64 samplePos, const QRect& rect) const; // Получить прямоугольник ромбика метки
    void adjustHorizontalOffset(float delta);
    void adjustZoomLevel(float delta);
    QString getPositionText(qint64 position) const;
    QString getBarText(float beatPosition) const;

    QVector<QVector<float>> audioData;
    QVector<BPMAnalyzer::BeatInfo> beats;
    // BeatVisualizer::AnalysisResult beatAnalysis; // Новый результат анализа ударных
    float bpm;
    int sampleRate;
    qint64 playbackPosition;
    qint64 gridStartSample;
    float horizontalOffset;
    float verticalOffset;
    float zoomLevel;
    bool isDragging;
    bool isRightMousePanning;
    QPoint lastMousePos;
    QString colorScheme;
    qint64 loopStartPosition;
    qint64 loopEndPosition;
    float scrollStep;    // 10% от ширины окна
    float zoomStep;      // 20% изменение масштаба
    bool showTimeDisplay;
    bool showBarsDisplay;
    bool showBeatDeviations;
    bool showBeatWaveform; // Показывать силуэт ударных поверх волны (в стиле VirtualDJ)
    bool beatsAligned; // Флаг, указывающий, были ли доли выровнены после анализа
    int beatsPerBar;
    
    QVector<Marker> markers; // Список меток
    int draggingMarkerIndex; // Индекс перетаскиваемой метки (-1 если нет)
    
    WaveformColors colors; // Объект для управления цветами
    // BeatVisualizer::VisualizationSettings beatVisualizationSettings; // Настройки визуализации ударных

    static const int minZoom;
    static const int maxZoom;
    static const int markerHeight;
    static const int markerSpacing;
};

#endif // WAVEFORMVIEW_H 