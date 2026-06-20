#include "../include/pitchgridwidget.h"
#include "../include/uiconstants.h"
#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

PitchGridWidget::PitchGridWidget(QWidget *parent)
    : QWidget(parent)
    , sampleRate(44100)
    , playbackPosition(0)
    , gridStartSample(0)
    , cursorXPosition(0.0f)
    , horizontalOffset(0.0f)
    , verticalOffset(0.0f)
    , zoomLevel(1.0f)
    , timelineReferenceWidthPx(0)
    , timelineSampleCount(0)
    , bpm(120.0f)
    , beatsPerBar(4)
    , minPitch(minPitchDefault)
    , maxPitch(maxPitchDefault)
    , isDragging(false)
    , isRightMousePanning(false)
    , beatGridSnap(true)
    , selectedPitch(-1)
    , colorScheme(QStringLiteral("dark"))
{
    setMinimumHeight(minVisiblePitchRows * pitchHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setColorScheme(colorScheme);
}

void PitchGridWidget::setAudioData(const QVector<QVector<float>>& data)
{
    audioData = data;
    rebuildPeaksIfNeeded();
    update();
}

void PitchGridWidget::setSampleRate(int rate)
{
    sampleRate = rate;
    update();
}

void PitchGridWidget::setPlaybackPosition(qint64 position)
{
    playbackPosition = position;
    update();
}

void PitchGridWidget::setCursorPosition(float xPosition)
{
    cursorXPosition = xPosition;
    update();
}

void PitchGridWidget::setHorizontalOffset(float offset)
{
    horizontalOffset = qBound(0.0f, offset, 1.0f);
    update();
}

void PitchGridWidget::adjustHorizontalOffset(float delta)
{
    const float newOffset = qBound(0.0f, horizontalOffset + delta, 1.0f);
    if (qFuzzyCompare(newOffset, horizontalOffset)) {
        return;
    }
    horizontalOffset = newOffset;
    update();
    emit horizontalOffsetChanged(horizontalOffset);
}

void PitchGridWidget::setVerticalOffset(float offset)
{
    verticalOffset = qBound(0.0f, offset, 1.0f);
    update();
}

void PitchGridWidget::adjustVerticalOffset(float delta)
{
    if (maxVerticalScrollPx() <= 0) {
        return;
    }
    const float newOffset = qBound(0.0f, verticalOffset + delta, 1.0f);
    if (qFuzzyCompare(newOffset, verticalOffset)) {
        return;
    }
    verticalOffset = newOffset;
    update();
    emit verticalOffsetChanged(verticalOffset);
}

int PitchGridWidget::pitchContentHeightPx() const
{
    return (maxPitch - minPitch + 1) * pitchHeight;
}

int PitchGridWidget::maxVerticalScrollPx() const
{
    return qMax(0, pitchContentHeightPx() - height());
}

int PitchGridWidget::verticalScrollPixels() const
{
    return int(verticalOffset * maxVerticalScrollPx());
}

void PitchGridWidget::setZoomLevel(float zoom)
{
    zoomLevel = qMax(0.01f, zoom);
    peaksBuildWidth = 0;
    rebuildPeaksIfNeeded();
    update();
}

void PitchGridWidget::setTimelineReferenceWidth(int widthPx)
{
    const int nextWidth = qMax(0, widthPx);
    if (timelineReferenceWidthPx == nextWidth) {
        return;
    }
    timelineReferenceWidthPx = nextWidth;
    peaksBuildWidth = 0;
    update();
}

void PitchGridWidget::setTimelineSampleCount(qint64 samples)
{
    const qint64 nextCount = qMax<qint64>(0, samples);
    if (timelineSampleCount == nextCount) {
        return;
    }
    timelineSampleCount = nextCount;
    update();
}

qint64 PitchGridWidget::effectiveTimelineSamples() const
{
    const qint64 audioSamples = audioData.isEmpty() ? 0 : audioData[0].size();
    if (timelineSampleCount > 0) {
        return qMax(timelineSampleCount, audioSamples);
    }
    return audioSamples;
}

int PitchGridWidget::timelineContentWidthPx() const
{
    return qMax(1, width());
}

int PitchGridWidget::timelineReferenceWidth() const
{
    return timelineReferenceWidthPx > 0 ? timelineReferenceWidthPx : timelineContentWidthPx();
}

float PitchGridWidget::timelineToContentX(float timelineX) const
{
    const int refW = timelineReferenceWidth();
    const int contentW = timelineContentWidthPx();
    if (refW <= 0 || contentW <= 0) {
        return timelineX;
    }
    return timelineX * float(contentW) / float(refW);
}

float PitchGridWidget::contentToTimelineX(float contentX) const
{
    const int contentW = timelineContentWidthPx();
    if (contentW <= 0) {
        return contentX;
    }
    return contentX * float(timelineReferenceWidth()) / float(contentW);
}

float PitchGridWidget::playbackCursorContentX() const
{
    return timelineToContentX(cursorXPosition);
}

void PitchGridWidget::setBPM(float newBpm)
{
    bpm = newBpm;
    update();
}

void PitchGridWidget::setBeatsPerBar(int beats)
{
    beatsPerBar = beats;
    update();
}

void PitchGridWidget::setGridStartSample(qint64 sample)
{
    gridStartSample = sample;
    update();
}

void PitchGridWidget::setPitchRange(int min, int max)
{
    minPitch = min;
    maxPitch = max;
    setMinimumHeight(minVisiblePitchRows * pitchHeight);
    update();
}

void PitchGridWidget::setBeatGridSnapEnabled(bool enabled)
{
    beatGridSnap = enabled;
}

void PitchGridWidget::setColorScheme(const QString& scheme)
{
    colorScheme = scheme;

    if (scheme == QStringLiteral("light")) {
        backgroundColor = QColor(245, 245, 245);
        whiteKeyColor = QColor(255, 255, 255);
        blackKeyColor = QColor(210, 210, 210);
        gridColor = QColor(200, 200, 200);
        beatGridColor = QColor(180, 180, 200);
        barLineColor = QColor(120, 120, 180);
        waveformColor = QColor(30, 30, 30);
        cursorColor = QColor(220, 60, 60);
        pitchLabelColor = QColor(40, 40, 40, UiConstants::kPitchLabelAlpha);
        selectionColor = QColor(100, 150, 255, 80);
    } else {
        backgroundColor = QColor(32, 32, 32);
        whiteKeyColor = QColor(48, 48, 48);
        blackKeyColor = QColor(24, 24, 24);
        gridColor = QColor(70, 70, 70);
        beatGridColor = QColor(90, 90, 110);
        barLineColor = QColor(130, 130, 170);
        waveformColor = QColor(210, 210, 210);
        cursorColor = QColor(255, 100, 100);
        pitchLabelColor = QColor(220, 220, 220, UiConstants::kPitchLabelAlpha);
        selectionColor = QColor(100, 150, 255, 70);
    }

    update();
}

void PitchGridWidget::rebuildPeaksIfNeeded()
{
    const int w = timelineContentWidthPx();
    const int h = height();
    if (w <= 0 || h <= 2 || audioData.isEmpty()) {
        waveformPeaks = {};
        peaksBuildWidth = 0;
        peaksBuildHeight = 0;
        return;
    }

    if (w == peaksBuildWidth && h == peaksBuildHeight && !waveformPeaks.isEmpty()) {
        return;
    }

    waveformPeaks = GiadaPitchGridEngine::buildWaveformPeaks(audioData, w, h);
    peaksBuildWidth = w;
    peaksBuildHeight = h;
}

GiadaPitchGridEngine::Viewport PitchGridWidget::currentViewport() const
{
    return GiadaPitchGridEngine::Viewport::compute(
        effectiveTimelineSamples(), timelineReferenceWidth(), zoomLevel, horizontalOffset);
}

void PitchGridWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    peaksBuildWidth = 0;
    rebuildPeaksIfNeeded();

    const float clamped = maxVerticalScrollPx() > 0 ? qBound(0.0f, verticalOffset, 1.0f) : 0.0f;
    if (!qFuzzyCompare(clamped, verticalOffset)) {
        verticalOffset = clamped;
        emit verticalOffsetChanged(verticalOffset);
    }
}

void PitchGridWidget::paintEvent(QPaintEvent*)
{
    rebuildPeaksIfNeeded();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect area = rect();
    painter.fillRect(area, backgroundColor);

    drawPianoRollBackground(painter, area);
    drawPitchLabels(painter, area);
    drawBeatGrid(painter, area);
    drawWaveformPeaks(painter, area);
    drawSelectedPitchRow(painter, area);
    drawPlaybackCursor(painter, area);
}

void PitchGridWidget::drawPianoRollBackground(QPainter& painter, const QRect& rect) const
{
    const int verticalOffsetPixels = verticalScrollPixels();

    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        const int yTop = (maxPitch - pitch) * pitchHeight - verticalOffsetPixels;
        const int yBottom = yTop + pitchHeight;
        if (yBottom < 0 || yTop > rect.bottom()) {
            continue;
        }

        const QColor keyColor = GiadaPitchGridEngine::isBlackKey(pitch) ? blackKeyColor : whiteKeyColor;
        painter.fillRect(QRect(rect.left(), yTop, rect.width(), pitchHeight), keyColor);

        painter.setPen(gridColor);
        painter.drawLine(rect.left(), yBottom, rect.right(), yBottom);
    }
}

void PitchGridWidget::drawBeatGrid(QPainter& painter, const QRect& rect) const
{
    if (bpm <= 0.0f || audioData.isEmpty()) {
        return;
    }

    const auto viewport = currentViewport();
    const int refWidth = timelineReferenceWidth();
    const QVector<GiadaPitchGridEngine::BeatLine> lines =
        GiadaPitchGridEngine::visibleBeatLines(viewport, bpm, beatsPerBar, sampleRate,
                                               gridStartSample, refWidth);

    for (const GiadaPitchGridEngine::BeatLine& line : lines) {
        const float x = timelineToContentX(line.x);
        if (line.isBarLine) {
            painter.setPen(QPen(barLineColor, 2.0));
        } else {
            QPen dashedPen(beatGridColor, 1.0, Qt::DashLine);
            painter.setPen(dashedPen);
        }
        painter.drawLine(QPointF(rect.left() + x, rect.top()),
                         QPointF(rect.left() + x, rect.bottom()));
    }
}

void PitchGridWidget::drawWaveformPeaks(QPainter& painter, const QRect& rect) const
{
    if (waveformPeaks.isEmpty()) {
        return;
    }

    const auto viewport = currentViewport();
    painter.setPen(waveformColor);
    const int centerY = rect.height() / 2;
    const int timelineWidth = timelineContentWidthPx();
    const qint64 audioSamples = audioData.isEmpty() ? 0 : audioData[0].size();

    for (int x = 0; x < timelineWidth; ++x) {
        const qint64 sample = viewport.pixelToSample(int(contentToTimelineX(float(x))));
        if (sample >= audioSamples) {
            painter.drawLine(QPointF(rect.left() + x, centerY),
                             QPointF(rect.left() + x, centerY));
            continue;
        }
        const int peakIndex = int(sample / waveformPeaks.ratio);
        if (peakIndex < 0 || peakIndex >= waveformPeaks.pixelWidth) {
            continue;
        }
        painter.drawLine(QPointF(rect.left() + x, centerY),
                         QPointF(rect.left() + x, waveformPeaks.upper[peakIndex]));
        painter.drawLine(QPointF(rect.left() + x, centerY),
                         QPointF(rect.left() + x, waveformPeaks.lower[peakIndex]));
    }
}

void PitchGridWidget::drawSelectedPitchRow(QPainter& painter, const QRect& rect) const
{
    if (selectedPitch < minPitch || selectedPitch > maxPitch) {
        return;
    }

    const int verticalOffsetPixels = verticalScrollPixels();
    const int yTop = (maxPitch - selectedPitch) * pitchHeight - verticalOffsetPixels;

    painter.fillRect(QRect(rect.left(), yTop, rect.width(), pitchHeight), selectionColor);
}

void PitchGridWidget::drawPitchLabels(QPainter& painter, const QRect& rect) const
{
    painter.setPen(pitchLabelColor);
    painter.setFont(QFont(QStringLiteral("Arial"), 8));

    const int verticalOffsetPixels = verticalScrollPixels();
    const int labelX = rect.right() - 34;

    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        const int y = (maxPitch - pitch) * pitchHeight + pitchHeight / 2 - verticalOffsetPixels + 4;
        if (y < 0 || y > rect.bottom()) {
            continue;
        }
        painter.drawText(QPointF(labelX, y), getPitchName(pitch));
    }
}

void PitchGridWidget::drawPlaybackCursor(QPainter& painter, const QRect& rect) const
{
    const float cursorX = timelineToContentX(cursorXPosition);
    if (cursorX < -10.0f || cursorX > rect.width() + 10.0f) {
        return;
    }

    painter.setPen(QPen(cursorColor, 2));
    painter.drawLine(QPointF(rect.left() + cursorX, rect.top()),
                     QPointF(rect.left() + cursorX, rect.bottom()));

    QPolygonF triangle;
    triangle << QPointF(rect.left() + cursorX - 5, rect.top() + 5)
             << QPointF(rect.left() + cursorX + 5, rect.top() + 5)
             << QPointF(rect.left() + cursorX, rect.top() + 15);
    painter.setBrush(cursorColor);
    painter.drawPolygon(triangle);
}

QString PitchGridWidget::getPitchName(int midiNote) const
{
    static const QString noteNames[] = {
        QStringLiteral("C"), QStringLiteral("C#"), QStringLiteral("D"), QStringLiteral("D#"),
        QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("F#"), QStringLiteral("G"),
        QStringLiteral("G#"), QStringLiteral("A"), QStringLiteral("A#"), QStringLiteral("B")
    };
    const int octave = (midiNote / 12) - 1;
    const int note = midiNote % 12;
    return QStringLiteral("%1%2").arg(noteNames[note]).arg(octave);
}

int PitchGridWidget::getPitchFromY(int y, const QRect& rect) const
{
    const int verticalOffsetPixels = verticalScrollPixels();
    const int adjustedY = y + verticalOffsetPixels;
    const int pitch = maxPitch - (adjustedY / pitchHeight);
    return qBound(minPitch, pitch, maxPitch);
}

qint64 PitchGridWidget::getPositionFromX(int x, const QRect& rect) const
{
    if (audioData.isEmpty() || sampleRate <= 0) {
        return 0;
    }

    const auto viewport = currentViewport();
    qint64 sample = viewport.pixelToSample(int(contentToTimelineX(float(x))));
    sample = GiadaPitchGridEngine::snapToBeatGrid(sample, bpm, beatsPerBar, sampleRate,
                                                  gridStartSample, beatGridSnap);
    const qint64 maxSample = qMax<qint64>(0, effectiveTimelineSamples() - 1);
    sample = qBound(qint64(0), sample, maxSample);
    return (sample * 1000) / sampleRate;
}

void PitchGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();

        selectedPitch = getPitchFromY(event->pos().y(), rect());
        emit pitchChanged(selectedPitch);
        emit positionChanged(getPositionFromX(event->pos().x(), rect()));
    } else if (event->button() == Qt::RightButton) {
        isRightMousePanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void PitchGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isRightMousePanning && (event->buttons() & Qt::RightButton)) {
        const QPoint delta = event->pos() - lastMousePos;
        const int refWidth = qMax(1, timelineReferenceWidth());
        adjustHorizontalOffset(-float(delta.x()) / (refWidth * zoomLevel));
        lastMousePos = event->pos();
        return;
    }

    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        selectedPitch = getPitchFromY(event->pos().y(), rect());
        emit pitchChanged(selectedPitch);
        emit positionChanged(getPositionFromX(event->pos().x(), rect()));
        lastMousePos = event->pos();
        return;
    }

    if (!(event->buttons() & Qt::LeftButton) && !(event->buttons() & Qt::RightButton)) {
        setCursor(Qt::OpenHandCursor);
    }
}

void PitchGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    } else if (event->button() == Qt::RightButton) {
        isRightMousePanning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void PitchGridWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const float timelineX = contentToTimelineX(float(event->position().x()));
        emit timelineZoomRequested(event->angleDelta().y(), timelineX);
        event->accept();
        return;
    }

    const int wheelDelta = event->angleDelta().y();
    const int maxScroll = maxVerticalScrollPx();
    if (wheelDelta != 0 && maxScroll > 0) {
        constexpr float kRowsPerWheelStep = 3.0f;
        const float pixelDelta = -(wheelDelta / 120.0f) * kRowsPerWheelStep * float(pitchHeight);
        adjustVerticalOffset(pixelDelta / float(maxScroll));
    }
    event->accept();
}
