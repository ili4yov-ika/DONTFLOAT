// Перестановка нот должна быть слышна, а не только видна.
//
// Ноты A B C D переставляем в порядок C D A B и проверяем, что коррекция
// действительно переносит звук: на месте A теперь звучит C и так далее.
// Раньше `PitchCorrection::apply` брала сегмент по нынешней позиции ноты,
// поэтому дорожка играла в прежнем порядке.

#include <QtTest/QTest>

#include "../include/pitchcorrection.h"
#include "../include/pitchdetector.h"
#include "../include/pitchnotemovecommand.h"
#include "../include/pitchnotesplitcommand.h"

#include <cmath>

namespace {

constexpr int kSampleRate = 44100;
constexpr int kNoteSamples = 22050;  // 0.5 с на ноту
constexpr int kNoteCount = 4;

/** Четыре подряд идущие ноты, каждая — постоянный уровень («A», «B», «C», «D»). */
QVector<QVector<float>> makeMarkedAudio()
{
    QVector<float> mono(kNoteSamples * kNoteCount, 0.0f);
    const float levels[kNoteCount] = { 0.2f, 0.4f, 0.6f, 0.8f };
    for (int note = 0; note < kNoteCount; ++note) {
        for (int i = 0; i < kNoteSamples; ++i) {
            mono[note * kNoteSamples + i] = levels[note];
        }
    }
    return { mono };
}

PitchDetector::PitchNote makeNote(int index)
{
    PitchDetector::PitchNote note;
    note.startSample = qint64(index) * kNoteSamples;
    note.endSample = note.startSample + kNoteSamples;
    note.midiPitch = 60.0f;
    note.detectedPitch = 60.0f;
    note.confidence = 1.0f;
    return note;
}

/** Средний уровень в середине слота \a index (края смазаны кроссфейдом). */
float levelAtSlot(const QVector<float>& channel, int index)
{
    const int from = index * kNoteSamples + kNoteSamples / 4;
    const int to = index * kNoteSamples + (kNoteSamples * 3) / 4;
    double sum = 0.0;
    for (int i = from; i < to; ++i) {
        sum += std::abs(double(channel[i]));
    }
    return float(sum / double(to - from));
}

} // namespace

class NoteMoveRenderTest : public QObject
{
    Q_OBJECT

private slots:
    void testMovedNotesSwapAudio();
    void testMoveMarksPendingEdits();
    void testUndoOfMoveRestoresOriginalOrder();
    void testSplitKeepsSourceRangesOfBothHalves();
};

// A B C D → C D A B: звук едет вместе с нотами
void NoteMoveRenderTest::testMovedNotesSwapAudio()
{
    const QVector<QVector<float>> audio = makeMarkedAudio();
    QVector<PitchDetector::PitchNote> notes;
    for (int i = 0; i < kNoteCount; ++i) {
        notes.append(makeNote(i));
    }

    // Первая пара уезжает во вторую половину, вторая — в первую
    for (int i = 0; i < kNoteCount; ++i) {
        const int targetSlot = (i + 2) % kNoteCount;
        PitchNoteMoveCommand command(nullptr, &notes, i, notes[i].startSample,
                                     qint64(targetSlot) * kNoteSamples, QStringLiteral("move"));
        command.redo();
    }
    QCOMPARE(notes[0].startSample, qint64(2) * kNoteSamples);
    QCOMPARE(notes[0].sourceStart(), qint64(0));

    const QVector<QVector<float>> out = PitchCorrection::apply(audio, notes, kSampleRate);
    QCOMPARE(out.size(), audio.size());
    QCOMPARE(out[0].size(), audio[0].size());

    // В слоте 0 теперь звучит бывшая нота C (уровень 0.6), в слоте 1 — D
    QVERIFY2(std::abs(levelAtSlot(out[0], 0) - 0.6f) < 0.05f,
             qPrintable(QStringLiteral("слот 0: %1").arg(levelAtSlot(out[0], 0))));
    QVERIFY2(std::abs(levelAtSlot(out[0], 1) - 0.8f) < 0.05f,
             qPrintable(QStringLiteral("слот 1: %1").arg(levelAtSlot(out[0], 1))));
    QVERIFY2(std::abs(levelAtSlot(out[0], 2) - 0.2f) < 0.05f,
             qPrintable(QStringLiteral("слот 2: %1").arg(levelAtSlot(out[0], 2))));
    QVERIFY2(std::abs(levelAtSlot(out[0], 3) - 0.4f) < 0.05f,
             qPrintable(QStringLiteral("слот 3: %1").arg(levelAtSlot(out[0], 3))));
}

// Кнопка «Применить коррекцию» должна загораться и от одного лишь переноса
void NoteMoveRenderTest::testMoveMarksPendingEdits()
{
    QVector<PitchDetector::PitchNote> notes { makeNote(0) };
    QVERIFY(!PitchCorrection::hasPendingEdits(notes));

    PitchNoteMoveCommand command(nullptr, &notes, 0, notes[0].startSample,
                                 qint64(2) * kNoteSamples, QStringLiteral("move"));
    command.redo();
    QVERIFY(PitchCorrection::hasPendingEdits(notes));
    QVERIFY(notes[0].isMovedInTime());
}

// Ctrl+Z возвращает ноту на место — и звук вместе с ней
void NoteMoveRenderTest::testUndoOfMoveRestoresOriginalOrder()
{
    const QVector<QVector<float>> audio = makeMarkedAudio();
    QVector<PitchDetector::PitchNote> notes;
    for (int i = 0; i < kNoteCount; ++i) {
        notes.append(makeNote(i));
    }

    PitchNoteMoveCommand command(nullptr, &notes, 0, notes[0].startSample,
                                 qint64(3) * kNoteSamples, QStringLiteral("move"));
    command.redo();
    QVERIFY(notes[0].isMovedInTime());

    command.undo();
    QVERIFY(!notes[0].isMovedInTime());
    QCOMPARE(notes[0].startSample, qint64(0));

    // Без правок коррекция возвращает исходный звук как есть
    const QVector<QVector<float>> out = PitchCorrection::apply(audio, notes, kSampleRate);
    QCOMPARE(out[0], audio[0]);
}

// Разрез делит и исходный отрезок: половинки берут свои куски звука
void NoteMoveRenderTest::testSplitKeepsSourceRangesOfBothHalves()
{
    QVector<PitchDetector::PitchNote> notes { makeNote(0) };
    const qint64 cut = kNoteSamples / 2;

    PitchNoteSplitCommand command(&notes, 0, cut, QStringLiteral("split"));
    command.redo();
    QCOMPARE(notes.size(), 2);
    QCOMPARE(notes[0].sourceStart(), qint64(0));
    QCOMPARE(notes[0].sourceEnd(), cut);
    QCOMPARE(notes[1].sourceStart(), cut);
    QCOMPARE(notes[1].sourceEnd(), qint64(kNoteSamples));
    // Половинки стоят на своих местах — переносом это не считается
    QVERIFY(!notes[0].isMovedInTime());
    QVERIFY(!notes[1].isMovedInTime());
}

QTEST_MAIN(NoteMoveRenderTest)
#include "note_move_render_test.moc"
