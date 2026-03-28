#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include <QtCore/QTimer>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QToolTip>
#include <QtGui/QFontMetrics>
#include <QtMultimedia/QAudioBuffer>
#include "bpmanalyzer.h"
#include "waveformcolors.h"
#include "timeutils.h"
#include "beatvisualizer.h"
#include "markerengine.h"
#include "timestretchprocessor.h"
#include "fft_engine.h"

class WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(QWidget *parent = nullptr);

    enum class WaveformRenderMode {
        Peaks,
        Spectrogram
    };

    enum class SpectrogramColorScheme {
        HeatMap,    // чёрный → синий → красный → жёлтый → белый
        Grayscale,  // чёрно-белый
        Cool        // тёмно-синий → голубой → белый
    };

    enum class SpectrogramWindowFunction {
        Rectangular = 0,
        BlackmanHarris,   // рекомендуется по умолчанию
        Hamming,
        Hanning
    };

    struct SpectrogramSettings {
        int windowSize    = 1024;   // размер FFT-окна (256/512/1024/2048)
        int maxFrames     = 512;    // максимальное кол-во временных кадров
        int freqBins      = 256;    // частотных полос на дисплее
        bool logFreqScale = true;   // логарифмическая шкала частот
        bool dbAmplitude  = true;   // dB амплитуда
        int  zeroPadFactor = 2;     // zero-padding (FFT-блок = nextPow2(windowSize)*zeroPadFactor)
        float floorDb     = -90.f;  // нижняя граница dB
        SpectrogramColorScheme    colorScheme    = SpectrogramColorScheme::HeatMap;
        SpectrogramWindowFunction windowFunction = SpectrogramWindowFunction::BlackmanHarris;

        bool operator==(const SpectrogramSettings& o) const {
            return windowSize    == o.windowSize
                && maxFrames     == o.maxFrames
                && freqBins      == o.freqBins
                && logFreqScale  == o.logFreqScale
                && dbAmplitude   == o.dbAmplitude
                && zeroPadFactor == o.zeroPadFactor
                && floorDb       == o.floorDb
                && colorScheme   == o.colorScheme
                && windowFunction == o.windowFunction;
        }
    };

    void setAudioData(const QVector<QVector<float>>& data);
    void setBeatInfo(const QVector<BPMAnalyzer::BeatInfo>& beats);
    QVector<BPMAnalyzer::BeatInfo> getBeatInfo() const { return beats; }
    void setGridStartSample(qint64 sample) { gridStartSample = sample; update(); }
    qint64 getGridStartSample() const { return gridStartSample; }
    void setBPM(float bpm);
    float getBPM() const { return bpm; }
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
    void setShowBeatWaveform(bool show);
    bool getShowBeatWaveform() const;
    void setBeatsAligned(bool aligned);
    bool getBeatsAligned() const;
    void setWaveformRenderMode(WaveformRenderMode mode);
    WaveformRenderMode getWaveformRenderMode() const { return renderMode; }
    void setSpectrogramSettings(const SpectrogramSettings& s);
    SpectrogramSettings getSpectrogramSettings() const { return spectrogramSettings; }

    // Методы для работы с метками
    void addMarker(qint64 position);
    void addMarker(const Marker& marker);
    void removeMarker(int index);
    void sortMarkers();
    QVector<Marker> getMarkers() const { return markers; }
    QVector<Marker> snapMarkersToGrid(const QVector<Marker>& markersIn) const;

    // Метод для получения информации об активном сегменте
    struct ActiveSegmentInfo {
        bool isValid;
        qint64 startTimeMs;
        qint64 endTimeMs;
        float stretchFactor;
        QString startMarkerTime;
        QString endMarkerTime;
    };
    ActiveSegmentInfo getActiveSegmentInfo() const;
    void setMarkers(const QVector<Marker>& newMarkers) {
        markers = newMarkers;
        // Нулевая метка статична в 0:00
        if (!markers.isEmpty() && markers[0].isFixed) {
            markers[0].position = 0;
            markers[0].originalPosition = 0;
        }
        for (Marker& marker : markers) {
            marker.updateTimeFromSamples(sampleRate);
        }
        update();
    }
    void clearMarkers();

    // Метод для обновления исходных данных (используется TimeStretchCommand)
    void updateOriginalData(const QVector<QVector<float>>& newData);

    // Методы для применения растяжения
    // Возвращает структуру с обработанными данными и новыми позициями меток
    // Теперь использует TimeStretchProcessor::StretchResult
    TimeStretchProcessor::StretchResult applyTimeStretch(const QVector<Marker>& markers) const;

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
    void markerDragFinished(); // Сигнал о завершении перетаскивания метки (для обновления воспроизведения)
    void markersChanged(); // Сигнал об изменении набора меток

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

    /// Параметры видимой области волны (сэмплы/пиксель, startSample и т.д.)
    struct ViewportGeometry {
        float samplesPerPixel = 0.0f;
        int visibleSamples = 0;
        int maxStartSample = 0;
        int startSample = 0;
    };
    ViewportGeometry getViewportGeometry(qint64 sampleCount, float viewWidth) const;

private:
    void drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawWaveformChannel(QPainter& painter, const QVector<float>& samples, const QRectF& rect);
    void drawGrid(QPainter& painter, const QRect& rect);
    void drawBeatLines(QPainter& painter, const QRect& rect);
    void drawPlaybackCursor(QPainter& painter, const QRect& rect);
    void drawBarMarkers(QPainter& painter, const QRectF& rect);
    void drawLoopMarkers(QPainter& painter, const QRect& rect);
    void drawMarkers(QPainter& painter, const QRect& rect);
    qint64 snapSampleToGrid(qint64 samplePos) const;

    // Новые методы для визуализации области конца таймлайна и растяжения
    void drawTailArea(QPainter& painter, const QRect& rect);
    void drawTailWaveform(QPainter& painter, const QRect& rect, float startX, float width, float coefficient);
    // void drawStretchIndicator(QPainter& painter, const QRect& rect, float tailCoefficient); // УДАЛЕНО (2026-01-12)
    bool isLastSegment(int markerIndex) const;
    float calculateTailAreaCoefficient() const;

    QPointF sampleToPoint(int sampleIndex, float value, const QRectF& rect) const;
    int getMarkerIndexAt(const QPoint& pos) const; // Получить индекс метки под курсором
    void adjustHorizontalOffset(float delta);
    void adjustZoomLevel(float delta);
    QString getPositionText(qint64 position) const;
    QString getBarText(float beatPosition) const;
    void scheduleUpdate(const QRect& rect = QRect()); // Throttled update для производительности

    // Методы для обработки в реальном времени
    void processRealtimeStretch(); // Обработка аудио в реальном времени при перетаскивании меток
    void scheduleRealtimeProcess(); // Планирование обработки с throttling

    QVector<QVector<float>> audioData;        // Текущие данные для визуализации
    QVector<QVector<float>> originalAudioData; // Исходные данные для пересчета в реальном времени
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
    int beatsPerBar;

    QVector<Marker> markers; // Список меток
    int draggingMarkerIndex; // Индекс перетаскиваемой метки (-1 если нет)

    // Оптимизация производительности: throttling обновлений
    QTimer* updateTimer; // Таймер для отложенных обновлений
    bool pendingUpdate; // Флаг ожидающего обновления
    QRect pendingUpdateRect; // Область ожидающего обновления

    // Throttling для обработки в реальном времени
    bool realtimeProcessPending; // Флаг ожидания обработки в реальном времени

    // Кеширование для tooltip
    QPoint lastTooltipPos; // Последняя позиция мыши для tooltip
    int lastTooltipMarkerIndex; // Последний индекс метки для tooltip

    WaveformColors colors; // Объект для управления цветами

    // Настройки визуализации ударных (используются через BeatVisualizer)
    BeatVisualizer::VisualizationSettings beatVisualizationSettings;
    BeatVisualizer::BeatDeviationColors beatDeviationColors;

    WaveformRenderMode renderMode;
    SpectrogramSettings spectrogramSettings;
    QVector<QImage> spectrogramImages; // по одному изображению на канал
    bool spectrogramDirty;

    static const int minZoom;
    static const int maxZoom;
    static const int markerHeight;
    static const int markerSpacing;
};

#endif // WAVEFORMVIEW_H
