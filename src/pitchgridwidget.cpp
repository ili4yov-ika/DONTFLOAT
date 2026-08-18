#include "../include/pitchgridwidget.h"
#include "../include/uiconstants.h"
#include <QtCore/QtMath>
#include <QtGui/QCursor>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <cmath>

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
    , minPitch(kMinPitchDefault)
    , maxPitch(kMaxPitchDefault)
    , isDragging(false)
    , isRightMousePanning(false)
    , beatGridSnap(false)
    , selectedPitch(-1)
    , selectedNoteIndex(-1)
    , isNoteDragging(false)
    , noteDragStartPitch(0.0f)
    , noteDragFreePitch(false)
    , noteCutMode(CutMode::SnapToGrid)
    , splitModeActive(false)
    , splitPreviewNoteIndex(-1)
    , splitPreviewSample(0)
    , splitPreviewValid(false)
    , splitShortcut(Qt::Key_S)
{
    primaryKey = PianoRollEngine::KeySignature::fromString(QStringLiteral("C Major"));
    setMinimumHeight(kMinVisiblePitchRows * kPitchRowHeightPx);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    applyTheme(QStringLiteral("dark"));
}

void PitchGridWidget::applyTheme(const QString& scheme)
{
    if (scheme == QStringLiteral("light")) {
        theme.background = QColor(245, 245, 245);
        theme.whiteKey = QColor(255, 255, 255);
        theme.blackKey = QColor(210, 210, 210);
        theme.outOfKeyKey = QColor(200, 200, 200);
        theme.gridLine = QColor(200, 200, 200);
        theme.beatLine = QColor(180, 180, 200);
        theme.barLine = QColor(120, 120, 180);
        theme.cursor = QColor(220, 60, 60);
        theme.labelText = QColor(40, 40, 40);
        theme.outOfKeyLegendText = QColor(170, 170, 170);
        theme.labelBackground = QColor(235, 235, 235, kLegendBackgroundAlpha);
        theme.selection = QColor(100, 150, 255, 80);
        theme.octaveAccent = QColor(180, 200, 255, 90);
        theme.legendBorder = QColor(180, 180, 180);
        theme.noteFill = QColor(90, 160, 235, 190);
        theme.noteEditedFill = QColor(240, 160, 60, 200);
        theme.noteBorder = QColor(40, 90, 160);
        theme.noteSelectedBorder = QColor(255, 255, 255);
        theme.splitLine = QColor(30, 30, 30);
    } else {
        theme.background = QColor(32, 32, 32);
        theme.whiteKey = QColor(48, 48, 48);
        theme.blackKey = QColor(24, 24, 24);
        theme.outOfKeyKey = QColor(52, 52, 52);
        theme.gridLine = QColor(70, 70, 70);
        theme.beatLine = QColor(90, 90, 110);
        theme.barLine = QColor(130, 130, 170);
        theme.cursor = QColor(255, 100, 100);
        theme.labelText = QColor(220, 220, 220);
        theme.outOfKeyLegendText = QColor(110, 110, 110);
        theme.labelBackground = QColor(28, 28, 28, kLegendBackgroundAlpha);
        theme.selection = QColor(100, 150, 255, 70);
        theme.octaveAccent = QColor(80, 110, 180, 60);
        theme.legendBorder = QColor(60, 60, 60);
        theme.noteFill = QColor(80, 150, 230, 190);
        theme.noteEditedFill = QColor(235, 150, 50, 200);
        theme.noteBorder = QColor(150, 200, 255);
        theme.noteSelectedBorder = QColor(255, 255, 255);
        theme.splitLine = QColor(255, 245, 200);
    }
    update();
}

void PitchGridWidget::setAudioData(const QVector<QVector<float>>& data)
{
    audioData = data;
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
    if (sampleRate > 0) {
        const qint64 cursorSample = (position * sampleRate) / 1000;
        const qint64 maxSample = qMax<qint64>(0, effectiveTimelineSamples() - 1);
        cursorXPosition = currentViewport().sampleToPixelX(qBound(qint64(0), cursorSample, maxSample));
    }
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
    return (maxPitch - minPitch + 1) * kPitchRowHeightPx;
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
    update();
}

void PitchGridWidget::setTimelineReferenceWidth(int widthPx)
{
    const int nextWidth = qMax(0, widthPx);
    if (timelineReferenceWidthPx == nextWidth) {
        return;
    }
    timelineReferenceWidthPx = nextWidth;
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
    return qMax(1, width() - kLegendColumnWidthPx);
}

int PitchGridWidget::timelineReferenceWidth() const
{
    return timelineReferenceWidthPx > 0 ? timelineReferenceWidthPx : qMax(1, width());
}

float PitchGridWidget::timelineToContentX(float timelineX) const
{
    const int refW = timelineReferenceWidth();
    if (refW <= 0 || width() <= 0) {
        return timelineX;
    }
    // Каретка и сетка используют те же X, что и WaveformView (легенда — overlay справа).
    return timelineX * float(width()) / float(refW);
}

float PitchGridWidget::contentToTimelineX(float contentX) const
{
    if (width() <= 0) {
        return contentX;
    }
    return contentX * float(timelineReferenceWidth()) / float(width());
}

float PitchGridWidget::playbackCursorContentX() const
{
    return timelineToContentX(cursorXPosition);
}

QRect PitchGridWidget::legendRect() const
{
    return QRect(width() - kLegendColumnWidthPx, 0, kLegendColumnWidthPx, height());
}

bool PitchGridWidget::isInLegendArea(int x) const
{
    return x >= width() - kLegendColumnWidthPx;
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
    setMinimumHeight(kMinVisiblePitchRows * kPitchRowHeightPx);
    update();
}

void PitchGridWidget::setBeatGridSnapEnabled(bool enabled)
{
    beatGridSnap = enabled;
}

void PitchGridWidget::setPrimaryKey(const QString& keyText)
{
    primaryKey = PianoRollEngine::KeySignature::fromString(keyText);
    update();
}

void PitchGridWidget::setSecondaryKey(const QString& keyText)
{
    secondaryKey = PianoRollEngine::KeySignature::fromString(keyText);
    update();
}

void PitchGridWidget::setColorScheme(const QString& scheme)
{
    applyTheme(scheme);
}

void PitchGridWidget::setNotes(const QVector<PitchDetector::PitchNote>& newNotes)
{
    pitchNotes = newNotes;
    if (selectedNoteIndex >= pitchNotes.size()) {
        selectedNoteIndex = -1;
    }
    // Индексы нот сместились (например, после разреза) — подсветку реза сбрасываем
    splitPreviewNoteIndex = -1;
    splitPreviewValid = false;
    update();
}

void PitchGridWidget::clearNotes()
{
    pitchNotes.clear();
    selectedNoteIndex = -1;
    isNoteDragging = false;
    splitPreviewNoteIndex = -1;
    splitPreviewValid = false;
    update();
}

void PitchGridWidget::setNotePitch(int noteIndex, float midiPitch)
{
    if (noteIndex < 0 || noteIndex >= pitchNotes.size()) {
        return;
    }
    pitchNotes[noteIndex].midiPitch = qBound(0.0f, midiPitch, 127.0f);
    update();
}

void PitchGridWidget::setCutMode(CutMode mode)
{
    if (noteCutMode == mode) {
        return;
    }
    noteCutMode = mode;
    if (splitModeActive) {
        // Позиция реза под курсором пересчитывается по новому режиму
        updateSplitPreview(mapFromGlobal(QCursor::pos()));
    }
    update();
}

void PitchGridWidget::setSplitModeActive(bool active)
{
    if (splitModeActive == active) {
        return;
    }
    splitModeActive = active;
    if (!splitModeActive) {
        clearSplitPreview();
    } else if (underMouse()) {
        updateSplitPreview(mapFromGlobal(QCursor::pos()));
    }
    applyCursorShape();
    update();
    emit splitModeChanged(splitModeActive);
}

void PitchGridWidget::setSplitShortcut(const QKeySequence& sequence)
{
    splitShortcut = sequence;
}

qint64 PitchGridWidget::cutSampleFromX(int x) const
{
    const auto viewport = currentViewport();
    const qint64 sample = viewport.pixelToSample(int(contentToTimelineX(float(x))));
    const qint64 maxSample = qMax<qint64>(0, effectiveTimelineSamples());
    return qBound<qint64>(0, sample, maxSample);
}

qint64 PitchGridWidget::playbackCursorSample() const
{
    const auto viewport = currentViewport();
    const qint64 maxSample = qMax<qint64>(0, effectiveTimelineSamples());
    const qint64 pixelSample =
        qBound<qint64>(0, viewport.pixelToSample(int(cursorXPosition)), maxSample);
    if (sampleRate <= 0) {
        return pixelSample;
    }

    // Позиция в мс точнее пиксельной (один пиксель — сотни сэмплов при малом
    // зуме), но берём её только если она совпадает с нарисованной кареткой.
    const qint64 msSample =
        qBound<qint64>(0, (playbackPosition * sampleRate) / 1000, maxSample);
    const float drift = std::abs(viewport.sampleToPixelX(msSample) - cursorXPosition);
    return drift <= 2.0f ? msSample : pixelSample;
}

int PitchGridWidget::noteIndexContainingSample(qint64 sample) const
{
    auto contains = [this, sample](int index) {
        const PitchDetector::PitchNote& note = pitchNotes[index];
        return sample >= note.startSample && sample < note.endSample;
    };

    if (selectedNoteIndex >= 0 && selectedNoteIndex < pitchNotes.size()
        && contains(selectedNoteIndex)) {
        return selectedNoteIndex;
    }
    // Сверху вниз по z-порядку, как и при попадании мышью
    for (int i = pitchNotes.size() - 1; i >= 0; --i) {
        if (contains(i)) {
            return i;
        }
    }
    return -1;
}

bool PitchGridWidget::requestNoteSplit(int noteIndex, qint64 rawSample)
{
    if (noteIndex < 0 || noteIndex >= pitchNotes.size()) {
        emit noteSplitRejected(SplitRejection::NoNoteAtCursor);
        return false;
    }

    const PitchDetector::PitchNote& note = pitchNotes[noteIndex];
    const qint64 cutSample = PianoRollEngine::snapToGrid(
        rawSample, bpm, beatsPerBar, sampleRate, gridStartSample,
        noteCutMode == CutMode::SnapToGrid);

    if (!PianoRollEngine::canSplitNoteAt(note.startSample, note.endSample, cutSample)) {
        emit noteSplitRejected(SplitRejection::CutOutsideNote);
        return false;
    }

    emit noteSplitRequested(noteIndex, cutSample);
    return true;
}

bool PitchGridWidget::splitNoteAtPlaybackCursor()
{
    const qint64 sample = playbackCursorSample();
    return requestNoteSplit(noteIndexContainingSample(sample), sample);
}

void PitchGridWidget::updateSplitPreview(const QPoint& pos)
{
    if (!splitModeActive || !rect().contains(pos)) {
        clearSplitPreview();
        return;
    }

    const int noteIndex = noteIndexAt(pos);
    const qint64 rawSample = cutSampleFromX(pos.x());
    const qint64 cutSample = PianoRollEngine::snapToGrid(
        rawSample, bpm, beatsPerBar, sampleRate, gridStartSample,
        noteCutMode == CutMode::SnapToGrid);
    const bool valid = noteIndex >= 0
        && PianoRollEngine::canSplitNoteAt(pitchNotes[noteIndex].startSample,
                                           pitchNotes[noteIndex].endSample, cutSample);

    if (splitPreviewNoteIndex == noteIndex && splitPreviewSample == cutSample
        && splitPreviewValid == valid) {
        return;
    }
    splitPreviewNoteIndex = noteIndex;
    splitPreviewSample = cutSample;
    splitPreviewValid = valid;
    update();
}

void PitchGridWidget::clearSplitPreview()
{
    if (splitPreviewNoteIndex < 0 && !splitPreviewValid) {
        return;
    }
    splitPreviewNoteIndex = -1;
    splitPreviewValid = false;
    update();
}

bool PitchGridWidget::matchesSplitShortcut(const QKeyEvent* keyEvent) const
{
    if (!keyEvent || splitShortcut.isEmpty()) {
        return false;
    }
    const QKeySequence pressed(keyEvent->keyCombination());
    return pressed == splitShortcut;
}

void PitchGridWidget::applyCursorShape()
{
    setCursor(splitModeActive ? Qt::SplitHCursor : Qt::ArrowCursor);
}

PianoRollEngine::Viewport PitchGridWidget::currentViewport() const
{
    return PianoRollEngine::Viewport::compute(
        effectiveTimelineSamples(), timelineReferenceWidth(), zoomLevel, horizontalOffset);
}

QColor PitchGridWidget::rowColorForPitch(int midiNote) const
{
    if (!PianoRollEngine::isPitchInKeys(midiNote, primaryKey, secondaryKey)) {
        return theme.outOfKeyKey;
    }
    return PianoRollEngine::isBlackKey(midiNote) ? theme.blackKey : theme.whiteKey;
}

QColor PitchGridWidget::legendKeyColor(int midiNote) const
{
    const bool inKey = PianoRollEngine::isPitchInKeys(midiNote, primaryKey, secondaryKey);
    const bool hasActiveKey = primaryKey.isValid() || secondaryKey.isValid();

    QColor keyColor;
    if (hasActiveKey && !inKey) {
        keyColor = theme.outOfKeyKey;
    } else if (PianoRollEngine::isBlackKey(midiNote)) {
        keyColor = theme.labelBackground.darker(115);
    } else {
        keyColor = theme.labelBackground.lighter(108);
    }
    keyColor.setAlpha(kLegendBackgroundAlpha);
    return keyColor;
}

QColor PitchGridWidget::legendTextColor(int midiNote) const
{
    const bool hasActiveKey = primaryKey.isValid() || secondaryKey.isValid();
    if (hasActiveKey && !PianoRollEngine::isPitchInKeys(midiNote, primaryKey, secondaryKey)) {
        return theme.outOfKeyLegendText;
    }
    return theme.labelText;
}

void PitchGridWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const float clamped = maxVerticalScrollPx() > 0 ? qBound(0.0f, verticalOffset, 1.0f) : 0.0f;
    if (!qFuzzyCompare(clamped, verticalOffset)) {
        verticalOffset = clamped;
        emit verticalOffsetChanged(verticalOffset);
    }
}

void PitchGridWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect area = rect();
    painter.fillRect(area, theme.background);

    drawPianoRollBackground(painter, area);
    drawBeatGrid(painter, area);
    drawSelectedPitchRow(painter, area);
    // Референс — под своими нотами: он фон для сверки, а не рабочий материал
    drawReferenceNotes(painter, area);
    drawNoteBlocks(painter, area);
    drawSplitPreview(painter, area);
    drawLegendColumn(painter, legendRect());
    drawPlaybackCursor(painter, area);
}

void PitchGridWidget::drawPianoRollBackground(QPainter& painter, const QRect& rect) const
{
    const int verticalOffsetPixels = verticalScrollPixels();

    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        const int yTop = (maxPitch - pitch) * kPitchRowHeightPx - verticalOffsetPixels;
        const int yBottom = yTop + kPitchRowHeightPx;
        if (yBottom < 0 || yTop > rect.bottom()) {
            continue;
        }

        painter.fillRect(QRect(rect.left(), yTop, rect.width(), kPitchRowHeightPx),
                         rowColorForPitch(pitch));

        if (pitch % 12 == 0) {
            painter.fillRect(QRect(rect.left(), yTop, rect.width(), kPitchRowHeightPx),
                             theme.octaveAccent);
        }

        painter.setPen(theme.gridLine);
        painter.drawLine(rect.left(), yBottom, rect.right(), yBottom);
    }
}

void PitchGridWidget::drawLegendColumn(QPainter& painter, const QRect& rect) const
{
    if (rect.width() <= 0) {
        return;
    }

    painter.fillRect(rect, theme.labelBackground);

    const int verticalOffsetPixels = verticalScrollPixels();
    painter.setFont(QFont(QStringLiteral("Arial"), 8));

    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        const int yTop = (maxPitch - pitch) * kPitchRowHeightPx - verticalOffsetPixels;
        const int yBottom = yTop + kPitchRowHeightPx;
        if (yBottom < 0 || yTop > rect.bottom()) {
            continue;
        }

        const QRect rowRect(rect.left(), yTop, rect.width(), kPitchRowHeightPx);
        painter.fillRect(rowRect, legendKeyColor(pitch));

        if (selectedPitch == pitch) {
            painter.fillRect(rowRect, theme.selection);
        }

        painter.setPen(theme.gridLine);
        painter.drawLine(rect.left(), yBottom, rect.right(), yBottom);

        painter.setPen(legendTextColor(pitch));
        const QString label = PianoRollEngine::midiNoteName(pitch);
        const QRect textRect(rowRect.adjusted(2, 0, -1, 0));
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, label);
    }

    painter.setPen(theme.legendBorder);
    painter.drawLine(rect.topLeft(), rect.bottomLeft());
    painter.drawLine(rect.topRight(), rect.bottomRight());
}

void PitchGridWidget::drawBeatGrid(QPainter& painter, const QRect& rect) const
{
    if (bpm <= 0.0f || audioData.isEmpty()) {
        return;
    }

    const auto viewport = currentViewport();
    const int refWidth = timelineReferenceWidth();
    const QVector<PianoRollEngine::GridLine> lines =
        PianoRollEngine::visibleGridLines(viewport, bpm, beatsPerBar, sampleRate,
                                          gridStartSample, refWidth);

    for (const PianoRollEngine::GridLine& line : lines) {
        const float x = rect.left() + timelineToContentX(line.x);
        if (line.isBarLine) {
            painter.setPen(QPen(theme.barLine, 2.0));
        } else {
            painter.setPen(QPen(theme.beatLine, 1.0, Qt::DashLine));
        }
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
}

void PitchGridWidget::drawSelectedPitchRow(QPainter& painter, const QRect& rect) const
{
    if (selectedPitch < minPitch || selectedPitch > maxPitch) {
        return;
    }

    const int verticalOffsetPixels = verticalScrollPixels();
    const int yTop = (maxPitch - selectedPitch) * kPitchRowHeightPx - verticalOffsetPixels;

    const int legendLeft = width() - kLegendColumnWidthPx;
    painter.fillRect(QRect(rect.left(), yTop, legendLeft, kPitchRowHeightPx), theme.selection);
}

QRectF PitchGridWidget::noteRect(const PitchDetector::PitchNote& note) const
{
    const auto viewport = currentViewport();
    const float x1 = timelineToContentX(viewport.sampleToPixelX(note.startSample));
    const float x2 = timelineToContentX(viewport.sampleToPixelX(note.endSample));
    const float yTop = pitchToContentY(note.midiPitch);
    return QRectF(x1, yTop + 1.0f, qMax(2.0f, x2 - x1), kPitchRowHeightPx - 2);
}

void PitchGridWidget::drawReferenceNotes(QPainter& painter, const QRect& rect) const
{
    if (referenceNotes_.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Серый полупрозрачный блок: виден, но не спорит с рабочими нотами
    const QColor fill(150, 150, 150, 110);
    const QColor border(190, 190, 190, 150);

    for (const PitchDetector::PitchNote& note : referenceNotes_) {
        const QRectF r = noteRect(note);
        if (r.right() < rect.left() || r.left() > rect.right()
            || r.bottom() < rect.top() || r.top() > rect.bottom()) {
            continue;
        }
        painter.setBrush(fill);
        painter.setPen(QPen(border, 1.0));
        painter.drawRoundedRect(r, 2.0, 2.0);
    }

    painter.restore();
}

void PitchGridWidget::setReferenceNotes(const QVector<PitchDetector::PitchNote>& referenceNotes)
{
    referenceNotes_ = referenceNotes;
    update();
}

void PitchGridWidget::clearReferenceNotes()
{
    if (referenceNotes_.isEmpty()) {
        return;
    }
    referenceNotes_.clear();
    update();
}

void PitchGridWidget::drawNoteBlocks(QPainter& painter, const QRect& rect) const
{
    if (pitchNotes.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < pitchNotes.size(); ++i) {
        const PitchDetector::PitchNote& note = pitchNotes[i];
        const QRectF r = noteRect(note);
        if (r.right() < rect.left() || r.left() > rect.right()
            || r.bottom() < rect.top() || r.top() > rect.bottom()) {
            continue;
        }

        const bool edited = std::abs(note.midiPitch - note.detectedPitch) > 0.01f;
        QColor fill = edited ? theme.noteEditedFill : theme.noteFill;
        // Слабые (неуверенные) ноты — полупрозрачнее
        if (note.confidence < 0.5f) {
            fill.setAlpha(int(fill.alpha() * 0.6f));
        }

        painter.setBrush(fill);
        if (i == selectedNoteIndex) {
            painter.setPen(QPen(theme.noteSelectedBorder, 1.5));
        } else {
            painter.setPen(QPen(theme.noteBorder, 1.0));
        }
        painter.drawRoundedRect(r, 2.0, 2.0);
    }

    painter.restore();
}

void PitchGridWidget::drawSplitPreview(QPainter& painter, const QRect& rect) const
{
    if (!splitModeActive || splitPreviewNoteIndex < 0
        || splitPreviewNoteIndex >= pitchNotes.size()) {
        return;
    }

    const auto viewport = currentViewport();
    const float x = rect.left() + timelineToContentX(viewport.sampleToPixelX(splitPreviewSample));
    if (x < rect.left() || x > rect.right()) {
        return;
    }

    const QRectF target = noteRect(pitchNotes[splitPreviewNoteIndex]);
    QColor lineColor = splitPreviewValid ? theme.splitLine : theme.outOfKeyLegendText;

    painter.save();
    // Линия реза через весь пианоролл — видно, к какому делению притянулся рез
    painter.setPen(QPen(lineColor, 1.0, Qt::DashLine));
    painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    if (splitPreviewValid) {
        painter.setPen(QPen(lineColor, 2.0));
        painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
    }
    painter.restore();
}

int PitchGridWidget::noteIndexAt(const QPoint& pos) const
{
    // Сверху вниз по z-порядку: последняя отрисованная нота проверяется первой
    for (int i = pitchNotes.size() - 1; i >= 0; --i) {
        if (noteRect(pitchNotes[i]).adjusted(-1, -1, 1, 1).contains(pos)) {
            return i;
        }
    }
    return -1;
}

void PitchGridWidget::changeSelectedNotePitch(int semitoneDelta)
{
    if (selectedNoteIndex < 0 || selectedNoteIndex >= pitchNotes.size()) {
        return;
    }
    PitchDetector::PitchNote& note = pitchNotes[selectedNoteIndex];
    const float oldPitch = note.midiPitch;
    // Клавиши двигают по полутонам от ближайшего целого (как snap-сетка).
    const float base = std::round(oldPitch);
    const float newPitch = qBound(0.0f, base + float(semitoneDelta), 127.0f);
    if (std::abs(newPitch - oldPitch) < 1.0e-4f) {
        return;
    }
    note.midiPitch = newPitch;
    update();
    emit notePitchEdited(selectedNoteIndex, oldPitch, newPitch);
}

float PitchGridWidget::pitchToContentY(float midiPitch) const
{
    return (float(maxPitch) - midiPitch) * float(kPitchRowHeightPx)
        - float(verticalScrollPixels());
}

int PitchGridWidget::getPitchFromY(int y) const
{
    return int(std::lround(getContinuousPitchFromY(float(y))));
}

float PitchGridWidget::getContinuousPitchFromY(float y) const
{
    const float adjustedY = y + float(verticalScrollPixels());
    const float pitch = float(maxPitch) - adjustedY / float(kPitchRowHeightPx);
    return qBound(float(minPitch), pitch, float(maxPitch));
}

void PitchGridWidget::drawPlaybackCursor(QPainter& painter, const QRect& rect) const
{
    const float cursorX = rect.left() + timelineToContentX(cursorXPosition);
    if (cursorX < -10.0f || cursorX > rect.width() + 10.0f) {
        return;
    }

    painter.setPen(QPen(theme.cursor, 2));
    painter.drawLine(QPointF(cursorX, rect.top()), QPointF(cursorX, rect.bottom()));

    QPolygonF triangle;
    triangle << QPointF(cursorX - 5, rect.top() + 5)
             << QPointF(cursorX + 5, rect.top() + 5)
             << QPointF(cursorX, rect.top() + 15);
    painter.setBrush(theme.cursor);
    painter.drawPolygon(triangle);
}

qint64 PitchGridWidget::getPositionFromX(int x) const
{
    if (audioData.isEmpty() || sampleRate <= 0) {
        return 0;
    }

    const auto viewport = currentViewport();
    qint64 sample = viewport.pixelToSample(int(contentToTimelineX(float(x))));
    sample = PianoRollEngine::snapToGrid(sample, bpm, beatsPerBar, sampleRate,
                                         gridStartSample, beatGridSnap);
    const qint64 maxSample = qMax<qint64>(0, effectiveTimelineSamples() - 1);
    sample = qBound(qint64(0), sample, maxSample);
    return (sample * 1000) / sampleRate;
}

void PitchGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        lastMousePos = event->pos();

        // Режим «Разделить»: клик по ноте режет её и не двигает каретку
        if (splitModeActive && !isInLegendArea(event->pos().x())) {
            requestNoteSplit(noteIndexAt(event->pos()), cutSampleFromX(event->pos().x()));
            return;
        }

        // Сначала пробуем захватить ноту: drag по вертикали меняет её высоту.
        // Alt + ЛКМ — свободный (дробный) питч мимо полутоновой сетки, как в Melodyne.
        const int noteIndex = noteIndexAt(event->pos());
        if (noteIndex >= 0) {
            selectedNoteIndex = noteIndex;
            isNoteDragging = true;
            noteDragFreePitch = (event->modifiers() & Qt::AltModifier) != 0;
            noteDragStartPitch = pitchNotes[noteIndex].midiPitch;
            setCursor(Qt::SizeVerCursor);
            update();
            emit notePreviewRequested(noteIndex);
            return;
        }

        if (selectedNoteIndex >= 0) {
            selectedNoteIndex = -1;
            update();
        }

        isDragging = true;

        selectedPitch = getPitchFromY(event->pos().y());
        emit pitchChanged(selectedPitch);

        cursorXPosition = contentToTimelineX(float(event->pos().x()));
        update();
        emit positionChanged(getPositionFromX(event->pos().x()));
    } else if (event->button() == Qt::RightButton) {
        isRightMousePanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void PitchGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (splitModeActive && !(event->buttons() & Qt::RightButton)) {
        updateSplitPreview(event->pos());
        return;
    }

    if (isRightMousePanning && (event->buttons() & Qt::RightButton)) {
        const QPoint delta = event->pos() - lastMousePos;
        const int refWidth = qMax(1, timelineReferenceWidth());
        adjustHorizontalOffset(-float(delta.x()) / (refWidth * zoomLevel));
        lastMousePos = event->pos();
        return;
    }

    if (isNoteDragging && (event->buttons() & Qt::LeftButton)) {
        if (selectedNoteIndex >= 0 && selectedNoteIndex < pitchNotes.size()) {
            // Alt можно зажать/отпустить во время drag.
            noteDragFreePitch = (event->modifiers() & Qt::AltModifier) != 0;
            float targetPitch = getContinuousPitchFromY(float(event->pos().y()));
            if (!noteDragFreePitch) {
                targetPitch = std::round(targetPitch);
            }
            targetPitch = qBound(0.0f, targetPitch, 127.0f);
            if (std::abs(targetPitch - pitchNotes[selectedNoteIndex].midiPitch) > 1.0e-4f) {
                pitchNotes[selectedNoteIndex].midiPitch = targetPitch;
                update();
                emit notePreviewPitchChanged(selectedNoteIndex, targetPitch);
            }
        }
        return;
    }

    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        selectedPitch = getPitchFromY(event->pos().y());
        emit pitchChanged(selectedPitch);

        cursorXPosition = contentToTimelineX(float(event->pos().x()));
        update();
        emit positionChanged(getPositionFromX(event->pos().x()));
        lastMousePos = event->pos();
        return;
    }

    if (!(event->buttons() & Qt::LeftButton) && !(event->buttons() & Qt::RightButton)) {
        setCursor(noteIndexAt(event->pos()) >= 0 ? Qt::SizeVerCursor
                                                 : Qt::OpenHandCursor);
    }
}

void PitchGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (isNoteDragging) {
            isNoteDragging = false;
            noteDragFreePitch = false;
            setCursor(Qt::ArrowCursor);
            emit notePreviewStopped();
            if (selectedNoteIndex >= 0 && selectedNoteIndex < pitchNotes.size()) {
                const float newPitch = pitchNotes[selectedNoteIndex].midiPitch;
                if (std::abs(newPitch - noteDragStartPitch) > 1.0e-4f) {
                    emit notePitchEdited(selectedNoteIndex, noteDragStartPitch, newPitch);
                }
            }
        }
        isDragging = false;
    } else if (event->button() == Qt::RightButton) {
        isRightMousePanning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void PitchGridWidget::leaveEvent(QEvent *event)
{
    clearSplitPreview();
    QWidget::leaveEvent(event);
}

bool PitchGridWidget::event(QEvent *event)
{
    // Клавиша реза перехватывается до QShortcut приложения: пока фокус на
    // пианоролле, режет сам виджет (в плагинах главного окна нет вовсе),
    // и глобальный шорткат не сработает вторым разрезом.
    if (event->type() == QEvent::ShortcutOverride
        && matchesSplitShortcut(static_cast<QKeyEvent*>(event))) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

void PitchGridWidget::keyPressEvent(QKeyEvent *event)
{
    if (matchesSplitShortcut(event)) {
        splitNoteAtPlaybackCursor();
        event->accept();
        return;
    }

    if (splitModeActive && event->key() == Qt::Key_Escape) {
        setSplitModeActive(false);
        event->accept();
        return;
    }

    if (selectedNoteIndex >= 0
        && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        const int step = (event->modifiers() & Qt::ShiftModifier) ? 12 : 1;
        changeSelectedNotePitch(event->key() == Qt::Key_Up ? step : -step);
        event->accept();
        return;
    }

    if (selectedNoteIndex >= 0 && event->key() == Qt::Key_Escape) {
        selectedNoteIndex = -1;
        update();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
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
        const float pixelDelta = -(wheelDelta / 120.0f) * kRowsPerWheelStep * float(kPitchRowHeightPx);
        adjustVerticalOffset(pixelDelta / float(maxScroll));
    }
    event->accept();
}
