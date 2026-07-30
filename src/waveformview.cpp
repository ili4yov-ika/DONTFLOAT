#include "../include/waveformview.h"
#include "../include/beatvisualizer.h"
#include "../include/timeutils.h"
#include "../include/timestretchprocessor.h"
#include <QtCore/QtMath>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QTimer>
#include <QtCore/QtGlobal>
#include <QtGui/QPainter>
#include <QtGui/QPolygon>
#include <QtGui/QPainterPath>
#include <QtGui/QBrush>
#include <QtWidgets/QMenu>
#include <QtCore/QTime>
#include <QtCore/QDebug>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QApplication>
#include <QtConcurrent/QtConcurrent>
#include <QtGui/QResizeEvent>
#include <thread>
#include <cmath>
#include <algorithm>

// Определение статических констант
const int WaveformView::minZoom = 1;
const int WaveformView::maxZoom = 1000;
const int WaveformView::markerHeight = 20;  // Высота области маркеров
const int WaveformView::markerSpacing = 60;  // Минимальное расстояние между маркерами в пикселях
const int WaveformView::keyStripHeight = 16;  // Высота полосы модуляции (тональности по тактам)

// Минимальный сегмент между метками: 50 мс (в таком сегменте нельзя создавать новые метки)
static const qint64 MIN_MARKER_SEGMENT_MS = 50;

namespace {

QRgb spectrogramColorForValue(float v, WaveformView::SpectrogramColorScheme scheme)
{
    v = std::clamp(v, 0.f, 1.f);
    int r = 0;
    int g = 0;
    int b = 0;
    switch (scheme) {
    case WaveformView::SpectrogramColorScheme::HeatMap:
        if (v < 0.25f) {
            b = int(v * 4.f * 255.f);
        } else if (v < 0.5f) {
            const float t = (v - 0.25f) * 4.f;
            r = int(t * 255.f);
            b = int((1.f - t) * 255.f);
        } else if (v < 0.75f) {
            const float t = (v - 0.5f) * 4.f;
            r = 255;
            g = int(t * 255.f);
        } else {
            const float t = (v - 0.75f) * 4.f;
            r = 255;
            g = 255;
            b = int(t * 255.f);
        }
        break;
    case WaveformView::SpectrogramColorScheme::Grayscale:
        r = g = b = int(v * 255.f);
        break;
    case WaveformView::SpectrogramColorScheme::Cool:
        r = int(v * v * 255.f);
        g = int(v * 180.f);
        b = 255;
        break;
    }
    return qRgb(r, g, b);
}

QVector<QImage> computeSpectrogramImages(const QVector<QVector<float>>& audioDataCopy,
                                         int sampleRateCopy,
                                         const WaveformView::SpectrogramSettings& settingsCopy)
{
    QVector<QImage> result;
    result.resize(audioDataCopy.size());

    const int windowSize = settingsCopy.windowSize;
    const int maxFrames = settingsCopy.maxFrames;
    const int freqBins = settingsCopy.freqBins;
    const int zeroPadFactor = qMax(1, settingsCopy.zeroPadFactor);
    const bool logScale = settingsCopy.logFreqScale;
    const bool useDb = settingsCopy.dbAmplitude;
    const float floorDb = settingsCopy.floorDb;

    DFEngine::WindowFunction winType = DFEngine::WindowFunction::BlackmanHarris;
    switch (settingsCopy.windowFunction) {
    case WaveformView::SpectrogramWindowFunction::Rectangular:
        winType = DFEngine::WindowFunction::Rectangular;
        break;
    case WaveformView::SpectrogramWindowFunction::BlackmanHarris:
        winType = DFEngine::WindowFunction::BlackmanHarris;
        break;
    case WaveformView::SpectrogramWindowFunction::Hamming:
        winType = DFEngine::WindowFunction::Hamming;
        break;
    case WaveformView::SpectrogramWindowFunction::Hanning:
        winType = DFEngine::WindowFunction::Hanning;
        break;
    }

    std::vector<float> window;
    DFEngine::precomputeWindow(window, windowSize, winType);

    for (int ch = 0; ch < audioDataCopy.size(); ++ch) {
        const QVector<float>& samples = audioDataCopy[ch];
        if (samples.size() <= windowSize) {
            result[ch] = QImage();
            continue;
        }

        const int totalSamples = samples.size();
        const int hop = qMax(1, (totalSamples - windowSize) / maxFrames);
        const int frameCount = qMax(1, (totalSamples - windowSize) / hop);

        const unsigned fftSize = DFEngine::nextPow2(windowSize) * zeroPadFactor;
        const int linearBins = int(fftSize / 2) + 1;

        std::vector<std::vector<float>> colData(frameCount, std::vector<float>(freqBins, 0.f));
        std::vector<float> linearMag;
        std::vector<float> displayMag;

        for (int frame = 0; frame < frameCount; ++frame) {
            const int start = frame * hop;
            if (start + windowSize > totalSamples) {
                break;
            }

            DFEngine::realFFT(samples.constData() + start, windowSize,
                              window, zeroPadFactor, linearMag);

            if (useDb) {
                DFEngine::normalizeDb(linearMag, floorDb);
            } else {
                DFEngine::normalizeLinear(linearMag);
            }

            if (logScale) {
                DFEngine::logFreqCompress(linearMag, displayMag,
                                          freqBins, float(sampleRateCopy));
            } else {
                displayMag.resize(freqBins);
                for (int k = 0; k < freqBins; ++k) {
                    const int srcIdx = k * (linearBins - 1) / qMax(freqBins - 1, 1);
                    displayMag[k] = (srcIdx < int(linearMag.size())) ? linearMag[srcIdx] : 0.f;
                }
            }

            for (int k = 0; k < freqBins; ++k) {
                colData[frame][k] = (k < int(displayMag.size())) ? displayMag[k] : 0.f;
            }
        }

        QImage img(frameCount, freqBins, QImage::Format_RGB32);
        for (int y = 0; y < freqBins; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            const int bin = freqBins - 1 - y;
            for (int x = 0; x < frameCount; ++x) {
                line[x] = spectrogramColorForValue(colData[x][bin], settingsCopy.colorScheme);
            }
        }
        result[ch] = img;
    }

    return result;
}

} // namespace

/**
 * Преобразует позицию в исходном аудио в позицию отображения с учётом меток растяжения.
 * Когда метки перемещены (position != originalPosition), аудио визуально растянуто,
 * и силуэты ударных должны следовать за этим маппингом.
 */
static qint64 mapOriginalSampleToDisplay(qint64 originalPos,
                                         const QVector<Marker>& markers,
                                         qint64 totalOriginalSamples)
{
    if (markers.size() < 2 || totalOriginalSamples <= 0) {
        return originalPos;
    }

    QVector<Marker> sorted = markers;
    std::sort(sorted.begin(), sorted.end(), [](const Marker& a, const Marker& b) {
        return a.originalPosition < b.originalPosition;
    });

    // До первой метки — линейная интерполяция от 0 до первой метки
    if (originalPos <= sorted.first().originalPosition) {
        const Marker& m1 = sorted.first();
        if (m1.originalPosition <= 0) return originalPos;
        double t = double(originalPos) / double(m1.originalPosition);
        return qint64(t * m1.position);
    }

    // После последней метки — линейная экстраполяция по последнему сегменту
    if (originalPos >= sorted.last().originalPosition) {
        const Marker& m0 = sorted[sorted.size() - 2];
        const Marker& m1 = sorted[sorted.size() - 1];
        qint64 origLen = m1.originalPosition - m0.originalPosition;
        qint64 dispLen = m1.position - m0.position;
        if (origLen <= 0) return originalPos;
        double t = double(originalPos - m1.originalPosition) / double(origLen);
        return m1.position + qint64(t * dispLen);
    }

    // Между метками — линейная интерполяция
    for (int i = 0; i < sorted.size() - 1; ++i) {
        const Marker& m0 = sorted[i];
        const Marker& m1 = sorted[i + 1];
        if (originalPos >= m0.originalPosition && originalPos < m1.originalPosition) {
            qint64 origLen = m1.originalPosition - m0.originalPosition;
            qint64 dispLen = m1.position - m0.position;
            if (origLen <= 0) return originalPos;
            double t = double(originalPos - m0.originalPosition) / double(origLen);
            return m0.position + qint64(t * dispLen);
        }
    }

    return originalPos;
}

static bool markersChangeTimeline(const QVector<Marker>& markers)
{
    for (const Marker& m : markers) {
        if (m.position != m.originalPosition) {
            return true;
        }
    }
    return false;
}

static qint64 estimateStretchedLength(const QVector<Marker>& markers, qint64 originalSize)
{
    if (markers.size() < 2 || originalSize <= 0) {
        return originalSize;
    }

    QVector<Marker> sorted = markers;
    std::sort(sorted.begin(), sorted.end(), [](const Marker& a, const Marker& b) {
        return a.originalPosition < b.originalPosition;
    });

    qint64 displayLen = sorted.last().position;
    const qint64 origTail = originalSize - sorted.last().originalPosition;
    if (origTail > 0 && sorted.size() >= 2) {
        const Marker& m0 = sorted[sorted.size() - 2];
        const Marker& m1 = sorted.last();
        const qint64 origSeg = m1.originalPosition - m0.originalPosition;
        const qint64 dispSeg = m1.position - m0.position;
        if (origSeg > 0 && dispSeg > 0) {
            const float tailFactor = qMax(0.1f, float(dispSeg) / float(origSeg));
            displayLen += qint64(origTail * tailFactor);
        } else {
            displayLen += origTail;
        }
    } else if (origTail > 0) {
        displayLen += origTail;
    }

    return qMax(displayLen, qint64(1));
}

static qint64 mapDisplaySampleToOriginal(qint64 displayPos,
                                         const QVector<Marker>& markers,
                                         qint64 totalOriginalSamples)
{
    if (markers.size() < 2 || totalOriginalSamples <= 0) {
        return qBound(qint64(0), displayPos, totalOriginalSamples - 1);
    }

    QVector<Marker> sorted = markers;
    std::sort(sorted.begin(), sorted.end(), [](const Marker& a, const Marker& b) {
        return a.originalPosition < b.originalPosition;
    });

    if (displayPos <= sorted.first().position) {
        const Marker& m1 = sorted.first();
        if (m1.position <= 0) {
            return qBound(qint64(0), displayPos, totalOriginalSamples - 1);
        }
        const double t = double(displayPos) / double(m1.position);
        return qBound(qint64(0), qint64(t * m1.originalPosition), totalOriginalSamples - 1);
    }

    const qint64 displayTailStart = sorted.last().position;
    const qint64 displayTotal = estimateStretchedLength(markers, totalOriginalSamples);
    if (displayPos >= displayTailStart) {
        if (sorted.size() < 2) {
            return qBound(qint64(0), displayPos, totalOriginalSamples - 1);
        }
        const Marker& m0 = sorted[sorted.size() - 2];
        const Marker& m1 = sorted.last();
        const qint64 origLen = m1.originalPosition - m0.originalPosition;
        const qint64 dispLen = m1.position - m0.position;
        const qint64 origTail = totalOriginalSamples - m1.originalPosition;
        const qint64 dispTail = qMax<qint64>(1, displayTotal - displayTailStart);
        if (origLen > 0 && dispLen > 0 && origTail > 0) {
            const double t = double(displayPos - displayTailStart) / double(dispTail);
            return qBound(qint64(0),
                          m1.originalPosition + qint64(t * origTail),
                          totalOriginalSamples - 1);
        }
        return qBound(qint64(0), m1.originalPosition, totalOriginalSamples - 1);
    }

    for (int i = 0; i < sorted.size() - 1; ++i) {
        const Marker& m0 = sorted[i];
        const Marker& m1 = sorted[i + 1];
        if (displayPos >= m0.position && displayPos < m1.position) {
            const qint64 origLen = m1.originalPosition - m0.originalPosition;
            const qint64 dispLen = m1.position - m0.position;
            if (origLen <= 0 || dispLen <= 0) {
                return qBound(qint64(0), displayPos, totalOriginalSamples - 1);
            }
            const double t = double(displayPos - m0.position) / double(dispLen);
            return qBound(qint64(0),
                          m0.originalPosition + qint64(t * origLen),
                          totalOriginalSamples - 1);
        }
    }

    return qBound(qint64(0), displayPos, totalOriginalSamples - 1);
}

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , bpm(120.0f)
    , sampleRate(44100)
    , playbackPosition(0)
    , gridStartSample(0)
    , horizontalOffset(0.0f)
    , verticalOffset(0.0f)
    , zoomLevel(1.0f)
    , isDragging(false)
    , isRightMousePanning(false)
    , isGridDragging(false)
    , gridDragAnchorSample(0)
    , gridDragAnchorMouseX(0.f)
    , loopStartPosition(0)
    , loopEndPosition(0)
    , scrollStep(defaultScrollStep)
    , zoomStep(defaultZoomStep)
    , showTimeDisplay(true)
    , showBarsDisplay(false)
    , beatsPerBar(4)
    , draggingMarkerIndex(-1)
    , updateTimer(nullptr)
    , pendingUpdate(false)
    , realtimeStretchDirty(false)
    , realtimeStretchJobActive(false)
    , realtimeStretchShuttingDown(false)
    , realtimeStretchJobsRunning(0)
    , audioSourceGeneration(0)
    , realtimeJobGeneration(0)
    , lastTooltipMarkerIndex(-1)
    , renderMode(WaveformRenderMode::Peaks)
    , spectrogramDirty(true)
    // spectrogramImages инициализируется пустым QVector автоматически
{
    setMinimumHeight(100);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // Разрешаем получение фокуса для обработки клавиш

    // Инициализируем таймер для throttling обновлений (16ms ≈ 60 FPS)
    updateTimer = new QTimer(this);
    updateTimer->setSingleShot(true);
    updateTimer->setInterval(16); // ~60 FPS
    connect(updateTimer, &QTimer::timeout, this, [this]() {
        if (pendingUpdate) {
            if (pendingUpdateRect.isValid()) {
                QWidget::update(pendingUpdateRect);
            } else {
                QWidget::update();
            }
            pendingUpdate = false;
            pendingUpdateRect = QRect();
        }
    });

    spectrogramWatcher = new QFutureWatcher<QVector<QImage>>(this);
    connect(spectrogramWatcher, &QFutureWatcher<QVector<QImage>>::finished,
            this, &WaveformView::onSpectrogramReady);
}

WaveformView::~WaveformView()
{
    realtimeStretchShuttingDown = true;
    ++audioSourceGeneration;

    if (spectrogramWatcher) {
        spectrogramWatcher->cancel();
        spectrogramWatcher->waitForFinished();
    }

    QDeadlineTimer deadline(60000);
    while (realtimeStretchJobsRunning.load() > 0 && !deadline.hasExpired()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}

bool WaveformView::isRealtimeStretchRunning() const
{
    return realtimeStretchJobActive || realtimeStretchJobsRunning.load() > 0;
}

void WaveformView::drainPendingRealtimeStretch(int timeoutMs)
{
    if (!isRealtimeStretchRunning()) {
        return;
    }

    QDeadlineTimer deadline(timeoutMs);
    while (isRealtimeStretchRunning() && !deadline.hasExpired()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}

void WaveformView::setAudioData(const QVector<QVector<float>>& channels)
{
    // Проверка на пустые данные
    if (channels.isEmpty()) {
        return;
    }

    // Проверка на корректность данных
    for (const auto& channel : channels) {
        if (channel.isEmpty()) {
            return;
        }
    }

    // Нормализация данных
    audioData.clear();
    for (const auto& channel : channels) {
        QVector<float> normalizedChannel;
        normalizedChannel.reserve(channel.size());

        // Находим максимальное значение для нормализации
        float maxValue = 0.0f;
        for (float sample : channel) {
            maxValue = qMax(maxValue, qAbs(sample));
        }

        // Нормализуем значения
        if (maxValue > 0.0f) {
            for (float sample : channel) {
                normalizedChannel.append(sample / maxValue);
            }
        } else {
            normalizedChannel = channel;
        }

        audioData.append(normalizedChannel);
    }

    // Сохраняем исходные данные для пересчета в реальном времени
    originalAudioData = audioData;
    ++audioSourceGeneration; // Результаты фоновых задач со старыми данными будут отброшены
    realtimeStretchDirty = false;

    // Сброс кеша спектрограммы
    spectrogramImages.clear();
    spectrogramDirty = true;
    invalidateWavePixmapCache();
    markMarkersCacheDirty();

    // Сбрасываем масштаб и смещение
    zoomLevel = 1.0f;
    horizontalOffset = 0.0f;
    gridStartSample = 0;
    emit zoomChanged(zoomLevel);

    // Обновляем время всех меток при загрузке нового аудио
    for (Marker& marker : markers) {
        marker.updateTimeFromSamples(sampleRate);
    }

    // Запускаем анализ ударных (временно отключено)
    // analyzeBeats();

    update();
}

void WaveformView::setBPM(float newBpm)
{
    bpm = newBpm;
    update();
}

void WaveformView::setZoomLevel(float zoom)
{
    float newZoom = qBound(float(minZoom), zoom, float(maxZoom));
    if (!qFuzzyCompare(newZoom, zoomLevel)) {
        zoomLevel = newZoom;
        invalidateWavePixmapCache();
        emit zoomChanged(zoomLevel);
        update();
    }
}

void WaveformView::zoomAtPixelX(int angleDeltaY, float pixelX)
{
    if (audioData.isEmpty()) {
        return;
    }

    float delta = angleDeltaY / 120.f;
    float oldZoom = zoomLevel;
    float newZoom = oldZoom;

    if (delta > 0) {
        newZoom *= zoomStep;
    } else if (delta < 0) {
        newZoom /= zoomStep;
    } else {
        return;
    }

    newZoom = qBound(float(minZoom), newZoom, float(maxZoom));
    if (qFuzzyCompare(newZoom, oldZoom)) {
        return;
    }

    const float mouseX = qBound(0.0f, pixelX, float(width()));
    float samplesPerPixel = float(audioData[0].size()) / (width() * oldZoom);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    qint64 mouseSample = startSample + qint64(mouseX * samplesPerPixel);

    zoomLevel = newZoom;

    float newSamplesPerPixel = float(audioData[0].size()) / (width() * newZoom);
    int newVisibleSamples = int(width() * newSamplesPerPixel);
    int newMaxStartSample = qMax(0, audioData[0].size() - newVisibleSamples);

    float newOffset = 0.0f;
    if (newMaxStartSample > 0) {
        newOffset = float(mouseSample - mouseX * newSamplesPerPixel) / newMaxStartSample;
        newOffset = qBound(0.0f, newOffset, 1.0f);
    }

    horizontalOffset = newOffset;
    invalidateWavePixmapCache();
    emit zoomChanged(zoomLevel);
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::setSampleRate(int rate)
{
    sampleRate = rate;

    // Обновляем время всех меток при изменении sampleRate
    for (Marker& marker : markers) {
        marker.updateTimeFromSamples(sampleRate);
    }

    update();
}

WaveformView::ViewportGeometry WaveformView::getViewportGeometry(qint64 sampleCount, float viewWidth) const
{
    ViewportGeometry v;
    v.samplesPerPixel = float(sampleCount) / (viewWidth * zoomLevel);
    v.visibleSamples = int(viewWidth * v.samplesPerPixel);
    v.maxStartSample = qMax(0, int(sampleCount) - v.visibleSamples);
    v.startSample = int(horizontalOffset * v.maxStartSample);
    return v;
}

void WaveformView::markMarkersCacheDirty()
{
    markersSortDirty = true;
}

void WaveformView::ensureSortedMarkersCache()
{
    if (!markersSortDirty) {
        return;
    }
    cachedSortedMarkers = markers;
    std::sort(cachedSortedMarkers.begin(), cachedSortedMarkers.end(),
              [](const Marker& a, const Marker& b) {
                  return a.position < b.position;
              });
    markersSortDirty = false;
}

void WaveformView::invalidateWavePixmapCache()
{
    wavePixmapDirty = true;
}

bool WaveformView::isWavePixmapCacheValid() const
{
    if (wavePixmapDirty || cachedWavePixmap.isNull()) {
        return false;
    }
    if (cachedWaveSize != size()) {
        return false;
    }
    if (cachedWaveAudioGeneration != audioSourceGeneration) {
        return false;
    }
    if (cachedWaveUsesWarpedPreview != needsWarpedWaveformPreview()) {
        return false;
    }
    const int zoomKey = int(zoomLevel * 1000.f);
    const int hOffsetKey = int(horizontalOffset * 1000.f);
    const int vOffsetKey = int(verticalOffset * 1000.f);
    return cachedWaveZoomKey == zoomKey
        && cachedWaveHorizontalOffsetKey == hOffsetKey
        && cachedWaveVerticalOffsetKey == vOffsetKey;
}

void WaveformView::regenerateWavePixmap()
{
    if (audioData.isEmpty() || width() <= 0 || height() <= 0) {
        cachedWavePixmap = QPixmap();
        wavePixmapDirty = true;
        return;
    }

    cachedWavePixmap = QPixmap(size());
    cachedWavePixmap.fill(colors.getBackgroundColor());

    QPainter pixmapPainter(&cachedWavePixmap);
    pixmapPainter.setRenderHint(QPainter::Antialiasing, false);

    const float channelHeight = float(height()) / float(qMax(1, audioData.size()));
    for (int i = 0; i < audioData.size(); ++i) {
        const QRectF channelRect(0, i * channelHeight, width(), channelHeight);
        if (needsWarpedWaveformPreview() && i >= originalAudioData.size()) {
            continue;
        }
        const QVector<float>& sourceSamples =
            needsWarpedWaveformPreview() ? originalAudioData[i] : audioData[i];
        if (sourceSamples.isEmpty()) {
            continue;
        }
        if (needsWarpedWaveformPreview()) {
            drawWarpedWaveformPreview(pixmapPainter, sourceSamples, channelRect);
        } else {
            drawWaveform(pixmapPainter, sourceSamples, channelRect);
        }
    }

    cachedWaveZoomKey = int(zoomLevel * 1000.f);
    cachedWaveHorizontalOffsetKey = int(horizontalOffset * 1000.f);
    cachedWaveVerticalOffsetKey = int(verticalOffset * 1000.f);
    cachedWaveSize = size();
    cachedWaveAudioGeneration = audioSourceGeneration;
    cachedWaveUsesWarpedPreview = needsWarpedWaveformPreview();
    wavePixmapDirty = false;
}

void WaveformView::scheduleSpectrogramRegeneration()
{
    if (renderMode != WaveformRenderMode::Spectrogram || audioData.isEmpty() || !spectrogramWatcher) {
        return;
    }
    if (spectrogramComputationInProgress.load()) {
        return;
    }

    spectrogramComputationInProgress = true;
    spectrogramDirty = false;
    const QVector<QVector<float>> audioCopy = audioData;
    const int sampleRateCopy = sampleRate;
    const SpectrogramSettings settingsCopy = spectrogramSettings;

    const QFuture<QVector<QImage>> future = QtConcurrent::run(
        [audioCopy, sampleRateCopy, settingsCopy]() {
            return computeSpectrogramImages(audioCopy, sampleRateCopy, settingsCopy);
        });
    spectrogramWatcher->setFuture(future);
}

void WaveformView::onSpectrogramReady()
{
    spectrogramComputationInProgress = false;
    if (realtimeStretchShuttingDown || !spectrogramWatcher) {
        return;
    }
    if (spectrogramWatcher->isCanceled()) {
        if (spectrogramDirty) {
            scheduleSpectrogramRegeneration();
        }
        return;
    }

    if (spectrogramDirty) {
        scheduleSpectrogramRegeneration();
        return;
    }

    spectrogramImages = spectrogramWatcher->result();
    update();
}

void WaveformView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    invalidateWavePixmapCache();
}

void WaveformView::setMarkers(const QVector<Marker>& newMarkers)
{
    markers = newMarkers;
    if (!markers.isEmpty() && markers[0].isFixed) {
        markers[0].position = 0;
        markers[0].originalPosition = 0;
    }
    for (Marker& marker : markers) {
        marker.updateTimeFromSamples(sampleRate);
    }
    markMarkersCacheDirty();
    invalidateWavePixmapCache();
    update();
    emit markersChanged();
}

void WaveformView::setBeatInfo(const QVector<BPMAnalyzer::BeatInfo>& newBeats)
{
    beats = newBeats;
    if (!beats.isEmpty()) {
        gridStartSample = beats.first().position;
    }
    update();
}

void WaveformView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    // Отключаем антиалиасинг для лучшей производительности (включаем только для меток)
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Фон
    painter.fillRect(rect(), colors.getBackgroundColor());

    // Рисуем сетку
    drawGrid(painter, rect());

    // Режим отображения: пики или спектрограмма
    if (!audioData.isEmpty()) {
        // Геометрия текущего окна по сэмплам
        ViewportGeometry vp = getViewportGeometry(displaySampleCount(), width());

        if (renderMode == WaveformRenderMode::Spectrogram) {
            if (spectrogramDirty && !spectrogramComputationInProgress.load()) {
                scheduleSpectrogramRegeneration();
            }

            // ---- Отрисовка по каналам ----
            const int numCh = audioData.size();
            const float channelHeight = float(height()) / float(qMax(1, numCh));
            const qint64 totalSamples = audioData[0].size();

            auto sampleToFrame = [](qint64 sample, qint64 total, int frames) -> int {
                if (total <= 0) return 0;
                int f = int(double(sample) / double(total) * frames);
                return qBound(0, f, frames - 1);
            };

            for (int ch = 0; ch < numCh; ++ch) {
                if (ch >= spectrogramImages.size()) break;
                const QImage& img = spectrogramImages[ch];
                if (img.isNull()) continue;

                int frameCount = img.width();
                int freqBinsH  = img.height();

                QRectF chRect(0, ch * channelHeight, width(), channelHeight);

                int startFrame = sampleToFrame(vp.startSample, totalSamples, frameCount);
                qint64 endSample = qint64(vp.startSample) + qint64(vp.visibleSamples);
                int endFrame = sampleToFrame(endSample, totalSamples, frameCount) + 1;
                endFrame = qBound(startFrame + 1, endFrame, frameCount);
                int subWidth = qMax(1, endFrame - startFrame);

                QImage sub = img.copy(startFrame, 0, subWidth, freqBinsH);
                painter.drawImage(chRect, sub);
            }

            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setPen(QPen(QColor(80, 80, 80), 1));
            for (int i = 1; i < numCh; ++i) {
                float y = i * channelHeight;
                painter.drawLine(QPointF(0, y), QPointF(width(), y));
            }
        } else {
            if (!isWavePixmapCacheValid()) {
                regenerateWavePixmap();
            }
            if (!cachedWavePixmap.isNull()) {
                painter.drawPixmap(0, 0, cachedWavePixmap);
            }

            const float channelHeight = height() / float(audioData.size());
            for (int i = 0; i < audioData.size(); ++i) {
                QRectF channelRect(0, i * channelHeight, width(), channelHeight);
                if (needsWarpedWaveformPreview() && i >= originalAudioData.size()) {
                    continue;
                }
                const QVector<float>& sourceSamples =
                    needsWarpedWaveformPreview() ? originalAudioData[i] : audioData[i];
                if (sourceSamples.isEmpty()) {
                    continue;
                }

                if (beatVisualizationSettings.showBeatWaveform && !beats.isEmpty()) {
                    QVector<BPMAnalyzer::BeatInfo> displayBeats = beats;
                    if (!originalAudioData.isEmpty() && markers.size() >= 2) {
                        qint64 refSize = originalAudioData[0].size();
                        for (BPMAnalyzer::BeatInfo& b : displayBeats) {
                            b.position = mapOriginalSampleToDisplay(b.position, markers, refSize);
                        }
                    }
                    BeatVisualizer::drawBeatWaveform(painter, sourceSamples, displayBeats,
                                                     channelRect, sampleRate,
                                                     vp.samplesPerPixel, vp.startSample,
                                                     beatVisualizationSettings);
                }
            }
        }
    }

    // Рисуем линии тактов
    drawBeatLines(painter, rect());

    // Рисуем маркеры тактов
    QRectF markerRect(0, 0, width(), markerHeight);
    drawBarMarkers(painter, markerRect);

    // Рисуем полосу модуляции (потактовые тональности), как в Melodyne
    QRectF keyStripRect(0, markerHeight, width(), keyStripHeight);
    drawKeyModulation(painter, keyStripRect);

    // Рисуем курсор воспроизведения
    drawPlaybackCursor(painter, rect());

    // Рисуем маркеры цикла
    drawLoopMarkers(painter, rect());

    // ===== НОВОЕ: Рисуем область конца таймлайна =====
    // При сжатии (coefficient < 1.0) рисуем область конца справа
    drawTailArea(painter, rect());

    // Рисуем метки ПОСЛЕ всех остальных элементов, чтобы они были поверх
    // ВАЖНО: Последняя метка может быть за границей виджета при растяжении
    drawMarkers(painter, rect());
}

void WaveformView::drawWaveform(QPainter& painter, const QVector<float>& samples, const QRectF& rect)
{
    if (samples.isEmpty()) return;

    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(samples.size()) / (rect.width() * zoomLevel);

    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, samples.size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);

    // Применяем вертикальное смещение
    QRectF adjustedRect = rect;
    adjustedRect.translate(0, -verticalOffset * rect.height());

    const float centerY = adjustedRect.center().y();
    const float halfHeight = adjustedRect.height() * 0.5f;

    for (int x = 0; x < rect.width(); ++x) {
        int currentSample = startSample + int(x * samplesPerPixel);
        int nextSample = startSample + int((x + 1) * samplesPerPixel);

        if (currentSample >= samples.size()) break;

        // Оптимизация: используем шаг для уменьшения количества итераций
        // при большом количестве сэмплов на пиксель (максимум 200 проверок на пиксель)
        float minValue = 0, maxValue = 0;
        int sampleCount = qMin(nextSample, samples.size()) - currentSample;
        int step = qMax(1, sampleCount / 200);

        for (int s = currentSample; s < qMin(nextSample, samples.size()); s += step) {
            minValue = qMin(minValue, samples[s]);
            maxValue = qMax(maxValue, samples[s]);
        }

        // Определяем цвет в зависимости от частоты
        float frequency = qAbs(maxValue - minValue);
        QColor waveColor;
        if (frequency < 0.3f) {
            waveColor = colors.getLowColor();
        } else if (frequency < 0.6f) {
            waveColor = colors.getMidColor();
        } else {
            waveColor = colors.getHighColor();
        }

        painter.setPen(waveColor);

        // Рисуем вертикальную линию от минимума до максимума
        float topY = centerY - (maxValue * halfHeight);
        float bottomY = centerY - (minValue * halfHeight);
        painter.drawLine(QPointF(adjustedRect.x() + x, topY),
                        QPointF(adjustedRect.x() + x, bottomY));
    }
}

bool WaveformView::needsWarpedWaveformPreview() const
{
    if (originalAudioData.isEmpty() || audioData.isEmpty() || markers.size() < 2) {
        return false;
    }
    if (originalAudioData[0].isEmpty() || audioData[0].isEmpty()) {
        return false;
    }
    if (originalAudioData.size() != audioData.size()) {
        return false;
    }
    if (!markersChangeTimeline(markers)) {
        return false;
    }
    // Полноценное превью уже применено — рисуем готовые сэмплы
    if (audioData[0].size() != originalAudioData[0].size()) {
        return false;
    }
    return true;
}

qint64 WaveformView::displaySampleCount() const
{
    if (audioData.isEmpty() || audioData[0].isEmpty()) {
        return 0;
    }
    const qint64 originalSize =
        originalAudioData.isEmpty() ? audioData[0].size() : originalAudioData[0].size();
    if (needsWarpedWaveformPreview()) {
        return estimateStretchedLength(markers, originalSize);
    }
    return audioData[0].size();
}

bool WaveformView::hasTimelineStretch() const
{
    return markersChangeTimeline(markers);
}

void WaveformView::drawWarpedWaveformPreview(QPainter& painter,
                                             const QVector<float>& samples,
                                             const QRectF& rect)
{
    if (samples.isEmpty()) {
        return;
    }

    const qint64 displayLength = estimateStretchedLength(markers, samples.size());
    float samplesPerPixel = float(displayLength) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, int(displayLength) - visibleSamples);
    int startDisplaySample = int(horizontalOffset * maxStartSample);

    QRectF adjustedRect = rect;
    adjustedRect.translate(0, -verticalOffset * rect.height());

    const float centerY = adjustedRect.center().y();
    const float halfHeight = adjustedRect.height() * 0.5f;

    for (int x = 0; x < int(rect.width()); ++x) {
        const qint64 displayStart = startDisplaySample + qint64(x * samplesPerPixel);
        const qint64 displayEnd = startDisplaySample + qint64((x + 1) * samplesPerPixel);

        const qint64 origStart = mapDisplaySampleToOriginal(displayStart, markers, samples.size());
        const qint64 origEnd = mapDisplaySampleToOriginal(qMax(displayStart, displayEnd - 1),
                                                          markers,
                                                          samples.size());

        float minValue = 0.0f;
        float maxValue = 0.0f;
        const qint64 from = qBound(qint64(0), qMin(origStart, origEnd), qint64(samples.size() - 1));
        const qint64 to = qBound(qint64(0), qMax(origStart, origEnd), qint64(samples.size() - 1));
        const qint64 span = qMax<qint64>(1, to - from + 1);
        const qint64 step = qMax<qint64>(1, span / 200);

        for (qint64 s = from; s <= to; s += step) {
            const int idx = int(s);
            if (idx < 0 || idx >= samples.size()) {
                continue;
            }
            minValue = qMin(minValue, samples[idx]);
            maxValue = qMax(maxValue, samples[idx]);
        }

        const float frequency = qAbs(maxValue - minValue);
        QColor waveColor;
        if (frequency < 0.3f) {
            waveColor = colors.getLowColor();
        } else if (frequency < 0.6f) {
            waveColor = colors.getMidColor();
        } else {
            waveColor = colors.getHighColor();
        }

        painter.setPen(waveColor);
        const float topY = centerY - (maxValue * halfHeight);
        const float bottomY = centerY - (minValue * halfHeight);
        painter.drawLine(QPointF(adjustedRect.x() + x, topY),
                         QPointF(adjustedRect.x() + x, bottomY));
    }
}

void WaveformView::drawGrid(QPainter& painter, const QRect& rect)
{
    // Горизонтальные линии для разделения каналов
    painter.setPen(QPen(colors.getGridColor()));
    float channelHeight = rect.height() / float(qMax(1, audioData.size()));
    for (int i = 1; i < audioData.size(); ++i) {
        float y = i * channelHeight;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
}

void WaveformView::drawBeatLines(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty()) return;

    float samplesPerBeat = (60.0f * sampleRate) / bpm;
    // Длина такта в четвертях: 4/4->4, 3/4->3, 2/4->2, 1/4->1, 6/8->3, 12/8->6
    float barLengthInQuarters = (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    // Число долей в такте — по знаменателю: X/4 → 4 удара, X/8 → 8 ударов
    int subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
    float samplesPerSubdivision = samplesPerBar / float(subdivisionsPerBar);

    ViewportGeometry vp = getViewportGeometry(displaySampleCount(), rect.width());

    int firstSubdivision = 0;
    if (gridStartSample > 0) {
        float subsFromGrid = float(vp.startSample - gridStartSample) / samplesPerSubdivision;
        firstSubdivision = int(qFloor(subsFromGrid));
    } else {
        firstSubdivision = int(vp.startSample / samplesPerSubdivision);
    }

    for (int sub = firstSubdivision; ; ++sub) {
        qint64 samplePos = gridStartSample > 0
            ? qint64(gridStartSample + sub * samplesPerSubdivision)
            : qint64(sub * samplesPerSubdivision);
        if (samplePos < vp.startSample) continue;

        float x = (samplePos - vp.startSample) / vp.samplesPerPixel;
        if (x >= rect.width()) break;

        bool isStrongBeat = (sub % subdivisionsPerBar) == 0;
        if (isStrongBeat) {
            painter.setPen(QPen(colors.getBeatColor(), 2.0));
        } else {
            painter.setPen(QPen(colors.getBeatColor(), 1.0));
        }
        painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));
    }
}

void WaveformView::applyGridStartSample(qint64 newGrid, bool moveMarkers)
{
    if (audioData.isEmpty()) {
        return;
    }

    const qint64 maxGrid = qMax<qint64>(0, audioData[0].size() - 1);
    newGrid = qBound<qint64>(0, newGrid, maxGrid);
    const qint64 delta = newGrid - gridStartSample;
    if (delta == 0) {
        return;
    }

    gridStartSample = newGrid;

    if (moveMarkers) {
        for (Marker& m : markers) {
            if (m.isFixed || m.isEndMarker) {
                continue;
            }
            m.position = qBound<qint64>(0, m.position + delta, maxGrid);
            m.originalPosition = m.position;
            m.updateTimeFromSamples(sampleRate);
        }
        markMarkersCacheDirty();
        invalidateWavePixmapCache();
        emit markersChanged();
    }

    emit gridStartChanged(gridStartSample);
    update();
}

void WaveformView::shiftGridBySamples(qint64 sampleDelta, bool moveMarkers)
{
    if (sampleDelta == 0 || audioData.isEmpty()) {
        return;
    }
    applyGridStartSample(gridStartSample + sampleDelta, moveMarkers);
}

qint64 WaveformView::snapSampleToGrid(qint64 samplePos) const
{
    if (audioData.isEmpty() || sampleRate <= 0 || bpm <= 0.0f) {
        return samplePos;
    }

    float samplesPerBeat = (60.0f * sampleRate) / bpm;
    float barLengthInQuarters = (beatsPerBar == 6) ? 3.f
                            : (beatsPerBar == 12) ? 6.f
                            : float(qMax(1, beatsPerBar));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    int subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
    float samplesPerSubdivision = samplesPerBar / float(subdivisionsPerBar);

    float subsFromStart;
    if (gridStartSample > 0) {
        subsFromStart = float(samplePos - gridStartSample) / samplesPerSubdivision;
    } else {
        subsFromStart = float(samplePos) / samplesPerSubdivision;
    }

    int nearestSub = qRound(subsFromStart);
    if (nearestSub < 0) nearestSub = 0;

    qint64 snapped = gridStartSample > 0
        ? qint64(gridStartSample + nearestSub * samplesPerSubdivision)
        : qint64(nearestSub * samplesPerSubdivision);

    if (!audioData.isEmpty()) {
        snapped = qBound(qint64(0), snapped, qint64(audioData[0].size() - 1));
    }
    return snapped;
}

QVector<Marker> WaveformView::snapMarkersToGrid(const QVector<Marker>& markersIn) const
{
    QVector<Marker> result = markersIn;
    for (Marker& m : result) {
        // Неподвижная метка начала — не трогаем (snapSampleToGrid(0) сдвинула бы к gridStart)
        if (m.isFixed)
            continue;
        m.position = snapSampleToGrid(m.position);
        // После привязки к сетке «исходное» состояние = текущее (корректная работа растяжения и отрисовки)
        m.originalPosition = m.position;
        m.updateTimeFromSamples(sampleRate);
    }
    return result;
}

void WaveformView::drawPlaybackCursor(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty()) return;

    // playbackPosition уже в миллисекундах, конвертируем в сэмплы
    qint64 cursorSample = (playbackPosition * sampleRate) / 1000;

    // Ограничиваем позицию каретки границами аудио
    cursorSample = qBound(qint64(0), cursorSample, qint64(audioData[0].size() - 1));

    ViewportGeometry vp = getViewportGeometry(displaySampleCount(), rect.width());
    float cursorX = (cursorSample - vp.startSample) / vp.samplesPerPixel;



    if (cursorX >= 0 && cursorX < rect.width()) {
        const QColor cursorColor = colors.getPlayPositionColor();
        painter.setPen(QPen(cursorColor, 2));
        painter.drawLine(QPointF(rect.x() + cursorX, rect.top()), QPointF(rect.x() + cursorX, rect.bottom()));

        QPolygonF triangle;
        triangle << QPointF(rect.x() + cursorX - 5, rect.top() + 5)
                 << QPointF(rect.x() + cursorX + 5, rect.top() + 5)
                 << QPointF(rect.x() + cursorX, rect.top() + 15);
        painter.setPen(Qt::NoPen);
        painter.setBrush(cursorColor);
        painter.drawPolygon(triangle);
    }
}

void WaveformView::drawBarMarkers(QPainter& painter, const QRectF& rect)
{
    if (audioData.isEmpty()) return;

    painter.setPen(colors.getMarkerTextColor());
    painter.setFont(QFont("Arial", 8));

    float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
    // Длина такта в четвертях: 4/4->4, 3/4->3, 2/4->2, 1/4->1, 6/8->3, 12/8->6
    float barLengthInQuarters = (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    float samplesPerBar = barLengthInQuarters * samplesPerBeat;

    // Количество сэмплов на пиксель с учетом масштаба
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);

    // Вычисляем начальный сэмпл с учетом смещения и масштаба
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);

    // Находим первый такт перед видимой областью, выровненный по опорной доле
    float firstBar = 0.0f;
    if (gridStartSample > 0) {
        float barsFromGridStart = float(startSample - gridStartSample) / samplesPerBar;
        firstBar = floor(barsFromGridStart);
    } else {
        firstBar = floor(startSample / samplesPerBar);
    }

    // Рисуем маркеры тактов
    for (float bar = firstBar; ; bar++) {
        float barSample = gridStartSample > 0
            ? (gridStartSample + bar * samplesPerBar)
            : (bar * samplesPerBar);
        float x = (barSample - startSample) / samplesPerPixel;
        if (x >= rect.width()) break;
        if (x >= 0) {
            // Рисуем фон маркера
            QRectF markerRect(x - markerSpacing / 2.0f, rect.top(), markerSpacing, rect.height());
            painter.fillRect(markerRect, colors.getMarkerBackgroundColor());

            // Рисуем текст маркера
            QString barText = QString::number(int(bar) + 1);
            QRectF textRect = painter.fontMetrics().boundingRect(barText);
            painter.drawText(QPointF(
                x - textRect.width()/2,
                rect.top() + rect.height() - 4
            ), barText);

            // Рисуем вертикальную линию с разной толщиной для сильных долей
            // Линии тактов всегда сильные (начало нового такта)
            painter.setPen(QPen(colors.getBarLineColor(), 2.0));
            painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, height()));
        }
    }
}

void WaveformView::setKeyRegions(const QVector<KeyAnalyzer::KeyRegion>& regions)
{
    keyRegions = regions;
    update();
}

void WaveformView::clearKeyRegions()
{
    if (keyRegions.isEmpty()) {
        return;
    }
    keyRegions.clear();
    update();
}

void WaveformView::setShowKeyModulation(bool show)
{
    if (showKeyModulation == show) {
        return;
    }
    showKeyModulation = show;
    update();
}

namespace {
    // Стабильный цвет тональности: тоника задаёт оттенок, лад — насыщенность.
    QColor keyRegionColor(KeyAnalyzer::Key key)
    {
        if (key == KeyAnalyzer::UNKNOWN_KEY) {
            return QColor(110, 110, 110);
        }
        const int idx = int(key);
        const int tonic = idx / 2;             // 0..11 (C..B)
        const bool isMajor = (idx % 2) == 0;
        // Квинтовый круг для равномерного разноса оттенков соседних тональностей.
        const int circlePos = (tonic * 7) % 12;
        const int hue = (circlePos * 30) % 360;
        const int sat = isMajor ? 180 : 120;
        const int val = isMajor ? 210 : 170;
        return QColor::fromHsv(hue, sat, val);
    }
}

void WaveformView::drawKeyModulation(QPainter& painter, const QRectF& rect)
{
    if (!showKeyModulation || keyRegions.isEmpty() || audioData.isEmpty()) {
        return;
    }

    // Отображение сэмплов в пиксели — идентично drawBarMarkers, чтобы полоса
    // совпадала с тактовой сеткой.
    float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
    if (samplesPerPixel <= 0.0f) {
        return;
    }
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    const QFontMetrics fm = painter.fontMetrics();

    for (int i = 0; i < keyRegions.size(); ++i) {
        const KeyAnalyzer::KeyRegion& region = keyRegions[i];

        float xStart = (float(region.startSample) - startSample) / samplesPerPixel;
        float xEnd = (float(region.endSample) - startSample) / samplesPerPixel;

        // Пропускаем регионы полностью вне видимой области.
        if (xEnd < 0.0f || xStart > width()) {
            continue;
        }

        const float clampedStart = qMax(0.0f, xStart);
        const float clampedEnd = qMin(float(width()), xEnd);
        const float regionWidth = clampedEnd - clampedStart;
        if (regionWidth <= 0.5f) {
            continue;
        }

        QRectF band(clampedStart, rect.top(), regionWidth, rect.height());

        QColor color = keyRegionColor(region.key.key);
        QColor fill = color;
        fill.setAlpha(150);
        painter.fillRect(band, fill);

        // Граница между регионами = точка модуляции: акцентная линия.
        if (i > 0 && xStart >= 0.0f && xStart <= width()) {
            painter.setPen(QPen(QColor(255, 255, 255, 220), 2.0));
            painter.drawLine(QPointF(xStart, rect.top()),
                             QPointF(xStart, rect.bottom()));
        }

        // Название тональности, если помещается (keyName заполняется анализатором).
        const QString label = region.key.keyName;
        if (!label.isEmpty() && regionWidth >= fm.horizontalAdvance(label) + 6.0f) {
            painter.setPen(QColor(20, 20, 20));
            painter.drawText(band, Qt::AlignCenter, label);
        }
    }

    // Нижняя окантовка полосы.
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    painter.drawLine(QPointF(rect.left(), rect.bottom()),
                     QPointF(rect.right(), rect.bottom()));
    painter.restore();
}

QString WaveformView::getPositionText(qint64 position) const
{
    QString positionText;

    if (showTimeDisplay) {
        // Конвертируем позицию в миллисекунды используя TimeUtils
        qint64 msPosition = TimeUtils::samplesToMs(position, sampleRate);
        positionText = TimeUtils::formatTime(msPosition);
    }

    if (showBarsDisplay) {
        float samplesPerBeat = (float(sampleRate) * 60.0f) / bpm;
        float barLengthInQuarters = (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
        float samplesPerBar = barLengthInQuarters * samplesPerBeat;
        int subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
        float samplesPerSubdivision = samplesPerBar / float(subdivisionsPerBar);
        float subsFromStart = (gridStartSample > 0)
            ? float(position - gridStartSample) / samplesPerSubdivision
            : float(position) / samplesPerSubdivision;
        if (subsFromStart < 0.0f) subsFromStart = 0.0f;
        int bar = int(subsFromStart / subdivisionsPerBar) + 1;
        int beat = int(subsFromStart) % subdivisionsPerBar + 1;
        float subBeat = (subsFromStart - float(int(subsFromStart))) * float(subdivisionsPerBar);
        int sub = qBound(1, int(subBeat + 1), subdivisionsPerBar);

        QString barText = QString("%1.%2.%3").arg(bar).arg(beat).arg(sub);

        if (!positionText.isEmpty()) {
            positionText += " | ";
        }
        positionText += barText;
    }

    return positionText;
}

void WaveformView::setPlaybackPosition(qint64 position)
{
    // position приходит в миллисекундах, сохраняем как есть
    playbackPosition = position;
    update();
}

float WaveformView::getCursorXPosition() const
{
    if (audioData.isEmpty()) return 0.0f;

    qint64 cursorSample = (playbackPosition * sampleRate) / 1000;
    cursorSample = qBound(qint64(0), cursorSample, qint64(audioData[0].size() - 1));

    ViewportGeometry vp = getViewportGeometry(displaySampleCount(), width());
    return (cursorSample - vp.startSample) / vp.samplesPerPixel;
}


QPointF WaveformView::sampleToPoint(int sampleIndex, float value, const QRectF& rect) const
{
    return QPointF(
        rect.x() + sampleIndex,
        rect.center().y() + (value * rect.height() * 0.5f)
    );
}

void WaveformView::wheelEvent(QWheelEvent* event)
{
    if (audioData.isEmpty()) {
        event->ignore();
        return;
    }

    zoomAtPixelX(event->angleDelta().y(), event->position().x());
    event->accept();
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Проверяем, кликнули ли на метку
        int markerIndex = getMarkerIndexAt(event->pos());
        qDebug() << "mousePressEvent: markerIndex =" << markerIndex << "at pos:" << event->pos();
        if (markerIndex >= 0) {
            // Обновляем состояние выделения
            if (event->modifiers() & Qt::ShiftModifier) {
                // Shift+клик: добавляем/убираем метку из текущего выделения
                markers[markerIndex].isSelected = !markers[markerIndex].isSelected;
                update();
                return;
            } else {
                // Обычный клик: выделяем только эту метку
                for (Marker& m : markers) {
                    m.isSelected = false;
                }
                markers[markerIndex].isSelected = true;
            }

            // Проверяем, не является ли метка неподвижной
            if (markers[markerIndex].isFixed) {
                qDebug() << "Marker" << markerIndex << "is fixed, cannot drag";
                // Неподвижная метка - не начинаем перетаскивание
                return;
            }

            // Перед началом перетаскивания запоминаем стартовые позиции для всех выделенных меток
            for (Marker& m : markers) {
                if (m.isSelected) {
                    m.dragStartSample = m.position;
                }
            }

            // Начинаем перетаскивание метки (основной — та, по которой кликнули)
            draggingMarkerIndex = markerIndex;
            markerDragSessionActive = true;
            markers[markerIndex].isDragging = true;
            markers[markerIndex].dragStartPos = event->pos();
            qDebug() << "Started dragging marker" << markerIndex << "at position:" << markers[markerIndex].position;
            setCursor(Qt::SizeHorCursor);
            return;
        }

        if ((event->modifiers() & Qt::ShiftModifier) && bpm > 0.f && !audioData.isEmpty()) {
            isGridDragging = true;
            gridDragAnchorSample = gridStartSample;
            gridDragAnchorMouseX = event->position().x();
            lastMousePos = event->pos();
            setCursor(Qt::SizeHorCursor);
            return;
        }

        isDragging = false; // Начинаем как не перетаскивание
        lastMousePos = event->pos();

        // Устанавливаем позицию воспроизведения по клику
        if (!audioData.isEmpty()) {
            // Используем ту же логику, что и в drawPlaybackCursor
            float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
            int visibleSamples = int(width() * samplesPerPixel);
            int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
            int startSample = int(horizontalOffset * maxStartSample);

            // Вычисляем позицию клика в сэмплах
            qint64 clickSample = startSample + qint64(event->pos().x() * samplesPerPixel);
            clickSample = qBound(qint64(0), clickSample, qint64(audioData[0].size() - 1));

            // Конвертируем сэмплы в миллисекунды для внутреннего использования
            qint64 newPosition = (clickSample * 1000) / sampleRate;
            playbackPosition = newPosition;

            // Показываем всплывающую подсказку
            QString positionText = getPositionText(clickSample);
            if (!positionText.isEmpty()) {
                QToolTip::showText(event->globalPosition().toPoint(), positionText, this);
            }

            // Отправляем позицию в миллисекундах для QMediaPlayer
            emit positionChanged(newPosition);
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        // Правый клик: либо контекстное меню для меток, либо панорамирование
        int markerIndex = getMarkerIndexAt(event->pos());
        if (markerIndex >= 0) {
            // Обновляем выделение аналогично левому клику
            if (event->modifiers() & Qt::ShiftModifier) {
                markers[markerIndex].isSelected = !markers[markerIndex].isSelected;
            } else {
                for (Marker& m : markers) {
                    m.isSelected = false;
                }
                markers[markerIndex].isSelected = true;
            }

            // Строим контекстное меню
            QMenu menu(this);
            int selectedCount = 0;
            for (const Marker& m : markers) {
                if (m.isSelected) {
                    ++selectedCount;
                }
            }
            QString snapText = (selectedCount > 1)
                ? tr("Переместить выделенные метки на тактовую сетку")
                : tr("Переместить метку на тактовую сетку");
            QAction* snapAction = menu.addAction(snapText);
            QString deleteText = (selectedCount > 1)
                ? tr("Удалить выделенные метки")
                : tr("Удалить метку");
            QAction* deleteAction = menu.addAction(deleteText);

            QAction* chosen = menu.exec(event->globalPosition().toPoint());
            if (chosen == snapAction) {
                // Перемещаем выбранные метки на ближайшую линию тактовой сетки
                for (Marker& m : markers) {
                    if (m.isSelected && !m.isFixed) {
                        qint64 snapped = snapSampleToGrid(m.position);
                        m.position = snapped;
                        m.updateTimeFromSamples(sampleRate);
                    }
                }
                // Сортируем после изменения позиций
                std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
                    return a.position < b.position;
                });
                // Гарантируем, что нулевая метка (если есть) остаётся в 0
                if (!markers.isEmpty() && markers[0].isFixed) {
                    markers[0].position = 0;
                    markers[0].updateTimeFromSamples(sampleRate);
                }
                emit markersChanged();
                update();
            } else if (chosen == deleteAction) {
                // Удаляем все выделенные метки
                for (int i = markers.size() - 1; i >= 0; --i) {
                    if (markers[i].isSelected) {
                        removeMarker(i);
                    }
                }
            } else {
                update();
            }
        } else {
            // Начинаем панорамирование правой кнопкой мыши
            isRightMousePanning = true;
            lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    // Общая геометрия видимой области (если есть данные)
    ViewportGeometry vp;
    bool hasViewport = false;
    if (!audioData.isEmpty()) {
        vp = getViewportGeometry(displaySampleCount(), width());
        hasViewport = true;
    }

    if (isGridDragging && (event->buttons() & Qt::LeftButton)) {
        if (hasViewport) {
            const float dx = event->position().x() - gridDragAnchorMouseX;
            const qint64 deltaSamples = qint64(dx * vp.samplesPerPixel);
            const qint64 maxGrid = qMax<qint64>(0, audioData[0].size() - 1);
            const qint64 newGrid = qBound<qint64>(0, gridDragAnchorSample + deltaSamples, maxGrid);
            if (newGrid != gridStartSample) {
                applyGridStartSample(newGrid, true);
            }
        }
        lastMousePos = event->pos();
        return;
    }

    // Показываем tooltip с временем при наведении на метку (если не перетаскиваем)
    if (draggingMarkerIndex < 0 && hasViewport) {
        QPoint currentPos = event->position().toPoint();

        // Оптимизация: проверяем tooltip только если мышь переместилась достаточно далеко
        // (экономия вычислений при частых событиях мыши)
        if ((currentPos - lastTooltipPos).manhattanLength() < 3) {
            // Мышь почти не двигалась - пропускаем обновление tooltip
        } else {
            lastTooltipPos = currentPos;

            float mouseX = event->position().x();
            qint64 mouseSample = vp.startSample + qint64(mouseX * vp.samplesPerPixel);

            // Проверяем, находится ли курсор рядом с меткой
            const float markerTolerance = 10.0f; // Пиксели
            int foundMarkerIndex = -1;
            for (int i = 0; i < markers.size(); ++i) {
                const Marker& marker = markers[i];
                float markerX = (marker.position - vp.startSample) / vp.samplesPerPixel;
                float distance = qAbs(mouseX - markerX);

                if (distance < markerTolerance) {
                    foundMarkerIndex = i;
                    break; // Нашли метку - выходим
                }
            }

            // Если нашли метку и она отличается от предыдущей, показываем tooltip
            if (foundMarkerIndex >= 0 && foundMarkerIndex != lastTooltipMarkerIndex) {
                lastTooltipMarkerIndex = foundMarkerIndex;
                const Marker& marker = markers[foundMarkerIndex];

                // Формируем tooltip с информацией о метке
                QString tooltipText;
                if (marker.isEndMarker) {
                    tooltipText = tr("Конец таймлайна\nВремя: %1\nПозиция: %2 сэмплов")
                        .arg(TimeUtils::formatTime(marker.timeMs))
                        .arg(marker.position);
                } else if (marker.isFixed) {
                    tooltipText = tr("Начало таймлайна\nВремя: %1\nПозиция: %2 сэмплов")
                        .arg(TimeUtils::formatTime(marker.timeMs))
                        .arg(marker.position);
                } else {
                    // Вычисляем коэффициент для этого сегмента
                    QString coeffInfo = "";
                    const Marker* nextMarker = nullptr;
                    for (int j = 0; j < markers.size(); ++j) {
                        if (j != foundMarkerIndex && markers[j].position > marker.position) {
                            if (nextMarker == nullptr || markers[j].position < nextMarker->position) {
                                nextMarker = &markers[j];
                            }
                        }
                    }

                    if (nextMarker != nullptr) {
                        qint64 originalDistance = nextMarker->originalPosition - marker.originalPosition;
                        qint64 currentDistance = nextMarker->position - marker.position;
                        if (originalDistance > 0) {
                            float coefficient = float(currentDistance) / float(originalDistance);
                            coeffInfo = tr("\nКоэффициент: %1").arg(coefficient, 0, 'f', 3);
                        }
                    }

                    tooltipText = tr("Метка\nВремя: %1\nПозиция: %2 сэмплов%3")
                        .arg(TimeUtils::formatTime(marker.timeMs))
                        .arg(marker.position)
                        .arg(coeffInfo);
                }

                QToolTip::showText(event->globalPosition().toPoint(), tooltipText, this);
            } else if (foundMarkerIndex < 0 && lastTooltipMarkerIndex >= 0) {
                // Ушли с метки - сбрасываем кеш
                lastTooltipMarkerIndex = -1;

                // Показываем обычный tooltip с позицией
                qint64 samplePos = qBound(qint64(0), mouseSample, qint64(audioData[0].size() - 1));
                qint64 timeMs = TimeUtils::samplesToMs(samplePos, sampleRate);
                QString positionText = tr("Позиция: %1\nВремя: %2")
                    .arg(samplePos)
                    .arg(TimeUtils::formatTime(timeMs));
                QToolTip::showText(event->globalPosition().toPoint(), positionText, this);
            }
        }
    }

    // Обработка перетаскивания метки
    if (draggingMarkerIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (hasViewport) {
            const qint64 timelineSize = displaySampleCount();
            // Вычисляем новую позицию метки
            qint64 newSample = vp.startSample + qint64(event->pos().x() * vp.samplesPerPixel);

            // Специальная обработка для последней метки: разрешаем перетаскивание за границу
            bool isLastMarkerDragging = false;
            QVector<Marker> sortedMarkers = markers;
            std::sort(sortedMarkers.begin(), sortedMarkers.end(),
                      [](const Marker& a, const Marker& b) {
                return a.position < b.position;
            });

            if (sortedMarkers.size() >= 1) {
                isLastMarkerDragging = (markers[draggingMarkerIndex].position ==
                                       sortedMarkers[sortedMarkers.size() - 1].position);
            }

            if (isLastMarkerDragging) {
                // Последняя метка: разрешаем перетаскивание за границу виджета
                // Если курсор за границей виджета, продолжаем перемещение пропорционально
                if (event->pos().x() > width()) {
                    // Курсор за правой границей виджета
                    float overflowPixels = event->pos().x() - width();

                    // Дополнительные сэмплы пропорционально переполнению
                    qint64 overflowSamples = qint64(overflowPixels * vp.samplesPerPixel);

                    // Позиция = правая граница аудио + переполнение
                    qint64 audioEndSample = timelineSize;
                    newSample = audioEndSample + overflowSamples;

                    // Изменяем курсор для индикации возможности перетаскивания за границу
                    setCursor(Qt::SizeHorCursor);
                } else if (event->pos().x() > width() - 50) {
                    // Приближаемся к границе - показываем курсор изменения размера
                    setCursor(Qt::SizeHorCursor);
                } else {
                    setCursor(Qt::OpenHandCursor);
                }
            } else {
                // Обычная метка: ограничиваем позицию границами аудио
            newSample = qBound(qint64(0), newSample, qint64(timelineSize - 1));
            }

            // Ограничиваем позицию соседними метками, чтобы метки не перелезали друг через друга
            if (markers.size() > 1) {
                // Сохраняем текущую позицию перетаскиваемой метки
                qint64 currentMarkerPos = markers[draggingMarkerIndex].position;

                // Улучшенный поиск соседних меток
                // Используем текущую позицию для определения направления движения
                // (переменные могут использоваться в будущем для оптимизации)
                bool movingRight = (newSample > currentMarkerPos);
                bool movingLeft = (newSample < currentMarkerPos);
                Q_UNUSED(movingRight);
                Q_UNUSED(movingLeft);

                // Ищем предыдущую метку (строго меньше текущей позиции)
                qint64 prevMarkerPos = -1;
                int prevMarkerIndex = -1;
                Q_UNUSED(prevMarkerIndex); // Индекс предыдущей метки (может использоваться в будущем)
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        qint64 markerPos = markers[j].position;
                        // Ищем метку слева от текущей позиции (строго меньше)
                        if (markerPos < currentMarkerPos) {
                            if (prevMarkerPos < 0 || markerPos > prevMarkerPos) {
                                prevMarkerPos = markerPos;
                                prevMarkerIndex = j;
                            }
                        }
                    }
                }

                // Ищем следующую метку (строго больше текущей позиции)
                qint64 nextMarkerPos = -1;
                int nextMarkerIndex = -1;
                Q_UNUSED(nextMarkerIndex); // Индекс следующей метки (может использоваться в будущем)
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        qint64 markerPos = markers[j].position;
                        // Ищем метку справа от текущей позиции (строго больше)
                        if (markerPos > currentMarkerPos) {
                            if (nextMarkerPos < 0 || markerPos < nextMarkerPos) {
                                nextMarkerPos = markerPos;
                                nextMarkerIndex = j;
                            }
                        }
                    }
                }

                // Минимальный сегмент 50 мс — метки не перескакивают и не сближаются ближе 50 мс
                const qint64 minSegSamples = (sampleRate * MIN_MARKER_SEGMENT_MS) / 1000;
                qint64 maxSamples = timelineSize - 1;
                qint64 minAllowedPos = (prevMarkerPos >= 0) ? (prevMarkerPos + minSegSamples) : qint64(0);
                qint64 maxAllowedPos = (nextMarkerPos >= 0) ? (nextMarkerPos - minSegSamples) : maxSamples;

                if (prevMarkerPos >= 0 && nextMarkerPos >= 0) {
                    qint64 gapSize = nextMarkerPos - prevMarkerPos;
                    if (gapSize < 2 * minSegSamples) {
                        // Между соседями меньше 100 мс — не двигаем, чтобы не нарушить минимум 50 мс
                        newSample = currentMarkerPos;
                    } else {
                        newSample = qBound(minAllowedPos, newSample, maxAllowedPos);
                    }
                } else {
                    newSample = qBound(minAllowedPos, newSample, maxAllowedPos);
                }

                // Финальная проверка: убеждаемся, что новая позиция не совпадает с другими метками
                // Проверяем все метки, включая те, что могут быть созданы позже
                for (int j = 0; j < markers.size(); ++j) {
                    if (j != draggingMarkerIndex) {
                        qint64 markerPos = markers[j].position;
                        // Проверяем, не пересекается ли новая позиция с другой меткой
                        if (markerPos == newSample) {
                            // Позиция занята другой меткой - оставляем на текущей позиции
                            newSample = currentMarkerPos;
                            break;
                        }
                    }
                }

                // Финальная проверка валидности границ аудио (для обычных меток)
                if (!isLastMarkerDragging) {
                newSample = qBound(qint64(0), newSample, qint64(timelineSize - 1));
                }
            }

            // Применяем ограничение минимального сжатия (0.1) для ВСЕХ сегментов
            // Максимальное растяжение не ограничено
            if (sortedMarkers.size() >= 2) {
                // Находим индекс текущей метки в отсортированном массиве
                int sortedIndex = -1;
                for (int i = 0; i < sortedMarkers.size(); ++i) {
                    if (sortedMarkers[i].position == markers[draggingMarkerIndex].position) {
                        sortedIndex = i;
                        break;
                    }
                }

                if (sortedIndex >= 0) {
                    qint64 minAllowed = std::numeric_limits<qint64>::min();
                    qint64 maxAllowed = std::numeric_limits<qint64>::max();

                    // Проверяем левый сегмент (между предыдущей и текущей меткой)
                    if (sortedIndex > 0) {
                        const Marker& prevMarker = sortedMarkers[sortedIndex - 1];
                        const Marker& currentMarker = sortedMarkers[sortedIndex];

                        qint64 originalDistance = currentMarker.originalPosition - prevMarker.originalPosition;
                        if (originalDistance > 0) {
                            qint64 minDistance = qint64(originalDistance * 0.1);  // Минимум 10%

                            // Текущую метку можно двигать от prevMarker.position + minDistance
                            // (максимальное растяжение не ограничено)
                            minAllowed = qMax(minAllowed, prevMarker.position + minDistance);
                        }
                    }

                    // Проверяем правый сегмент (между текущей и следующей меткой)
                    if (sortedIndex < sortedMarkers.size() - 1) {
                        const Marker& currentMarker = sortedMarkers[sortedIndex];
                        const Marker& nextMarker = sortedMarkers[sortedIndex + 1];

                        qint64 originalDistance = nextMarker.originalPosition - currentMarker.originalPosition;
                        if (originalDistance > 0) {
                            qint64 minDistance = qint64(originalDistance * 0.1);  // Минимум 10%

                            // Текущую метку можно двигать до nextMarker.position - minDistance
                            // (максимальное растяжение не ограничено)
                            maxAllowed = qMin(maxAllowed, nextMarker.position - minDistance);
                        }
                    }

                    // Применяем самые строгие ограничения
                    if (minAllowed != std::numeric_limits<qint64>::min() ||
                        maxAllowed != std::numeric_limits<qint64>::max()) {
                        newSample = qMax(minAllowed, qMin(newSample, maxAllowed));
                    }
                }
            }

            // Проверяем, что новая позиция не совпадает с другими метками
            bool canMove = true;
            for (int j = 0; j < markers.size(); ++j) {
                if (j != draggingMarkerIndex && markers[j].position == newSample) {
                    canMove = false;
                    qDebug() << "Cannot move marker: position" << newSample << "is occupied by marker" << j;
                    break;
                }
            }

            // Если перемещение невозможно, оставляем на текущей позиции
            if (!canMove) {
                newSample = markers[draggingMarkerIndex].position;
            }

            if (event->modifiers() & Qt::ShiftModifier) {
                newSample = snapSampleToGrid(newSample);
            }

            // Обновляем позицию метки (и, при необходимости, группы выделенных меток)
            if (draggingMarkerIndex >= 0 && draggingMarkerIndex < markers.size()) {
                // Сохраняем характеристики для поиска после сортировки
                qint64 savedOriginalPos = markers[draggingMarkerIndex].originalPosition;
                bool savedIsEndMarker = markers[draggingMarkerIndex].isEndMarker;
                bool savedIsFixed = markers[draggingMarkerIndex].isFixed;

                // Сначала обновляем позицию основной метки
                qint64 oldMainStart = markers[draggingMarkerIndex].dragStartSample;
                markers[draggingMarkerIndex].position = newSample;
                markers[draggingMarkerIndex].updateTimeFromSamples(sampleRate);
                qint64 groupDelta = markers[draggingMarkerIndex].position - oldMainStart;

                // Перемещаем все остальные выделенные метки на тот же сдвиг
                if (groupDelta != 0) {
                    for (int j = 0; j < markers.size(); ++j) {
                        if (j == draggingMarkerIndex) continue;
                        Marker& m = markers[j];
                        if (m.isSelected && !m.isFixed) {
                            qint64 targetPos = m.dragStartSample + groupDelta;
                            targetPos = qBound(qint64(0), targetPos, qint64(timelineSize - 1));
                            m.position = targetPos;
                            m.updateTimeFromSamples(sampleRate);
                        }
                    }
                }

                qDebug() << "Marker" << draggingMarkerIndex << "moved to:" << newSample;

                // Сортируем метки по позиции (метки не перескакивают друг через друга)
                std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
                    return a.position < b.position;
                });

                // Нулевая метка всегда статична в 0:00
                if (!markers.isEmpty() && markers[0].isFixed) {
                    markers[0].position = 0;
                    markers[0].updateTimeFromSamples(sampleRate);
                }

                // Находим индекс после сортировки
                draggingMarkerIndex = -1;
                for (int j = 0; j < markers.size(); ++j) {
                    if (markers[j].originalPosition == savedOriginalPos &&
                        markers[j].isEndMarker == savedIsEndMarker &&
                        markers[j].isFixed == savedIsFixed &&
                        markers[j].position == newSample) {
                        draggingMarkerIndex = j;
                        break;
                    }
                }

                if (draggingMarkerIndex < 0) {
                    qDebug() << "ERROR: Could not find dragged marker after sort";
                    draggingMarkerIndex = -1;
                }
            } else {
                qDebug() << "ERROR: draggingMarkerIndex" << draggingMarkerIndex << "is invalid";
                draggingMarkerIndex = -1;
            }

            markMarkersCacheDirty();
            invalidateWavePixmapCache();
            scheduleRealtimeProcess();
            if (!originalAudioData.isEmpty() && markers.size() >= 2) {
                emit markerDragFinished();
            }
            update();
        }
        return;
    }

    // Проверяем, наведена ли мышь на метку
    if (!(event->buttons() & Qt::LeftButton) && !(event->buttons() & Qt::RightButton)) {
        int markerIndex = getMarkerIndexAt(event->pos());
        if (markerIndex >= 0) {
            setCursor(Qt::SizeHorCursor);
            return;
        }
        if ((event->modifiers() & Qt::ShiftModifier) && bpm > 0.f && !audioData.isEmpty()) {
            setCursor(Qt::SizeHorCursor);
            return;
        }
    }

    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - lastMousePos;

        // Определяем, перетаскиваем ли мы (движение больше определенного порога)
        if (!isDragging && delta.manhattanLength() > 5) {
            isDragging = true;
        }

        if (isDragging) {
            // Если перетаскиваем с зажатой кнопкой мыши, обновляем позицию воспроизведения
            if (!audioData.isEmpty()) {
                // Используем ту же логику, что и в drawPlaybackCursor
                float samplesPerPixel = float(audioData[0].size()) / (width() * zoomLevel);
                int visibleSamples = int(width() * samplesPerPixel);
                int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
                int startSample = int(horizontalOffset * maxStartSample);

                // Вычисляем позицию клика в сэмплах
                qint64 clickSample = startSample + qint64(event->pos().x() * samplesPerPixel);
                clickSample = qBound(qint64(0), clickSample, qint64(audioData[0].size() - 1));

                qint64 newPosition = (clickSample * 1000) / sampleRate;
                playbackPosition = newPosition;

                emit positionChanged(newPosition);
                // Используем throttled update для плавности
                scheduleUpdate();
            }
        } else {
            // Обычное перетаскивание для прокрутки
            adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        }

        lastMousePos = event->pos();
    } else if (event->buttons() & Qt::RightButton && isRightMousePanning) {
        // Панорамирование правой кнопкой мыши
        QPoint delta = event->pos() - lastMousePos;
        adjustHorizontalOffset(-float(delta.x()) / (width() * zoomLevel));
        lastMousePos = event->pos();
    } else {
        // При наведении мыши без нажатых кнопок показываем курсор "рука"
        if (!isRightMousePanning && draggingMarkerIndex < 0) {
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const bool hadMarkerDrag = markerDragSessionActive || draggingMarkerIndex >= 0;

        // Завершаем перетаскивание метки
        if (draggingMarkerIndex >= 0) {
            markers[draggingMarkerIndex].isDragging = false;
            draggingMarkerIndex = -1;
            setCursor(Qt::ArrowCursor);
        } else if (markerDragSessionActive) {
            setCursor(Qt::ArrowCursor);
        }

        if (hadMarkerDrag) {
            markerDragSessionActive = false;

            if (!originalAudioData.isEmpty() && markers.size() >= 2) {
                realtimeStretchDirty = true;
                startRealtimeStretchJob();
                emit markerDragFinished();
            }

            emit markersChanged();
            scheduleUpdate();
            return;
        }

        if (isGridDragging) {
            isGridDragging = false;
            setCursor(Qt::OpenHandCursor);
            return;
        }

        isDragging = false;
    } else if (event->button() == Qt::RightButton) {
        isRightMousePanning = false;
        setCursor(Qt::ArrowCursor);
    }
}



void WaveformView::keyPressEvent(QKeyEvent* event)
{
    // Пользовательские метки: только QShortcut AddMarker в MainWindow (настраиваемая клавиша, без дубляжа).

    // Специальная обработка Ctrl+A для меток растяжения
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        int selectedCount = 0;
        for (const Marker& m : markers) {
            if (m.isSelected) {
                ++selectedCount;
            }
        }
        if (selectedCount == 1 && !markers.isEmpty()) {
            // Если выделена ровно одна метка — выделяем все метки
            for (Marker& m : markers) {
                m.isSelected = true;
            }
            update();
            return;
        }
    }

    switch (event->key()) {
        case Qt::Key_Left:
            adjustHorizontalOffset(-scrollStep);
            break;
        case Qt::Key_Right:
            adjustHorizontalOffset(scrollStep);
            break;
        case Qt::Key_Up:
            setZoomLevel(zoomLevel * zoomStep);
            break;
        case Qt::Key_Down:
            setZoomLevel(zoomLevel / zoomStep);
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

void WaveformView::adjustHorizontalOffset(float delta)
{
    // Применяем смещение с ограничением от 0 до 1
    horizontalOffset = qBound(0.0f, horizontalOffset + delta, 1.0f);
    emit horizontalOffsetChanged(horizontalOffset);
    scheduleUpdate(); // Используем throttled update для плавности
}

void WaveformView::adjustZoomLevel(float delta)
{
    if (audioData.isEmpty()) {
        return;
    }

    float oldZoom = zoomLevel;
    float newZoom = oldZoom;

    if (delta > 0) {
        newZoom *= zoomStep;
    } else {
        newZoom /= zoomStep;
    }

    // Ограничиваем масштаб
    newZoom = qBound(float(minZoom), newZoom, float(maxZoom));

    if (qFuzzyCompare(newZoom, oldZoom)) {
        return;
    }

    // Масштабируем относительно центра экрана
    QPoint centerPos = QPoint(width() / 2, height() / 2);

    // Вычисляем текущую позицию в сэмплах под центром экрана
    float samplesPerPixel = float(audioData[0].size()) / (width() * oldZoom);
    int visibleSamples = int(width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    qint64 centerSample = startSample + qint64(centerPos.x() * samplesPerPixel);

    // Обновляем масштаб
    zoomLevel = newZoom;

    // Вычисляем новую позицию, чтобы центр экрана остался над тем же сэмплом
    float newSamplesPerPixel = float(audioData[0].size()) / (width() * newZoom);
    int newVisibleSamples = int(width() * newSamplesPerPixel);
    int newMaxStartSample = qMax(0, audioData[0].size() - newVisibleSamples);

    // Вычисляем новое смещение
    float newOffset = 0.0f;
    if (newMaxStartSample > 0) {
        newOffset = float(centerSample - centerPos.x() * newSamplesPerPixel) / newMaxStartSample;
        newOffset = qBound(0.0f, newOffset, 1.0f);
    }

    horizontalOffset = newOffset;

    // Эмитируем сигналы для обновления скроллбара
    emit zoomChanged(zoomLevel);
    emit horizontalOffsetChanged(horizontalOffset);
    scheduleUpdate(); // Используем throttled update для плавности
}

void WaveformView::setHorizontalOffset(float offset)
{
    // Ограничиваем смещение в пределах от 0 до 1
    horizontalOffset = qBound(0.0f, offset, 1.0f);

    // При изменении смещения обновляем отображение
    emit horizontalOffsetChanged(horizontalOffset);
    update();
}

void WaveformView::setVerticalOffset(float offset)
{
    // Ограничиваем смещение в пределах от 0 до 1
    verticalOffset = qBound(0.0f, offset, 1.0f);

    // При изменении смещения обновляем отображение
    update();
}

void WaveformView::setTimeDisplayMode(bool showTime)
{
    showTimeDisplay = showTime;
}

void WaveformView::setBarsDisplayMode(bool showBars)
{
    showBarsDisplay = showBars;
}

void WaveformView::setBeatsPerBar(int beats)
{
    beatsPerBar = qBound(1, beats, 32);
    update();
}

void WaveformView::setShowBeatWaveform(bool show)
{
    beatVisualizationSettings.showBeatWaveform = show;
    update();
}

bool WaveformView::getShowBeatWaveform() const
{
    return beatVisualizationSettings.showBeatWaveform;
}

void WaveformView::setBeatsAligned(bool aligned)
{
    beatVisualizationSettings.beatsAligned = aligned;
    update();
}

void WaveformView::setWaveformRenderMode(WaveformRenderMode mode)
{
    if (renderMode == mode) {
        return;
    }
    renderMode = mode;
    if (renderMode == WaveformRenderMode::Spectrogram) {
        spectrogramImages.clear();
        spectrogramDirty = true;
    }
    update();
}

void WaveformView::setSpectrogramSettings(const SpectrogramSettings& s)
{
    if (spectrogramSettings == s) {
        return;
    }
    spectrogramSettings = s;
    spectrogramImages.clear();
    spectrogramDirty = true;
    if (renderMode == WaveformRenderMode::Spectrogram) {
        update();
    }
}

bool WaveformView::getBeatsAligned() const
{
    return beatVisualizationSettings.beatsAligned;
}

QString WaveformView::getBarText(float beatPosition) const
{
    int subdivisionsPerBar = (beatsPerBar == 6 || beatsPerBar == 12) ? 8 : 4;
    int bar = int(beatPosition / subdivisionsPerBar) + 1;
    int beat = int(beatPosition) % subdivisionsPerBar + 1;
    float subBeat = (beatPosition - float(int(beatPosition))) * float(subdivisionsPerBar);
    int sub = qBound(1, int(subBeat + 1), subdivisionsPerBar);
    return QString("%1.%2.%3").arg(bar).arg(beat).arg(sub);
}

void WaveformView::setColorScheme(const QString& scheme)
{
    colors.setColorScheme(scheme);
    invalidateWavePixmapCache();
    update();
}

void WaveformView::setLoopStart(qint64 position)
{
    loopStartPosition = position;
    update();
}

void WaveformView::setLoopEnd(qint64 position)
{
    loopEndPosition = position;
    update();
}

void WaveformView::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    // Устанавливаем курсор "рука" при входе в виджет
    if (!isRightMousePanning) {
        setCursor(Qt::OpenHandCursor);
    }
}

void WaveformView::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    // Возвращаем обычный курсор при выходе из виджета
    if (!isRightMousePanning) {
        setCursor(Qt::ArrowCursor);
    }
}

void WaveformView::drawLoopMarkers(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty() || (loopStartPosition <= 0 && loopEndPosition <= 0)) {
        return;
    }

    // Используем ту же логику, что и в drawWaveform
    float samplesPerPixel = float(audioData[0].size()) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, audioData[0].size() - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);

    // Настраиваем перо для маркеров цикла
    QPen loopPen;
    loopPen.setWidth(2);
    loopPen.setStyle(Qt::DashLine);

    // Рисуем маркер начала цикла
    if (loopStartPosition > 0) {
        qint64 startSamplePos = (loopStartPosition * sampleRate) / 1000;

        // Проверяем, попадает ли маркер в видимую область
        if (startSamplePos >= startSample && startSamplePos < startSample + visibleSamples) {
            loopPen.setColor(QColor(0, 255, 0, 180)); // Зелёный для начала
            painter.setPen(loopPen);

            // Вычисляем позицию маркера в пикселях
            float x = (startSamplePos - startSample) / samplesPerPixel;

            if (x >= 0 && x < rect.width()) {
                painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));

                // Рисуем метку "A"
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(QPointF(rect.x() + x + 5, rect.top() + 20), "A");
            }
        }
    }

    // Рисуем маркер конца цикла
    if (loopEndPosition > 0) {
        qint64 endSamplePos = (loopEndPosition * sampleRate) / 1000;

        // Проверяем, попадает ли маркер в видимую область
        if (endSamplePos >= startSample && endSamplePos < startSample + visibleSamples) {
            loopPen.setColor(QColor(255, 0, 0, 180)); // Красный для конца
            painter.setPen(loopPen);

            // Вычисляем позицию маркера в пикселях
            float x = (endSamplePos - startSample) / samplesPerPixel;

            if (x >= 0 && x < rect.width()) {
                painter.drawLine(QPointF(rect.x() + x, rect.top()), QPointF(rect.x() + x, rect.bottom()));

                // Рисуем метку "B"
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(QPointF(rect.x() + x + 5, rect.top() + 20), "B");
            }
        }
    }

    // Если установлены оба маркера, заливаем область между ними
    if (loopStartPosition > 0 && loopEndPosition > 0) {
        qint64 startSamplePos = (loopStartPosition * sampleRate) / 1000;
        qint64 endSamplePos = (loopEndPosition * sampleRate) / 1000;

        // Проверяем, попадает ли хотя бы часть области цикла в видимую область
        if (endSamplePos >= startSample && startSamplePos < startSample + visibleSamples) {
            // Вычисляем позиции в пикселях
            float x1 = qMax(0.0f, (startSamplePos - startSample) / samplesPerPixel);
            float x2 = qMin(float(rect.width()), (endSamplePos - startSample) / samplesPerPixel);

            if (x2 > x1) {
                QColor fillColor(128, 128, 255, 40); // Полупрозрачный синий
                painter.fillRect(QRectF(rect.x() + x1, rect.top(), x2 - x1, rect.height()), fillColor);
            }
        }
    }
}

// Методы для визуализации ударных

// Новые методы для улучшенной визуализации ударных (временно отключены)
/*
void WaveformView::setBeatVisualizationSettings(const BeatVisualizer::VisualizationSettings& settings)
{
    beatVisualizationSettings = settings;
    update();
}

void WaveformView::analyzeBeats()
{
    // Временно отключено для исправления сборки
    if (audioData.isEmpty()) {
        return;
    }

    // Запускаем анализ ударных с текущими настройками
    beatAnalysis = BeatVisualizer::analyzeBeats(audioData, sampleRate, beatVisualizationSettings);

    // Обновляем отображение
    update();
}
*/

// Методы для работы с метками
void WaveformView::addMarker(qint64 position)
{
    if (audioData.isEmpty()) {
        qDebug() << "addMarker: audioData is empty";
        return;
    }

    // Ограничиваем позицию границами аудио
    position = qBound(qint64(0), position, qint64(audioData[0].size() - 1));

    // Минимальный сегмент между метками в сэмплах (50 мс)
    const qint64 minSegmentSamples = (sampleRate * MIN_MARKER_SEGMENT_MS) / 1000;
    if (minSegmentSamples < 1) return;

    // 1) Не создаём метку в том же месте и не ближе 50 мс к существующей
    for (int i = 0; i < markers.size(); ++i) {
        if (qAbs(markers[i].position - position) < minSegmentSamples) {
            qDebug() << "addMarker: marker already exists or too close at position:" << position;
            return;
        }
    }

    // Проверяем, не пытаемся ли создать метку справа от конечной метки
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].isEndMarker && position > markers[i].position) {
            qDebug() << "addMarker: cannot create marker to the right of end marker at position:" << markers[i].position;
            return; // Нельзя создавать метки справа от конечной метки
        }
    }

    qDebug() << "addMarker: adding marker at position:" << position << "audioData size:" << audioData[0].size();

    // Если это первая метка, автоматически создаем начальную (0) и конечную метки
    bool isFirstMarker = markers.isEmpty();

    if (isFirstMarker) {
        qint64 endPosition = audioData[0].size() - 1;

        // Начальная метка всегда статична в 0:00
        markers.append(Marker(0, true, sampleRate)); // true = неподвижная

        // Пользовательскую метку добавляем только если не в 0 и не ближе 50 мс от начала
        if (position >= minSegmentSamples) {
            Marker newMarker(position, sampleRate);
            markers.append(newMarker);
        }
        // Конечная метка
        markers.append(Marker(endPosition, false, true, sampleRate));

        qDebug() << "addMarker: created start marker at 0, user marker at" << position << "and end marker at" << endPosition;
    } else {
        // Находим соседние метки для пересчета коэффициентов
        const Marker* prevMarker = nullptr;
        const Marker* nextMarker = nullptr;
        qint64 maxPrevPos = -1;
        qint64 minNextPos = -1;

        for (int j = 0; j < markers.size(); ++j) {
            qint64 markerPos = markers[j].position;
            if (markerPos < position) {
                if (prevMarker == nullptr || markerPos > maxPrevPos) {
                    prevMarker = &markers[j];
                    maxPrevPos = markerPos;
                }
            } else if (markerPos > position) {
                if (nextMarker == nullptr || markerPos < minNextPos) {
                    nextMarker = &markers[j];
                    minNextPos = markerPos;
                }
            }
        }

        // 4) Минимальный сегмент 50 мс: между двумя метками нельзя создать новую, если до соседей меньше 50 мс
        if (prevMarker != nullptr && (position - prevMarker->position) < minSegmentSamples) {
            qDebug() << "addMarker: segment to previous marker would be < 50 ms";
            return;
        }
        if (nextMarker != nullptr && (nextMarker->position - position) < minSegmentSamples) {
            qDebug() << "addMarker: segment to next marker would be < 50 ms";
            return;
        }
        // В сегменте длиной 50 мс нельзя создавать новые метки (между prev и next должно быть >= 100 мс для вставки)
        if (prevMarker != nullptr && nextMarker != nullptr) {
            qint64 gap = nextMarker->position - prevMarker->position;
            if (gap < 2 * minSegmentSamples) {
                qDebug() << "addMarker: gap between neighbors < 100 ms, cannot add marker";
                return;
            }
        }

        // Вычисляем originalPosition новой метки на основе соседних меток
        qint64 originalPosition = position; // По умолчанию равен position

        if (prevMarker != nullptr && nextMarker != nullptr) {
            // Есть обе соседние метки - интерполируем originalPosition
            qint64 currentDistance = nextMarker->position - prevMarker->position;
            qint64 originalDistance = nextMarker->originalPosition - prevMarker->originalPosition;

            if (currentDistance > 0 && originalDistance > 0) {
                // Линейная интерполяция: вычисляем долю новой метки в текущем сегменте
                // и применяем ту же долю к исходному сегменту
                float ratio = float(position - prevMarker->position) / float(currentDistance);
                originalPosition = prevMarker->originalPosition + qint64(ratio * originalDistance);
            } else if (currentDistance > 0) {
                // Если originalDistance == 0, используем простую интерполяцию по position
                float ratio = float(position - prevMarker->position) / float(currentDistance);
                originalPosition = prevMarker->originalPosition + qint64(ratio * (nextMarker->originalPosition - prevMarker->originalPosition));
            }
        } else if (prevMarker != nullptr) {
            // Есть только предыдущая метка - используем её коэффициент для экстраполяции
            if (prevMarker->position != prevMarker->originalPosition && prevMarker->position > 0) {
                float stretchRatio = float(prevMarker->position) / float(prevMarker->originalPosition);
                originalPosition = qint64(position / stretchRatio);
            } else {
                originalPosition = position;
            }
        } else if (nextMarker != nullptr) {
            // Есть только следующая метка - используем её коэффициент для экстраполяции
            if (nextMarker->position != nextMarker->originalPosition && nextMarker->originalPosition > 0) {
                float stretchRatio = float(nextMarker->position) / float(nextMarker->originalPosition);
                originalPosition = qint64(position / stretchRatio);
            } else {
                originalPosition = position;
            }
        }

        // Создаем новую метку с правильно вычисленным originalPosition
        // originalPosition отражает исходную геометрию сегментов до применения эффекта
        Marker newMarker(position, sampleRate);
        newMarker.originalPosition = originalPosition;
        newMarker.updateTimeFromSamples(sampleRate);
        markers.append(newMarker);
    }

    // Сортируем метки по позиции (метки не должны перескакивать друг через друга)
    std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });

    // 1) Начальная (нулевая) метка всегда статична в 0:00
    if (!markers.isEmpty() && markers[0].isFixed) {
        markers[0].position = 0;
        markers[0].updateTimeFromSamples(sampleRate);
    }

    qDebug() << "addMarker: total markers now:" << markers.size();
    markMarkersCacheDirty();
    invalidateWavePixmapCache();
    update();
    emit markersChanged();
}

void WaveformView::addMarker(const Marker& marker)
{
    if (audioData.isEmpty()) {
        return;
    }

    markers.append(marker);
    markMarkersCacheDirty();
    invalidateWavePixmapCache();
    update();
    emit markersChanged();
}

void WaveformView::sortMarkers()
{
    std::sort(markers.begin(), markers.end(),
              [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });
    if (!markers.isEmpty() && markers[0].isFixed) {
        markers[0].position = 0;
        markers[0].originalPosition = 0;
        markers[0].updateTimeFromSamples(sampleRate);
    }
    markMarkersCacheDirty();
    invalidateWavePixmapCache();
    update();
    emit markersChanged();
}

void WaveformView::removeMarker(int index)
{
    if (index >= 0 && index < markers.size()) {
        markers.removeAt(index);
        if (draggingMarkerIndex == index) {
            draggingMarkerIndex = -1;
        } else if (draggingMarkerIndex > index) {
            draggingMarkerIndex--;
        }
        if (lastTooltipMarkerIndex == index) {
            lastTooltipMarkerIndex = -1;
        } else if (lastTooltipMarkerIndex > index) {
            lastTooltipMarkerIndex--;
        }
        markMarkersCacheDirty();
        invalidateWavePixmapCache();
        update();
        emit markersChanged();
    }
}

void WaveformView::clearMarkers()
{
    if (markers.isEmpty()) {
        return;
    }
    markers.clear();
    draggingMarkerIndex = -1;
    lastTooltipMarkerIndex = -1;
    markMarkersCacheDirty();
    invalidateWavePixmapCache();
    update();
    emit markersChanged();
}

int WaveformView::getMarkerIndexAt(const QPoint& pos) const
{
    if (audioData.isEmpty()) return -1;

    // Используем ту же логику, что и при отрисовке: позиции по текущим данным
    qint64 referenceSize = audioData[0].size();
    float samplesPerPixel = float(referenceSize) / (rect().width() * zoomLevel);
    int visibleSamples = int(rect().width() * samplesPerPixel);
    int maxStartSample = qMax(0, referenceSize - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    Q_UNUSED(visibleSamples);

    const float diamondSize = 10.0f;
    float centerY = rect().height() / 2.0f;

    // Проверяем каждую метку
    for (int i = 0; i < markers.size(); ++i) {
        // Вычисляем X позицию метки по текущей позиции
        float x = (markers[i].position - startSample) / samplesPerPixel;

        // Создаём прямоугольник ромбика
        QRectF diamondRect(x - diamondSize / 2, centerY - diamondSize / 2,
                          diamondSize, diamondSize);

        // Увеличиваем область клика для удобства
        QRectF expandedRect = diamondRect.adjusted(-5, -5, 5, 5);

        if (expandedRect.contains(pos)) {
            qDebug() << "Found marker" << i << "at pos:" << pos
                     << "x:" << x << "position:" << markers[i].position;
            return i;
        }
    }

    return -1;
}

// УДАЛЕНО (2026-01-12): Метод больше не используется, логика перенесена в getMarkerIndexAt
// QRectF WaveformView::getMarkerDiamondRect(qint64 samplePos, const QRect& rect) const
// {
//     ... (использовал неправильную позицию для проверки кликов)
// }

void WaveformView::drawMarkers(QPainter& painter, const QRect& rect)
{
    if (audioData.isEmpty() || markers.isEmpty()) {
        return;
    }

    // Отладочный вывод (можно закомментировать после проверки)
    // qDebug() << "drawMarkers: drawing" << markers.size() << "markers, rect:" << rect.width() << "x" << rect.height();

    // Используем длину отображаемого таймлайна (с учётом растяжения меток)
    const qint64 referenceSize = displaySampleCount();

    // Вычисляем позицию в сэмплах для видимой области
    float samplesPerPixel = float(referenceSize) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, referenceSize - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);
    Q_UNUSED(visibleSamples); // Используется неявно через samplesPerPixel

    // Определяем активный сегмент (сегмент, в котором находится позиция воспроизведения)
    qint64 playbackSample = TimeUtils::msToSamples(playbackPosition, sampleRate);
    const Marker* activeStartMarker = nullptr;
    const Marker* activeEndMarker = nullptr;

    ensureSortedMarkersCache();
    const QVector<Marker>& sortedMarkers = cachedSortedMarkers;

    for (int i = 0; i < sortedMarkers.size() - 1; ++i) {
        if (playbackSample >= sortedMarkers[i].position && playbackSample <= sortedMarkers[i + 1].position) {
            activeStartMarker = &sortedMarkers[i];
            activeEndMarker = &sortedMarkers[i + 1];
            break;
        }
    }

    if (!sortedMarkers.isEmpty() && playbackSample < sortedMarkers.first().position) {
        activeStartMarker = nullptr;
        activeEndMarker = &sortedMarkers.first();
    }

    if (!sortedMarkers.isEmpty() && playbackSample > sortedMarkers.last().position) {
        activeStartMarker = &sortedMarkers.last();
        activeEndMarker = nullptr;
    }

    // Визуально выделяем активный сегмент (полупрозрачный фон)
    if (activeStartMarker != nullptr || activeEndMarker != nullptr) {
        float startX = activeStartMarker ?
            ((activeStartMarker->position - startSample) / samplesPerPixel) : 0.0f;
        float endX = activeEndMarker ?
            ((activeEndMarker->position - startSample) / samplesPerPixel) : rect.width();

        // Ограничиваем границы видимой областью
        startX = qBound(0.0f, startX, static_cast<float>(rect.width()));
        endX = qBound(0.0f, endX, static_cast<float>(rect.width()));

        if (endX > startX) {
            // Рисуем полупрозрачный фон для активного сегмента
            QColor activeSegmentColor(255, 255, 0, 30); // Желтый с прозрачностью
            painter.fillRect(QRectF(startX, 0, endX - startX, rect.height()), activeSegmentColor);
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, false);

    // Отрисовываем метки в отсортированном порядке, чтобы за один проход иметь доступ
    // к предыдущей и следующей метке без дополнительных поисков.
    for (int i = 0; i < sortedMarkers.size(); ++i) {
        const Marker& marker = sortedMarkers[i];

        // Вычисляем X позицию метки по текущей позиции
        float x = (marker.position - startSample) / samplesPerPixel;

        // Проверяем, видна ли метка в видимой области
        if (x < -10 || x > rect.width() + 10) {
            // Метка вне видимой области, пропускаем
            continue;
        }

        // Отладочный вывод (можно закомментировать после проверки)
        // qDebug() << "Drawing marker" << i << "at x:" << x << "position:" << marker.position << "startSample:" << startSample;

        // Рисуем вертикальную линию: для выделенных меток — яркую, для остальных — чуть темнее
        QColor lineColor = marker.isSelected ? Qt::white : QColor(220, 220, 220);
        painter.setPen(QPen(lineColor, 2.0f));
        painter.drawLine(QPointF(x, 0), QPointF(x, rect.height()));

        // Рисуем ромбик в центре высоты виджета
        const float diamondSize = 10.0f; // Размер ромбика
        float centerY = rect.height() / 2.0f; // Центр по вертикали

        // Определяем цвета углов в зависимости от растяжения участков между метками
        QColor leftColor = Qt::white;  // Левый уголок указывает на участок слева
        QColor rightColor = Qt::white; // Правый уголок указывает на участок справа

        // Предыдущая метка в отсортированном массиве
        const Marker* prevMarker = (i > 0) ? &sortedMarkers[i - 1] : nullptr;

        // Проверяем участок слева (между предыдущей и текущей меткой),
        // только если метка не выделена (у выделенной оба угла остаются белыми)
        if (!marker.isSelected && prevMarker != nullptr) {
            qint64 originalLeftDistance = marker.originalPosition - prevMarker->originalPosition;
            qint64 currentLeftDistance = marker.position - prevMarker->position;

            if (originalLeftDistance > 0) {
                if (currentLeftDistance < originalLeftDistance) {
                    // Участок слева сжат - левый уголок синий
                    leftColor = Qt::blue;
                } else if (currentLeftDistance > originalLeftDistance) {
                    // Участок слева растянут - левый уголок красный
                    leftColor = Qt::red;
                }
            }
        }

        // Следующая метка в отсортированном массиве
        const Marker* nextMarker = (i + 1 < sortedMarkers.size()) ? &sortedMarkers[i + 1] : nullptr;

        // Проверяем участок справа (между текущей и следующей меткой),
        // только если метка не выделена
        if (!marker.isSelected && nextMarker != nullptr) {
            qint64 originalRightDistance = nextMarker->originalPosition - marker.originalPosition;
            qint64 currentRightDistance = nextMarker->position - marker.position;

            if (originalRightDistance > 0) {
                if (currentRightDistance < originalRightDistance) {
                    // Участок справа сжат - правый уголок синий
                    rightColor = Qt::blue;
                } else if (currentRightDistance > originalRightDistance) {
                    // Участок справа растянут - правый уголок красный
                    rightColor = Qt::red;
                }
            }
        }

        // Рисуем ромбик: четыре точки (верх, право, низ, лево)
        // Отраженный по горизонтали: левая часть - левый цвет, правая часть - правый цвет
        QPointF top(x, centerY - diamondSize / 2);      // Верх
        QPointF right(x + diamondSize / 2, centerY);   // Право
        QPointF bottom(x, centerY + diamondSize / 2);  // Низ
        QPointF left(x - diamondSize / 2, centerY);    // Лево

        // Рисуем левую половину ромбика (верх-лево-низ)
        painter.setPen(QPen(leftColor, 2.0f));
        painter.drawLine(top, left);
        painter.drawLine(left, bottom);

        // Рисуем правую половину ромбика (верх-право-низ)
        painter.setPen(QPen(rightColor, 2.0f));
        painter.drawLine(top, right);
        painter.drawLine(right, bottom);

        // Если это конечная метка (isEndMarker), показываем "Конец таймлайна" справа
        // Конечная метка может быть перемещена, но она всегда обозначает конец таймлайна
        if (marker.isEndMarker) {
            const float lineY = centerY; // По центру высоты
            const float textOffset = 15.0f; // Отступ текста от линии

            // Горизонтальная линия справа от метки
            const float lineLength = 30.0f;
            painter.setPen(QPen(Qt::white, 1.0f));
            painter.drawLine(QPointF(x + 5, lineY), QPointF(x + lineLength, lineY));

            // Текст "Конец таймлайна" справа от метки
            QString timelineEndText = tr("Конец таймлайна");
            QFontMetrics fm(painter.font());
            QRect textRect = fm.boundingRect(timelineEndText);
            painter.setPen(Qt::white);
            painter.drawText(QRectF(x + textOffset, lineY - textRect.height() - 5,
                                   textRect.width(), textRect.height()),
                            Qt::AlignLeft | Qt::AlignTop, timelineEndText);
        } else if (nextMarker != nullptr) {
            // Обычная логика для меток с следующей меткой справа
            // Используем найденную ранее nextMarker (по позиции, а не по индексу)

            // Вычисляем позицию следующей метки
            float nextX = (nextMarker->position - startSample) / samplesPerPixel;

            // Вычисляем коэффициент растяжения между метками
            qint64 originalDistance = nextMarker->originalPosition - marker.originalPosition;
            qint64 currentDistance = nextMarker->position - marker.position;

            // Показываем коэффициенты всегда, когда есть две метки (даже если они еще не были перемещены)
            if (originalDistance > 0) {
                float coefficient = float(currentDistance) / float(originalDistance);

                // Горизонтальные линии между метками
                const float lineY = centerY; // По центру высоты
                const float midX = (x + nextX) / 2.0f; // Середина между метками

                // Линия слева (от текущей метки до середины)
                painter.setPen(QPen(Qt::white, 1.0f));
                painter.drawLine(QPointF(x + 5, lineY), QPointF(midX - 20, lineY));

                // Линия справа (от середины до следующей метки)
                painter.drawLine(QPointF(midX + 20, lineY), QPointF(nextX - 5, lineY));

                // Коэффициент сверху посередине между метками
                QString coeffText = QString::number(coefficient, 'f', 3);
                QFontMetrics fm(painter.font());
                QRect textRect = fm.boundingRect(coeffText);
                painter.setPen(Qt::white);
                painter.drawText(QRectF(midX - textRect.width() / 2.0f, lineY - textRect.height() - 5,
                                       textRect.width(), textRect.height()),
                                Qt::AlignCenter, coeffText);
            }
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
}

TimeStretchProcessor::StretchResult WaveformView::applyTimeStretch(const QVector<Marker>& markers) const
{
    const QVector<MarkerData> markerData = MarkerUtils::toMarkerData(markers);

    // Вызываем новый API TimeStretchProcessor
    // Используем originalAudioData если есть, иначе audioData
    TimeStretchProcessor::StretchResult result = TimeStretchProcessor::applyMarkerStretch(
        originalAudioData.isEmpty() ? audioData : originalAudioData,
        markerData,
        sampleRate,
        true
    );

    return result;
}

WaveformView::ActiveSegmentInfo WaveformView::getActiveSegmentInfo() const
{
    ActiveSegmentInfo info;
    info.isValid = false;

    if (audioData.isEmpty() || markers.isEmpty()) {
        return info;
    }

    // Конвертируем позицию воспроизведения в сэмплы
    qint64 playbackSample = TimeUtils::msToSamples(playbackPosition, sampleRate);

    // Сортируем метки по позиции
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(), [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });

    // Находим сегмент, в котором находится позиция воспроизведения
    const Marker* startMarker = nullptr;
    const Marker* endMarker = nullptr;

    for (int i = 0; i < sortedMarkers.size() - 1; ++i) {
        if (playbackSample >= sortedMarkers[i].position && playbackSample <= sortedMarkers[i + 1].position) {
            startMarker = &sortedMarkers[i];
            endMarker = &sortedMarkers[i + 1];
            break;
        }
    }

    // Если позиция до первой метки
    if (!sortedMarkers.isEmpty() && playbackSample < sortedMarkers.first().position) {
        startMarker = nullptr;
        endMarker = &sortedMarkers.first();
    }

    // Если позиция после последней метки
    if (!sortedMarkers.isEmpty() && playbackSample > sortedMarkers.last().position) {
        startMarker = &sortedMarkers.last();
        endMarker = nullptr;
    }

    if (startMarker != nullptr || endMarker != nullptr) {
        info.isValid = true;

        if (startMarker != nullptr) {
            info.startTimeMs = startMarker->timeMs;
            info.startMarkerTime = TimeUtils::formatTime(startMarker->timeMs);
        } else {
            info.startTimeMs = 0;
            info.startMarkerTime = "00:00.0";
        }

        if (endMarker != nullptr) {
            info.endTimeMs = endMarker->timeMs;
            info.endMarkerTime = TimeUtils::formatTime(endMarker->timeMs);
        } else {
            qint64 audioSize = audioData[0].size();
            info.endTimeMs = TimeUtils::samplesToMs(audioSize, sampleRate);
            info.endMarkerTime = TimeUtils::formatTime(info.endTimeMs);
        }

        // Вычисляем коэффициент растяжения
        if (startMarker != nullptr && endMarker != nullptr) {
            qint64 originalDistance = endMarker->originalPosition - startMarker->originalPosition;
            qint64 currentDistance = endMarker->position - startMarker->position;
            if (originalDistance > 0) {
                info.stretchFactor = static_cast<float>(currentDistance) / static_cast<float>(originalDistance);
            } else {
                info.stretchFactor = 1.0f;
            }
        } else {
            info.stretchFactor = 1.0f;
        }
    }

    return info;
}

void WaveformView::scheduleUpdate(const QRect& rect)
{
    pendingUpdate = true;
    if (rect.isValid()) {
        // Объединяем области обновления
        if (pendingUpdateRect.isValid()) {
            pendingUpdateRect = pendingUpdateRect.united(rect);
        } else {
            pendingUpdateRect = rect;
        }
    } else {
        // Полное обновление - сбрасываем область
        pendingUpdateRect = QRect();
    }

    // Запускаем таймер, если он еще не запущен
    if (!updateTimer->isActive()) {
        updateTimer->start();
    }
}

void WaveformView::scheduleRealtimeProcess()
{
    realtimeStretchDirty = true;
    startRealtimeStretchJob();
}

void WaveformView::startRealtimeStretchJob()
{
    if (!realtimeStretchDirty || realtimeStretchShuttingDown) {
        return;
    }
    if (realtimeStretchJobActive) {
        return;
    }
    if (originalAudioData.isEmpty() || markers.size() < 2) {
        realtimeStretchDirty = false;
        return;
    }

    if (originalAudioData[0].size() > TimeStretchProcessor::maxRealtimePreviewSamples(sampleRate)) {
        realtimeStretchDirty = false;
        return;
    }

    realtimeStretchDirty = false;
    realtimeStretchJobActive = true;
    realtimeJobGeneration = audioSourceGeneration;

    const QVector<MarkerData> markerData = MarkerUtils::toMarkerData(markers);
    realtimeJobMarkers = markerData;

    const QVector<QVector<float>> input = originalAudioData;
    const int rate = sampleRate;
    const quint64 jobGen = realtimeJobGeneration;
    const quint64 audioGen = audioSourceGeneration;

    QPointer<WaveformView> self(this);
    std::atomic<int>* jobsCounter = &realtimeStretchJobsRunning;
    jobsCounter->fetch_add(1);

    std::thread([self, jobsCounter, input, markerData, rate, jobGen, audioGen]() {
        QVector<QVector<float>> audio;
        if (self && !self->realtimeStretchShuttingDown) {
            audio = TimeStretchProcessor::applyMarkerStretch(input, markerData, rate, false).audioData;
        }

        jobsCounter->fetch_sub(1);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, audio = std::move(audio), jobGen, audioGen, markerData]() {
            if (!self || self->realtimeStretchShuttingDown) {
                return;
            }

            self->realtimeStretchJobActive = false;

            const bool fresh = (jobGen == self->audioSourceGeneration)
                               && (audioGen == self->audioSourceGeneration)
                               && MarkerUtils::positionsMatch(self->markers, markerData);

            if (fresh && !audio.isEmpty() && !audio[0].isEmpty()) {
                self->audioData = audio;

                if (self->renderMode == WaveformRenderMode::Spectrogram) {
                    self->spectrogramImages.clear();
                    self->spectrogramDirty = true;
                }
                self->scheduleUpdate();
            } else {
                self->realtimeStretchDirty = true;
            }

            if (self->realtimeStretchDirty) {
                QTimer::singleShot(0, self.data(), &WaveformView::startRealtimeStretchJob);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void WaveformView::updateOriginalData(const QVector<QVector<float>>& newData)
{
    originalAudioData = newData;
    ++audioSourceGeneration; // Результаты фоновых задач со старыми данными будут отброшены
    realtimeStretchDirty = false;
    spectrogramImages.clear();
    spectrogramDirty = true;
}

void WaveformView::applyStretchedPreview(const QVector<QVector<float>>& channels)
{
    if (channels.isEmpty() || channels[0].isEmpty()) {
        return;
    }
    if (!originalAudioData.isEmpty() && channels.size() != originalAudioData.size()) {
        qWarning() << "applyStretchedPreview: channel count mismatch"
                   << channels.size() << "vs" << originalAudioData.size();
        return;
    }

    audioData = channels;
    if (renderMode == WaveformRenderMode::Spectrogram) {
        spectrogramImages.clear();
        spectrogramDirty = true;
    }
    invalidateWavePixmapCache();
    scheduleUpdate();
}

// ============================================================================
// Методы для визуализации области конца таймлайна и растяжения
// ============================================================================

bool WaveformView::isLastSegment(int markerIndex) const
{
    if (markers.isEmpty() || markerIndex < 0 || markerIndex >= markers.size()) {
        return false;
    }

    // Сортируем метки по позиции
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });

    // Проверяем, является ли это предпоследняя метка (начало последнего сегмента)
    const Marker& currentMarker = markers[markerIndex];
    return (sortedMarkers.size() >= 2 &&
            currentMarker.position == sortedMarkers[sortedMarkers.size() - 2].position);
}

float WaveformView::calculateTailAreaCoefficient() const
{
    if (markers.size() < 2) {
        return 0.0f; // Нет последнего сегмента
    }

    // Сортируем метки
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });

    // Берем две последние метки
    const Marker& lastMarker = sortedMarkers[sortedMarkers.size() - 1];
    const Marker& secondLastMarker = sortedMarkers[sortedMarkers.size() - 2];

    // Вычисляем коэффициент последнего сегмента
    qint64 originalDistance = lastMarker.originalPosition - secondLastMarker.originalPosition;
    qint64 currentDistance = lastMarker.position - secondLastMarker.position;

    if (originalDistance <= 0) {
        return 0.0f;
    }

    float coefficient = float(currentDistance) / float(originalDistance);

    // Применяем ограничение минимального сжатия: коэффициент >= 0.1
    // Максимальное растяжение не ограничено
    coefficient = qMax(0.1f, coefficient);

    // Коэффициент конца = 1 - коэффициент растяжения
    // Может быть отрицательным при растяжении > 1.0
    float tailCoefficient = 1.0f - coefficient;

    // При растяжении (coefficient > 1.0) tailCoefficient отрицательный
    // Это означает, что метка за пределами исходного аудио

    return tailCoefficient;
}

void WaveformView::drawTailArea(QPainter& painter, const QRect& rect)
{
    if (markers.size() < 2 || audioData.isEmpty()) {
        return;
    }

    // Получаем коэффициент области конца
    float tailCoefficient = calculateTailAreaCoefficient();

    // При растяжении (coefficient > 1.0) область конца не отображается
    if (tailCoefficient < 0.0f || tailCoefficient <= 0.01f) {
        return; // Нет области для отображения (coefficient > 1.0 или ≈ 1.0)
    }

    // Сортируем метки
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });

    const Marker& lastMarker = sortedMarkers[sortedMarkers.size() - 1];

    // ВАЖНО: Вычисляем позицию на экране относительно ИСХОДНЫХ данных,
    // чтобы метка оставалась на своем месте относительно таймлайна
    qint64 referenceSize = originalAudioData.isEmpty() ? audioData[0].size() : originalAudioData[0].size();
    float samplesPerPixel = float(referenceSize) / (rect.width() * zoomLevel);
    int visibleSamples = int(rect.width() * samplesPerPixel);
    int maxStartSample = qMax(0, referenceSize - visibleSamples);
    int startSample = int(horizontalOffset * maxStartSample);

    // Используем originalPosition для вычисления позиции на экране
    float lastMarkerX = (lastMarker.originalPosition - startSample) / samplesPerPixel;

    // Проверяем, видна ли последняя метка
    if (lastMarkerX < -10 || lastMarkerX > rect.width() + 10) {
        return; // Последняя метка не видна
    }

    // Вычисляем ширину области конца
    float tailAreaWidth = rect.width() - lastMarkerX;

    if (tailAreaWidth <= 0) {
        return; // Нет места для области конца
    }

    // Рисуем полупрозрачный фон для области конца
    QColor tailBgColor = colors.getBackgroundColor().darker(110);
    tailBgColor.setAlpha(100);
    painter.fillRect(QRectF(lastMarkerX, 0, tailAreaWidth, rect.height()), tailBgColor);

    // Рисуем волну из области после последней метки (масштабированную)
    drawTailWaveform(painter, rect, lastMarkerX, tailAreaWidth, tailCoefficient);
}

void WaveformView::drawTailWaveform(QPainter& painter, const QRect& rect,
                                   float startX, float width, float coefficient)
{
    // ВАЖНО: Используем originalAudioData для отображения области конца,
    // так как audioData уже обработаны и укорочены после processRealtimeStretch()
    if (originalAudioData.isEmpty() || markers.size() < 2 || width <= 0) {
        return;
    }

    // Сортируем метки
    QVector<Marker> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const Marker& a, const Marker& b) {
        return a.position < b.position;
    });

    const Marker& lastMarker = sortedMarkers[sortedMarkers.size() - 1];

    // Определяем область после последней метки в ИСХОДНЫХ данных
    // Используем originalPosition для работы с исходным аудио
    qint64 tailStartSample = lastMarker.originalPosition;
    qint64 totalSamples = originalAudioData[0].size();

    if (tailStartSample >= totalSamples) {
        return; // Нет данных после последней метки
    }

    // Количество сэмплов в области конца
    qint64 tailSamples = totalSamples - tailStartSample;

    // Вычисляем, сколько сэмплов показывать в визуальной области
    // coefficient определяет, какая часть исходной области конца отображается
    qint64 visibleTailSamples = qint64(tailSamples * coefficient);

    if (visibleTailSamples <= 0) {
        return;
    }

    // Рисуем волну для каждого канала из ИСХОДНЫХ данных
    float channelHeight = rect.height() / float(originalAudioData.size());

    for (int ch = 0; ch < originalAudioData.size(); ++ch) {
        const QVector<float>& channelData = originalAudioData[ch];

        // Вычисляем агрегированные значения для визуализации
        float samplesPerPixel = float(visibleTailSamples) / width;

        if (samplesPerPixel < 1.0f) {
            samplesPerPixel = 1.0f;
        }

        QVector<float> aggregatedData;
        aggregatedData.reserve(int(width) + 1);

        for (int x = 0; x < int(width); ++x) {
            qint64 sampleStart = tailStartSample + qint64(x * samplesPerPixel);
            qint64 sampleEnd = tailStartSample + qint64((x + 1) * samplesPerPixel);
            sampleEnd = qMin(sampleEnd, totalSamples);

            if (sampleStart >= totalSamples) {
                break;
            }

            // Агрегация: находим минимум и максимум в диапазоне
            float minVal = 0.0f;
            float maxVal = 0.0f;

            for (qint64 s = sampleStart; s < sampleEnd && s < channelData.size(); ++s) {
                float sample = channelData[s];
                minVal = qMin(minVal, sample);
                maxVal = qMax(maxVal, sample);
            }

            aggregatedData.append((minVal + maxVal) / 2.0f);
        }

        // Рисуем агрегированную волну с полупрозрачностью
        painter.setOpacity(0.6); // Полупрозрачность для области конца

        QColor waveColor = colors.getMidColor(); // Используем средний цвет волны
        waveColor.setAlpha(150); // Дополнительная прозрачность
        painter.setPen(QPen(waveColor, 1.0f));

        float centerY = ch * channelHeight + channelHeight / 2.0f;
        float amplitude = channelHeight / 2.0f * 0.9f; // 90% высоты канала

        for (int x = 0; x < aggregatedData.size(); ++x) {
            float sample = aggregatedData[x];
            float y = centerY - sample * amplitude;

            if (x == 0) {
                // Первая точка - начинаем с центра
                painter.drawLine(QPointF(startX, centerY), QPointF(startX + x, y));
            } else {
                float prevSample = aggregatedData[x - 1];
                float prevY = centerY - prevSample * amplitude;
                painter.drawLine(QPointF(startX + x - 1, prevY), QPointF(startX + x, y));
            }
        }

        // Линия центра (нулевой уровень)
        painter.setPen(QPen(QColor(128, 128, 128, 100), 1.0f, Qt::DashLine));
        painter.drawLine(QPointF(startX, centerY), QPointF(startX + width, centerY));

        painter.setOpacity(1.0); // Восстанавливаем непрозрачность
    }
}

// УДАЛЕНО (2026-01-12): Индикаторы растяжения удалены для минималистичного интерфейса
// void WaveformView::drawStretchIndicator(QPainter& painter, const QRect& rect, float tailCoefficient)
// {
//     // Метод был удален по запросу пользователя
// }
