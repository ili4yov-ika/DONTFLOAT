#ifndef PITCHGRIDWIDGET_H
#define PITCHGRIDWIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QSet>
#include <QtCore/QVector>
#include <QtGui/QKeySequence>
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
    /** Режим разреза нот: по делениям тактовой сетки или точно по позиции. */
    enum class CutMode {
        SnapToGrid, ///< «Вдоль сетки»: рез притягивается к ближайшему делению
        Free        ///< «Свободный рез»: рез точно там, где каретка/клик
    };
    Q_ENUM(CutMode)

    /** Почему разрез не состоялся (для сообщения в строке состояния). */
    enum class SplitRejection {
        NoNoteAtCursor, ///< В точке реза нет ноты
        CutOutsideNote  ///< После привязки к сетке рез вышел за пределы ноты
    };
    Q_ENUM(SplitRejection)

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
    /** Длина таймлайна в сэмплах — по ней ставятся полосы над пианороллом. */
    qint64 timelineSamples() const { return timelineSampleCount; }
    float playbackCursorContentX() const;
    void setBPM(float bpm);
    void setBeatsPerBar(int beatsPerBar);
    void setGridStartSample(qint64 sample);
    void setPitchRange(int minPitch, int maxPitch);
    void setColorScheme(const QString& scheme);
    void setBeatGridSnapEnabled(bool enabled);
    /**
     * Замки перемещения нот мышью. По умолчанию горизонталь заблокирована
     * (ноту нельзя увести по времени случайным движением), вертикаль открыта —
     * высота правится как раньше.
     */
    void setHorizontalMoveLocked(bool locked);
    void setVerticalMoveLocked(bool locked);
    bool isHorizontalMoveLocked() const { return horizontalMoveLocked; }
    bool isVerticalMoveLocked() const { return verticalMoveLocked; }
    void setPrimaryKey(const QString& keyText);
    void setSecondaryKey(const QString& keyText);

    // Ноты (результат анализа), координаты — сэмплы текущего таймлайна
    void setNotes(const QVector<PitchDetector::PitchNote>& newNotes);
    const QVector<PitchDetector::PitchNote>& notes() const { return pitchNotes; }
    void clearNotes();

    /**
     * Референсные ноты из импортированного MIDI: рисуются серым позади своих,
     * не участвуют ни в выделении, ни в разрезе, ни в коррекции.
     */
    void setReferenceNotes(const QVector<PitchDetector::PitchNote>& referenceNotes);
    const QVector<PitchDetector::PitchNote>& referenceNotes() const { return referenceNotes_; }
    void clearReferenceNotes();
    /** Меняет высоту одной ноты (для undo/redo), без сигналов. */
    void setNotePitch(int noteIndex, float midiPitch);

    // --- Разрез нот (панель кнопок под пианороллом) ---
    void setCutMode(CutMode mode);
    CutMode cutMode() const { return noteCutMode; }
    /** Режим «Разделить»: клик по ноте режет её (до повторного выключения). */
    void setSplitModeActive(bool active);
    bool isSplitModeActive() const { return splitModeActive; }
    /** Клавиша разреза по каретке (по умолчанию S); пустая — выключить. */
    void setSplitShortcut(const QKeySequence& sequence);
    QKeySequence splitShortcutKey() const { return splitShortcut; }
    /**
     * Режет ноту под кареткой воспроизведения по текущему режиму реза.
     * @return true, если запрос на разрез отправлен (см. noteSplitRequested).
     */
    bool splitNoteAtPlaybackCursor();

signals:
    void positionChanged(qint64 position);
    void pitchChanged(int pitch);
    void horizontalOffsetChanged(float offset);
    void verticalOffsetChanged(float offset);
    void timelineZoomRequested(int angleDeltaY, float timelinePixelX);
    /** Пользователь изменил высоту ноты (drag или клавиши). */
    void notePitchEdited(int noteIndex, float oldPitch, float newPitch);
    /** Ноту передвинули по времени (drag по горизонтали, если он разблокирован). */
    void noteTimeEdited(int noteIndex, qint64 oldStartSample, qint64 newStartSample);
    /** Блок ноты зажат мышью — начать зацикленное прослушивание. */
    void notePreviewRequested(int noteIndex);
    /** Высота удерживаемой ноты изменилась во время drag. */
    void notePreviewPitchChanged(int noteIndex, float midiPitch);
    /** Блок ноты отпущен — остановить прослушивание. */
    void notePreviewStopped();
    /** Режим «Разделить» включён/выключен (в т.ч. по Esc). */
    void splitModeChanged(bool active);
    /** Запрошен разрез ноты; splitSample — в сэмплах текущего таймлайна. */
    void noteSplitRequested(int noteIndex, qint64 splitSample);
    /** Разрез невозможен — хост может показать подсказку. */
    void noteSplitRejected(PitchGridWidget::SplitRejection reason);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
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
        QColor splitLine;
    };

    void drawPianoRollBackground(QPainter& painter, const QRect& rect) const;
    void drawLegendColumn(QPainter& painter, const QRect& legendRect) const;
    void drawBeatGrid(QPainter& painter, const QRect& rect) const;
    void drawSelectedPitchRow(QPainter& painter, const QRect& rect) const;
    void drawNoteBlocks(QPainter& painter, const QRect& rect) const;
    void drawReferenceNotes(QPainter& painter, const QRect& rect) const;
    void drawSplitPreview(QPainter& painter, const QRect& rect) const;
    void drawPlaybackCursor(QPainter& painter, const QRect& rect) const;

    QRectF noteRect(const PitchDetector::PitchNote& note) const;
    int noteIndexAt(const QPoint& pos) const;
    void changeSelectedNotePitch(int semitoneDelta);

    /** Сэмпл таймлайна под X с учётом режима реза (snap к сетке / свободно). */
    qint64 cutSampleFromX(int x) const;
    /** Сэмпл каретки воспроизведения в координатах таймлайна. */
    qint64 playbackCursorSample() const;
    /** Индекс ноты, содержащей сэмпл (приоритет — у выделенной). */
    int noteIndexContainingSample(qint64 sample) const;
    /** Общая часть разреза: проверки + сигнал. */
    bool requestNoteSplit(int noteIndex, qint64 rawSample);
    void updateSplitPreview(const QPoint& pos);
    void clearSplitPreview();
    bool matchesSplitShortcut(const QKeyEvent* keyEvent) const;
    void applyCursorShape();
    /** Курсор захвата ноты: показывает, какие направления открыты. */
    Qt::CursorShape noteDragCursor() const;

    int getPitchFromY(int y) const;
    float getContinuousPitchFromY(float y) const;
    float pitchToContentY(float midiPitch) const;
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
    /** Ноты референсного MIDI — только фон (см. setReferenceNotes). */
    QVector<PitchDetector::PitchNote> referenceNotes_;
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
    /** Активная нота: её ведёт перетаскивание и с неё начинается группа. */
    int selectedNoteIndex;
    /**
     * Группа выделенных нот: правки высоты идут по всей группе сразу.
     *
     * Shift+ЛКМ добавляет ноту в группу, Ctrl+ЛКМ исключает — так же, как
     * Shift работает с метками растяжения на волне.
     */
    QSet<int> selectedNotes;
    bool isNoteDragging;
    float noteDragStartPitch;
    bool noteDragFreePitch;
    /** Замки перемещения нот: горизонталь по умолчанию закрыта. */
    bool horizontalMoveLocked = true;
    bool verticalMoveLocked = false;
    /** Начало ноты и позиция мыши на момент захвата — для сдвига по времени. */
    qint64 noteDragStartSample = 0;
    qint64 noteDragGrabSample = 0;

    CutMode noteCutMode;
    bool splitModeActive;
    /** Подсветка будущего реза под курсором: -1 — нет ноты под мышью. */
    int splitPreviewNoteIndex;
    qint64 splitPreviewSample;
    bool splitPreviewValid;
    QKeySequence splitShortcut;

    static constexpr int kLegendColumnWidthPx = 36;
    static constexpr int kLegendBackgroundAlpha = 165;
    static constexpr int kPitchRowHeightPx = 16;
    static constexpr int kMinVisiblePitchRows = 4;
    static constexpr int kMinPitchDefault = 12;  // C0
    static constexpr int kMaxPitchDefault = 84;  // C6
};

#endif // PITCHGRIDWIDGET_H
