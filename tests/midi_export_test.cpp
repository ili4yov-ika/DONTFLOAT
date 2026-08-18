// Экспорт нот пианоролла в SMF: файл читается обратно тем же разбором, что
// используется для тестовых фикстур tests/midi/*.mid.

#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include "../include/midiexporter.h"
#include "../include/midiimporter.h"
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
    void testImportKeepsOwnTiming();
    void testImportFitsProjectBpm();
    void testImportAlignsToGridStart();
    void testImportDetectsKey();
    void testImportRejectsNonMidi();
    void testReferenceKeysSplitBarsOnModulation();
    void testReferenceKeysHoldThroughSilentBar();
};

namespace {

/** Пишет короткий референс во временный файл: три ноты по четверти. */
QString writeReferenceMidi(float bpm)
{
    QVector<PitchDetector::PitchNote> notes;
    const double samplesPerQuarter = (60.0 / double(bpm)) * double(kSampleRate);
    const int pitches[] = { 60, 64, 67 };  // до-мажорное трезвучие
    for (int i = 0; i < 3; ++i) {
        PitchDetector::PitchNote note;
        note.startSample = qint64(double(i) * samplesPerQuarter);
        note.endSample = qint64(double(i + 1) * samplesPerQuarter);
        note.midiPitch = float(pitches[i]);
        note.detectedPitch = note.midiPitch;
        note.confidence = 1.0f;
        notes.append(note);
    }

    MidiExporter::Options options;
    options.bpm = bpm;
    options.sampleRate = kSampleRate;

    const QString path = QDir::temp().filePath(QStringLiteral("dontfloat_reference_test.mid"));
    QString error;
    if (!MidiExporter::writeFile(path, notes, options, &error)) {
        return {};
    }
    return path;
}

} // namespace

// «Оставить как есть»: ноты идут в темпе файла, а не проекта
void MidiExportTest::testImportKeepsOwnTiming()
{
    const QString path = writeReferenceMidi(60.0f);  // одна четверть = 1 секунда
    QVERIFY(!path.isEmpty());

    MidiImporter::Options options;
    options.mode = MidiImporter::TimingMode::KeepAsIs;
    options.projectBpm = 120.0f;  // темп проекта другой — он не должен влиять
    options.sampleRate = kSampleRate;

    const MidiImporter::Result result = MidiImporter::readFile(path, options);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.notes.size(), 3);
    QVERIFY(std::fabs(result.sourceBpm - 60.0f) < 0.5f);

    const qint64 tolerance = kSampleRate / 100;
    QVERIFY(std::llabs(result.notes[1].startSample - kSampleRate) < tolerance);
    QVERIFY(std::llabs(result.notes[2].startSample - 2 * kSampleRate) < tolerance);

    QFile::remove(path);
}

// «Подогнать под BPM»: файл 60 BPM звучит в проекте на 120 — вдвое быстрее
void MidiExportTest::testImportFitsProjectBpm()
{
    const QString path = writeReferenceMidi(60.0f);
    QVERIFY(!path.isEmpty());

    MidiImporter::Options options;
    options.mode = MidiImporter::TimingMode::FitToBpm;
    options.projectBpm = 120.0f;
    options.sampleRate = kSampleRate;

    const MidiImporter::Result result = MidiImporter::readFile(path, options);
    QVERIFY2(result.ok, qPrintable(result.error));

    const qint64 tolerance = kSampleRate / 100;
    QVERIFY(std::llabs(result.notes[1].startSample - kSampleRate / 2) < tolerance);
    QVERIFY(std::llabs(result.notes[2].startSample - kSampleRate) < tolerance);

    QFile::remove(path);
}

// «Выровнять и подогнать»: первая нота встаёт на начало тактовой сетки
void MidiExportTest::testImportAlignsToGridStart()
{
    const QString path = writeReferenceMidi(120.0f);
    QVERIFY(!path.isEmpty());

    MidiImporter::Options options;
    options.mode = MidiImporter::TimingMode::AlignAndFitToBpm;
    options.projectBpm = 120.0f;
    options.sampleRate = kSampleRate;
    options.gridStartSample = 3 * kSampleRate;

    const MidiImporter::Result result = MidiImporter::readFile(path, options);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.notes.first().startSample, qint64(3 * kSampleRate));

    // Расстояния между нотами сохраняются — сдвигается весь набор целиком
    const qint64 tolerance = kSampleRate / 100;
    const qint64 step = result.notes[1].startSample - result.notes[0].startSample;
    QVERIFY(std::llabs(step - kSampleRate / 2) < tolerance);

    QFile::remove(path);
}

// Тональность референса определяется по нотам (до-мажорное трезвучие)
void MidiExportTest::testImportDetectsKey()
{
    const QString path = writeReferenceMidi(120.0f);
    QVERIFY(!path.isEmpty());

    MidiImporter::Options options;
    options.sampleRate = kSampleRate;
    const MidiImporter::Result result = MidiImporter::readFile(path, options);
    QVERIFY(result.ok);

    const QString key = MidiImporter::detectKey(result.notes);
    QVERIFY2(!key.isEmpty(), "тональность референса не определена");
    QVERIFY2(key.startsWith(QStringLiteral("C")), qPrintable(key));

    QFile::remove(path);
}

// Не-MIDI файл отвергается с текстом ошибки, а не падением
void MidiExportTest::testImportRejectsNonMidi()
{
    const QString path = QDir::temp().filePath(QStringLiteral("dontfloat_not_midi.bin"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not a midi file");
    file.close();

    const MidiImporter::Result result = MidiImporter::readFile(path, MidiImporter::Options {});
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.notes.isEmpty());

    QFile::remove(path);
}

namespace {

/** Мажорная гамма восьмыми на весь такт — однозначная тональность такта. */
void appendScaleBar(QVector<PitchDetector::PitchNote>& notes, int bar, int root)
{
    const double samplesPerEighth = (30.0 / double(kBpm)) * double(kSampleRate);
    const int degrees[] = { 0, 2, 4, 5, 7, 9, 11, 12 };
    for (int step = 0; step < 8; ++step) {
        PitchDetector::PitchNote note;
        note.startSample = qint64(double(bar * 8 + step) * samplesPerEighth);
        note.endSample = qint64(double(bar * 8 + step + 1) * samplesPerEighth);
        note.midiPitch = float(root + degrees[step]);
        note.detectedPitch = note.midiPitch;
        note.confidence = 1.0f;
        notes.append(note);
    }
}

KeyAnalyzer::BarGrid testGrid()
{
    KeyAnalyzer::BarGrid grid;
    grid.bpm = kBpm;
    grid.beatsPerBar = 4;
    grid.gridStartSample = 0;
    return grid;
}

} // namespace

// Смена тональности по тактам разбивает полосу референса на два региона
void MidiExportTest::testReferenceKeysSplitBarsOnModulation()
{
    QVector<PitchDetector::PitchNote> notes;
    appendScaleBar(notes, 0, 60);  // до мажор
    appendScaleBar(notes, 1, 60);
    appendScaleBar(notes, 2, 62);  // ре мажор — модуляция с третьего такта
    appendScaleBar(notes, 3, 62);

    const KeyAnalyzer::PerBarKeyResult result =
        MidiImporter::analyzeKeyPerBar(notes, testGrid(), kSampleRate);

    QCOMPARE(result.bars.size(), 4);
    QVERIFY2(result.hasModulation, "модуляция между тактами не замечена");
    QCOMPARE(result.regions.size(), 2);

    QCOMPARE(result.regions[0].startBar, 0);
    QCOMPARE(result.regions[0].endBar, 1);
    QCOMPARE(result.regions[1].startBar, 2);
    QCOMPARE(result.regions[1].endBar, 3);
    QVERIFY(result.regions[0].key.key != result.regions[1].key.key);
    QVERIFY2(result.regions[0].key.keyName.startsWith(QStringLiteral("C")),
             qPrintable(result.regions[0].key.keyName));

    // Границы регионов лежат на тактовой сетке: такт = 2 секунды при 120 BPM
    const qint64 samplesPerBar = 2 * kSampleRate;
    QCOMPARE(result.regions[0].startSample, qint64(0));
    QCOMPARE(result.regions[1].startSample, 2 * samplesPerBar);
}

// Пустой такт внутри не рвёт регион, а такты до первой ноты не разбираются
void MidiExportTest::testReferenceKeysHoldThroughSilentBar()
{
    QVector<PitchDetector::PitchNote> notes;
    appendScaleBar(notes, 2, 60);  // первые два такта — тишина
    appendScaleBar(notes, 4, 60);  // такт 3 пустой, тональность держится

    const KeyAnalyzer::PerBarKeyResult result =
        MidiImporter::analyzeKeyPerBar(notes, testGrid(), kSampleRate);

    QCOMPARE(result.regions.size(), 1);
    QCOMPARE(result.regions.first().startBar, 2);
    QCOMPARE(result.regions.first().endBar, 4);
    QCOMPARE(result.bars.size(), 3);
    QCOMPARE(result.bars[1].key.key, result.bars[0].key.key);
    QVERIFY(!result.hasModulation);
}

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
