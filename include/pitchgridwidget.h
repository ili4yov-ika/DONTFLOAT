#ifndef PITCHGRIDWIDGET_H
#define PITCHGRIDWIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include "pianoroll_engine.h"
#include "pitchdetector.h"

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
    void setPrimaryKey(const QString& keyText);
    void setSecondaryKey(const QString& keyText);

    // Ноты (результат анализа), координаты — сэмплы текущего таймлайна
    void setNotes(const QVector<PitchDetector::PitchNote>& newNotes);
    const QVector<PitchDetector::PitchNote>& notes() const { return pitchNotes; }
    void clearNotes();
    /** Меняет высоту одной ноты (для undo/redo), без сигналов. */
    void setNotePitch(int noteIndex, int midiPitch);

signals:
    void positionChanged(qint64 position);
    void pitchChanged(int pitch);
    void horizontalOffsetChanged(float offset);
    void verticalOffsetChanged(float offset);
    void timelineZoomRequested(int angleDeltaY, float timelinePixelX);
    /** Пользователь изменил высоту ноты (drag или клавиши). */
    void notePitchEdited(int noteIndex, int oldPitch, int newPitch);
    /** Блок ноты зажат мышью — начать зацикленное прослушивание. */
    void notePreviewRequested(int noteIndex);
    /** Высота удерживаемой ноты изменилась во время drag. */
    void notePreviewPitchChanged(int noteIndex, int midiPitch);
    /** Блок ноты отпущен — остановить прослушивание. */
    void notePreviewStopped();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Theme {
        QColor background;
        QColor whiteKey;
        QColor blackKey;
        QColor outOfKeyKey;
        QColor gridLine;
        QColor beatLine;
        QColor barLine;
        QColor cursor;
        QColor labelText;
        QColor outOfKeyLegendText;
        QColor labelBackground;
        QColor selection;
        QColor octaveAccent;
        QColor legendBorder;
        QColor noteFill;
        QColor noteEditedFill;
        QColor noteBorder;
        QColor noteSelectedBorder;
    };

    void drawPianoRollBackground(QPainter& painter, const QRect& rect) const;
    void drawLegendColumn(QPainter& painter, const QRect& legendRect) const;
    void drawBeatGrid(QPainter& painter, const QRect& rect) const;
    void drawSelectedPitchRow(QPainter& painter, const QRect& rect) const;
    void drawNoteBlocks(QPainter& painter, const QRect& rect) const;
    void drawPlaybackCursor(QPainter& painter, const QRect& rect) const;

    QRectF noteRect(const PitchDetector::PitchNote& note) const;
    int noteIndexAt(const QPoint& pos) const;
    void changeSelectedNotePitch(int semitoneDelta);

    int getPitchFromY(int y) const;
    qint64 getPositionFromX(int x) const;
    PianoRollEngine::Viewport currentViewport() const;
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
    QRect legendRect() const;
    bool isInLegendArea(int x) const;
    QColor rowColorForPitch(int midiNote) const;
    QColor legendKeyColor(int midiNote) const;
    QColor legendTextColor(int midiNote) const;
    void applyTheme(const QString& scheme);

    QVector<QVector<float>> audioData;
    QVector<PitchDetector::PitchNote> pitchNotes;
    PianoRollEngine::KeySignature primaryKey;
    PianoRollEngine::KeySignature secondaryKey;

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
    Theme theme;
    int selectedPitch;
    int selectedNoteIndex;
    bool isNoteDragging;
    int noteDragStartPitch;

    static constexpr int kLegendColumnWidthPx = 36;
    static constexpr int kLegendBackgroundAlpha = 165;
    static constexpr int kPitchRowHeightPx = 16;
    static constexpr int kMinVisiblePitchRows = 4;
    static constexpr int kMinPitchDefault = 36;
    static constexpr int kMaxPitchDefault = 84;
};

#endif // PITCHGRIDWIDGET_H
