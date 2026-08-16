#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtCore/QVector>

#include "../include/pianoroll_engine.h"
#include "../include/pitchdetector.h"
#include "../include/pitchgridwidget.h"
#include "../include/pitchnotesplitcommand.h"

/**
 * Логика разреза нот на пианоролле: привязка реза к тактовой сетке
 * («Вдоль сетки») против свободного реза, допустимость реза и undo/redo
 * команды PitchNoteSplitCommand.
 */
class PianoRollSplitTest : public QObject
{
    Q_OBJECT

private slots:
    void testGridCutSnapsToSubdivision();
    void testFreeCutKeepsExactSample();
    void testCanSplitRejectsEdgesAndShortNotes();
    void testSplitCommandSplitsAndRestores();
    void testSplitCommandKeepsPitchInBothParts();

    // Пианоролл: клик инструментом «Разделить» и горячая клавиша
    void testWidgetFreeCutUsesClickedSample();
    void testWidgetGridCutSnapsClickToGrid();
    void testWidgetClickWithoutNoteIsRejected();
    void testWidgetSplitModeKeepsPlaybackCursor();
    void testWidgetShortcutCutsAtPlaybackCursor();

private:
    static constexpr int kSampleRate = 44100;
    static constexpr float kBpm = 120.0f;
    static constexpr int kBeatsPerBar = 4;

    // Таймлайн теста: 10 с при 1000 px → 441 сэмпл на пиксель
    static constexpr int kWidgetWidth = 1000;
    static constexpr int kWidgetHeight = 300;
    static constexpr qint64 kTotalSamples = 441000;
    static constexpr qint64 kSamplesPerPixel = 441;
    static constexpr int kNotePitch = 60;
    static constexpr qint64 kNoteStart = 22050;   // 0.5 с
    static constexpr qint64 kNoteEnd = 110250;    // 2.5 с

    static PitchDetector::PitchNote makeNote(qint64 start, qint64 end, float pitch)
    {
        PitchDetector::PitchNote note;
        note.startSample = start;
        note.endSample = end;
        note.midiPitch = pitch;
        note.detectedPitch = pitch;
        note.confidence = 0.9f;
        return note;
    }

    /** Пианоролл с одной нотой C4 и таймлайном 1 px = 441 сэмплов. */
    void setUpWidget(PitchGridWidget& widget) const
    {
        QVector<QVector<float>> audio;
        audio.append(QVector<float>(int(kTotalSamples), 0.0f));

        widget.resize(kWidgetWidth, kWidgetHeight);
        widget.setSampleRate(kSampleRate);
        widget.setAudioData(audio);
        widget.setTimelineSampleCount(kTotalSamples);
        widget.setTimelineReferenceWidth(kWidgetWidth);
        widget.setZoomLevel(1.0f);
        widget.setHorizontalOffset(0.0f);
        widget.setBPM(kBpm);
        widget.setBeatsPerBar(kBeatsPerBar);
        widget.setGridStartSample(0);
        // Диапазон подобран так, чтобы нота C4 попадала в видимую область
        widget.setPitchRange(48, 72);
        widget.setNotes({ makeNote(kNoteStart, kNoteEnd, float(kNotePitch)) });
    }

    /** Y центра строки ноты (сверху вниз от maxPitch). */
    static int noteRowY(int maxPitch, int pitch) { return (maxPitch - pitch) * 16 + 8; }
};

void PianoRollSplitTest::testGridCutSnapsToSubdivision()
{
    // 120 BPM, 4/4 → доля 22050 сэмплов, деление сетки = доля
    const auto metrics = PianoRollEngine::computeBeatGridMetrics(kBpm, kBeatsPerBar, kSampleRate);
    QCOMPARE(qint64(metrics.samplesPerSubdivision), qint64(22050));

    QCOMPARE(PianoRollEngine::snapToGrid(25000, metrics, 0, true), qint64(22050));
    QCOMPARE(PianoRollEngine::snapToGrid(40000, metrics, 0, true), qint64(44100));
    // Сетка со смещённым началом трека
    QCOMPARE(PianoRollEngine::snapToGrid(30000, metrics, 5000, true), qint64(27050));
}

void PianoRollSplitTest::testFreeCutKeepsExactSample()
{
    const auto metrics = PianoRollEngine::computeBeatGridMetrics(kBpm, kBeatsPerBar, kSampleRate);
    QCOMPARE(PianoRollEngine::snapToGrid(25000, metrics, 0, false), qint64(25000));
    QCOMPARE(PianoRollEngine::snapToGrid(1, metrics, 0, false), qint64(1));
}

void PianoRollSplitTest::testCanSplitRejectsEdgesAndShortNotes()
{
    constexpr qint64 start = 1000;
    constexpr qint64 end = 5000;

    QVERIFY(PianoRollEngine::canSplitNoteAt(start, end, 3000));
    // Рез по краю ноты не делит её на две части
    QVERIFY(!PianoRollEngine::canSplitNoteAt(start, end, start));
    QVERIFY(!PianoRollEngine::canSplitNoteAt(start, end, end));
    // Рез вне ноты
    QVERIFY(!PianoRollEngine::canSplitNoteAt(start, end, 500));
    QVERIFY(!PianoRollEngine::canSplitNoteAt(start, end, 9000));
    // Слишком близко к краю: часть короче kMinNotePartSamples
    QVERIFY(!PianoRollEngine::canSplitNoteAt(start, end, start + 4));
    QVERIFY(!PianoRollEngine::canSplitNoteAt(start, end, end - 4));
    // Нота короче двух минимальных частей не делится вовсе
    QVERIFY(!PianoRollEngine::canSplitNoteAt(0, 2 * PianoRollEngine::kMinNotePartSamples - 1,
                                             PianoRollEngine::kMinNotePartSamples));
}

void PianoRollSplitTest::testSplitCommandSplitsAndRestores()
{
    QVector<PitchDetector::PitchNote> notes;
    notes << makeNote(0, 10000, 60.0f)
          << makeNote(20000, 30000, 62.0f);

    int applied = 0;
    PitchNoteSplitCommand command(&notes, 0, 4000, QStringLiteral("split"),
                                  [&applied]() { ++applied; });

    command.redo();
    QCOMPARE(notes.size(), 3);
    QCOMPARE(notes[0].startSample, qint64(0));
    QCOMPARE(notes[0].endSample, qint64(4000));
    QCOMPARE(notes[1].startSample, qint64(4000));
    QCOMPARE(notes[1].endSample, qint64(10000));
    // Следующая нота сдвинулась по индексу, но осталась прежней
    QCOMPARE(notes[2].startSample, qint64(20000));
    QCOMPARE(applied, 1);

    command.undo();
    QCOMPARE(notes.size(), 2);
    QCOMPARE(notes[0].startSample, qint64(0));
    QCOMPARE(notes[0].endSample, qint64(10000));
    QCOMPARE(notes[1].startSample, qint64(20000));
    QCOMPARE(applied, 2);

    // Повторный redo восстанавливает то же разбиение
    command.redo();
    QCOMPARE(notes.size(), 3);
    QCOMPARE(notes[0].endSample, qint64(4000));
}

void PianoRollSplitTest::testSplitCommandKeepsPitchInBothParts()
{
    QVector<PitchDetector::PitchNote> notes;
    notes << makeNote(0, 10000, 60.0f);
    notes[0].midiPitch = 63.5f; // нота уже отредактирована пользователем

    PitchNoteSplitCommand command(&notes, 0, 5000, QStringLiteral("split"));
    command.redo();

    QCOMPARE(notes.size(), 2);
    QCOMPARE(notes[0].midiPitch, 63.5f);
    QCOMPARE(notes[1].midiPitch, 63.5f);
    QCOMPARE(notes[0].detectedPitch, 60.0f);
    QCOMPARE(notes[1].detectedPitch, 60.0f);

    command.undo();
    QCOMPARE(notes.size(), 1);
    QCOMPARE(notes[0].midiPitch, 63.5f);
    QCOMPARE(notes[0].endSample, qint64(10000));
}

void PianoRollSplitTest::testWidgetFreeCutUsesClickedSample()
{
    PitchGridWidget widget;
    setUpWidget(widget);
    widget.setCutMode(PitchGridWidget::CutMode::Free);
    widget.setSplitModeActive(true);

    QSignalSpy spy(&widget, &PitchGridWidget::noteSplitRequested);
    const QPoint pos(160, noteRowY(72, kNotePitch));
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 0);
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(160 * kSamplesPerPixel));
}

void PianoRollSplitTest::testWidgetGridCutSnapsClickToGrid()
{
    PitchGridWidget widget;
    setUpWidget(widget);
    widget.setCutMode(PitchGridWidget::CutMode::SnapToGrid);
    widget.setSplitModeActive(true);

    QSignalSpy spy(&widget, &PitchGridWidget::noteSplitRequested);
    // 160 px = 70560 сэмплов — 3.2 доли; ближайшее деление сетки — 3-я доля
    const QPoint pos(160, noteRowY(72, kNotePitch));
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(66150));
}

void PianoRollSplitTest::testWidgetClickWithoutNoteIsRejected()
{
    PitchGridWidget widget;
    setUpWidget(widget);
    widget.setSplitModeActive(true);

    QSignalSpy splitSpy(&widget, &PitchGridWidget::noteSplitRequested);
    QSignalSpy rejectSpy(&widget, &PitchGridWidget::noteSplitRejected);

    // Та же позиция по времени, но строкой ниже ноты
    const QPoint pos(160, noteRowY(72, kNotePitch - 3));
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(splitSpy.count(), 0);
    QCOMPARE(rejectSpy.count(), 1);
}

void PianoRollSplitTest::testWidgetSplitModeKeepsPlaybackCursor()
{
    PitchGridWidget widget;
    setUpWidget(widget);
    widget.setSplitModeActive(true);

    // В режиме реза клик не двигает каретку и не тянет высоту ноты
    QSignalSpy positionSpy(&widget, &PitchGridWidget::positionChanged);
    QSignalSpy pitchSpy(&widget, &PitchGridWidget::notePitchEdited);
    QSignalSpy previewSpy(&widget, &PitchGridWidget::notePreviewRequested);

    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier,
                      QPoint(160, noteRowY(72, kNotePitch)));

    QCOMPARE(positionSpy.count(), 0);
    QCOMPARE(pitchSpy.count(), 0);
    QCOMPARE(previewSpy.count(), 0);

    // Выключение режима возвращает обычное поведение: каретка едет за кликом
    widget.setSplitModeActive(false);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(400, 40));
    QCOMPARE(positionSpy.count(), 1);
}

void PianoRollSplitTest::testWidgetShortcutCutsAtPlaybackCursor()
{
    PitchGridWidget widget;
    setUpWidget(widget);
    widget.setCutMode(PitchGridWidget::CutMode::Free);
    // Каретка на 1.6 с — внутри ноты; режим «Разделить» для клавиши не нужен
    widget.setPlaybackPosition(1600);

    QSignalSpy spy(&widget, &PitchGridWidget::noteSplitRequested);
    QTest::keyClick(&widget, Qt::Key_S);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 0);
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(70560));

    // Каретка вне ноты — разрез отклоняется
    QSignalSpy rejectSpy(&widget, &PitchGridWidget::noteSplitRejected);
    widget.setPlaybackPosition(5000);
    QTest::keyClick(&widget, Qt::Key_S);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(rejectSpy.count(), 1);
}

QTEST_MAIN(PianoRollSplitTest)
#include "pianoroll_split_test.moc"
