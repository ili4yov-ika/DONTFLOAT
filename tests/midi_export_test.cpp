// Экспорт нот пианоролла в SMF: файл читается обратно тем же разбором, что
// используется для тестовых фикстур tests/midi/*.mid.

#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include "../include/midiexporter.h"
#include "midi_smf.h"

#include <cmath>

namespace {

constexpr int kSampleRate = 48000;
constexpr float kBpm = 120.0f;

/** Нота из долей: старт и длина в четвертях при 120 BPM. */
PitchDetector::PitchNote noteAt(double startQuarters, double lengthQuarters, float midiPitch)
{
    const double samplesPerQuarter = (60.0 / double(kBpm)) * double(kSampleRate);
    PitchDetector::PitchNote note;
    note.startSample = qint64(startQuarters * samplesPerQuarter);
    note.endSample = qint64((startQuarters + lengthQuarters) * samplesPerQuarter);
    note.midiPitch = midiPitch;
    note.detectedPitch = midiPitch;
    note.confidence = 1.0f;
    return note;
}

} // namespace

class MidiExportTest : public QObject
{
    Q_OBJECT

private slots:
    void testNotesRoundTrip();
    void testEmptyNotesRejected();
};

void MidiExportTest::testNotesRoundTrip()
{
    QVector<PitchDetector::PitchNote> notes;
    notes.append(noteAt(0.0, 1.0, 60.0f));   // C4, четверть с начала
    notes.append(noteAt(1.0, 0.5, 64.0f));   // E4, восьмая
    notes.append(noteAt(2.0, 2.0, 67.4f));   // G4 (округление вниз), половина

    MidiExporter::Options options;
    options.bpm = kBpm;
    options.sampleRate = kSampleRate;

    const QString path = QDir::temp().filePath(QStringLiteral("dontfloat_midi_export_test.mid"));
    QString error;
    QVERIFY2(MidiExporter::writeFile(path, notes, options, &error), qPrintable(error));

    const MidiSmf::Song song = MidiSmf::loadFile(path);
    QCOMPARE(song.ticksPerQuarter, MidiExporter::kTicksPerQuarter);
    QVERIFY(std::fabs(song.bpm - kBpm) < 0.5f);
    QCOMPARE(song.notes.size(), notes.size());

    // Высоты и позиции совпадают с исходными нотами
    const int ticksPerQuarter = MidiExporter::kTicksPerQuarter;
    QCOMPARE(song.notes[0].pitch, 60);
    QCOMPARE(song.notes[0].startTick, 0);
    QCOMPARE(song.notes[0].endTick - song.notes[0].startTick, ticksPerQuarter);

    QCOMPARE(song.notes[1].pitch, 64);
    QCOMPARE(song.notes[1].startTick, ticksPerQuarter);
    QCOMPARE(song.notes[1].endTick - song.notes[1].startTick, ticksPerQuarter / 2);

    QCOMPARE(song.notes[2].pitch, 67);  // 67.4 → 67
    QCOMPARE(song.notes[2].startTick, 2 * ticksPerQuarter);
    QCOMPARE(song.notes[2].endTick - song.notes[2].startTick, 2 * ticksPerQuarter);

    QFile::remove(path);
}

void MidiExportTest::testEmptyNotesRejected()
{
    const QString path = QDir::temp().filePath(QStringLiteral("dontfloat_midi_export_empty.mid"));
    QString error;
    QVERIFY(!MidiExporter::writeFile(path, {}, MidiExporter::Options {}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFile::exists(path));
}

QTEST_MAIN(MidiExportTest)
#include "midi_export_test.moc"
