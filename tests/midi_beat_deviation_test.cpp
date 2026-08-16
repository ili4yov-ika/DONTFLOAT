#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QVector>

#include <cmath>

#include "../include/bpmanalyzer.h"
#include "midi_smf.h"

namespace {

QString midiFixture(const QString& fileName)
{
    const QString rel = QStringLiteral("tests/midi/%1").arg(fileName);
    if (QFileInfo::exists(rel)) {
        return QFileInfo(rel).absoluteFilePath();
    }
    QDir dir(QDir::current());
    for (int i = 0; i < 4; ++i) {
        const QString path = dir.absoluteFilePath(rel);
        if (QFileInfo::exists(path)) {
            return path;
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}

/** Четвертные доли по MIDI-сетке test_1 (140 BPM, 128 TPQN). */
QVector<BPMAnalyzer::BeatInfo> quarterBeatsFromMidi(const MidiSmf::Song& song, int sampleRate)
{
    QVector<BPMAnalyzer::BeatInfo> beats;
    if (song.notes.isEmpty() || song.ticksPerQuarter <= 0) {
        return beats;
    }

    int lastTick = 0;
    for (const MidiSmf::Note& n : song.notes) {
        lastTick = qMax(lastTick, n.endTick);
    }

    const int step = song.ticksPerQuarter;
    for (int tick = 0; tick <= lastTick; tick += step) {
        BPMAnalyzer::BeatInfo b;
        b.position = MidiSmf::tickToSample(song, tick, sampleRate);
        b.expectedPosition = 0;
        b.confidence = 1.0f;
        b.deviation = 0.0f;
        b.energy = 1.0f;
        beats.push_back(b);
    }
    return beats;
}

} // namespace

/**
 * Тест механизма неровных долей (calculateDeviations / findUnalignedBeats)
 * на идеальной сетке test_1.mid (140 BPM) и на сетке с искусственными сдвигами.
 *
 * Позиции долей переводятся в сэмплы так, как у test_1.wav (192 kHz), чтобы
 * масштаб совпадал с реальной фикстурой.
 */
class MidiBeatDeviationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void idealGridHasNoUnalignedBeats();
    void injectedShiftsAreDetected();
    void missedAndExtraBeatsStayLocal();
    void noteOnsetsAlignToQuarterGrid();

private:
    QString midPath;
    MidiSmf::Song song;
    static constexpr int kWavSampleRate = 192000; // как у test_1.wav
};

void MidiBeatDeviationTest::initTestCase()
{
    midPath = midiFixture(QStringLiteral("test_1.mid"));
    if (midPath.isEmpty()) {
        QSKIP("tests/midi/test_1.mid required");
    }
    // WAV нужен как контракт фикстуры (частота / наличие файла).
    if (midiFixture(QStringLiteral("test_1.wav")).isEmpty()) {
        QSKIP("tests/midi/test_1.wav required");
    }

    song = MidiSmf::loadFile(midPath);
    QVERIFY2(!song.notes.isEmpty(), "failed to parse test_1.mid");
    QVERIFY(song.bpm > 139.0f && song.bpm < 141.0f);
    QCOMPARE(song.ticksPerQuarter, 128);
}

void MidiBeatDeviationTest::idealGridHasNoUnalignedBeats()
{
    auto beats = quarterBeatsFromMidi(song, kWavSampleRate);
    QVERIFY2(beats.size() >= 16, "expected at least several bars of quarters");

    BPMAnalyzer::calculateDeviations(beats, song.bpm, kWavSampleRate);

    for (int i = 0; i < beats.size(); ++i) {
        QVERIFY2(qAbs(beats[i].deviation) < 0.001f,
                 qPrintable(QStringLiteral("beat %1 deviation=%2")
                                .arg(i)
                                .arg(beats[i].deviation)));
    }

    const QVector<int> unaligned = BPMAnalyzer::findUnalignedBeats(beats, 0.02f);
    QCOMPARE(unaligned.size(), 0);
}

void MidiBeatDeviationTest::injectedShiftsAreDetected()
{
    auto beats = quarterBeatsFromMidi(song, kWavSampleRate);
    QVERIFY(beats.size() > 20);

    const float interval = (60.0f * kWavSampleRate) / song.bpm;

    // Сдвигаем две доли: +8% и −6% интервала.
    const int idxLate = 8;
    const int idxEarly = 15;
    beats[idxLate].position += qint64(interval * 0.08f);
    beats[idxEarly].position -= qint64(interval * 0.06f);

    BPMAnalyzer::calculateDeviations(beats, song.bpm, kWavSampleRate);

    QVERIFY(beats[idxLate].deviation > 0.05f);
    QVERIFY(beats[idxEarly].deviation < -0.04f);

    const QVector<int> at2pct = BPMAnalyzer::findUnalignedBeats(beats, 0.02f);
    QVERIFY2(at2pct.contains(idxLate), "late beat not flagged @ 2%");
    QVERIFY2(at2pct.contains(idxEarly), "early beat not flagged @ 2%");

    const QVector<int> at10pct = BPMAnalyzer::findUnalignedBeats(beats, 0.10f);
    QVERIFY2(!at10pct.contains(idxLate), "8% late should be under 10% threshold");
    QVERIFY2(!at10pct.contains(idxEarly), "6% early should be under 10% threshold");
}

void MidiBeatDeviationTest::missedAndExtraBeatsStayLocal()
{
    // Пропуск доли и лишнее срабатывание — обычная ситуация для детектора onset'ов.
    // Ни то, ни другое не должно расходиться по всему треку.
    auto beats = quarterBeatsFromMidi(song, kWavSampleRate);
    QVERIFY(beats.size() > 20);

    const float interval = (60.0f * kWavSampleRate) / song.bpm;
    const int lastIndex = beats.size() - 1;

    BPMAnalyzer::BeatInfo extra = beats[6];
    extra.position += qint64(interval * 0.5f);
    beats.insert(7, extra);   // ложная доля между 6 и 7
    beats.remove(12);         // пропущенная доля

    const BPMAnalyzer::DeviationStats stats =
        BPMAnalyzer::calculateDeviations(beats, song.bpm, kWavSampleRate,
                                         BPMAnalyzer::DeviationOptions());

    QCOMPARE(stats.gapCount, 1);
    QCOMPARE(stats.duplicateCount, 1);

    const QVector<int> unaligned = BPMAnalyzer::findUnalignedBeats(beats, 0.02f);
    QCOMPARE(unaligned.size(), qsizetype(1));
    QCOMPARE(unaligned.first(), 7);

    // Хвост трека (после обеих аномалий) остаётся на сетке.
    QVERIFY2(qAbs(beats[lastIndex].deviation) < 0.001f, "аномалии не должны сдвигать хвост");
}

void MidiBeatDeviationTest::noteOnsetsAlignToQuarterGrid()
{
    // Все старты нот test_1 лежат на целочисленной сетке 32-х тиков (1/16).
    const int sixteenth = song.ticksPerQuarter / 4; // 32
    QVERIFY(sixteenth > 0);

    int offGrid = 0;
    for (const MidiSmf::Note& n : song.notes) {
        if (n.startTick % sixteenth != 0) {
            ++offGrid;
        }
    }
    QCOMPARE(offGrid, 0);

    // Старты на четвертях: позиция в сэмплах должна совпадать с идеальной
    // сеткой 140 BPM @ 192 kHz (даже если между четвертями есть паузы без onset).
    const float interval = (60.0f * kWavSampleRate) / song.bpm;
    int checked = 0;
    for (const MidiSmf::Note& n : song.notes) {
        if (n.startTick % song.ticksPerQuarter != 0) {
            continue;
        }
        const int beatIndex = n.startTick / song.ticksPerQuarter;
        const qint64 actual = MidiSmf::tickToSample(song, n.startTick, kWavSampleRate);
        const qint64 expected = qint64(std::llround(double(beatIndex) * double(interval)));
        const float deviation = float(actual - expected) / interval;
        QVERIFY2(qAbs(deviation) < 0.001f,
                 qPrintable(QStringLiteral("quarter onset tick=%1 deviation=%2")
                                .arg(n.startTick)
                                .arg(deviation)));
        ++checked;
    }
    QVERIFY2(checked >= 16, "expected many quarter-aligned note onsets");
}

QTEST_MAIN(MidiBeatDeviationTest)
#include "midi_beat_deviation_test.moc"
