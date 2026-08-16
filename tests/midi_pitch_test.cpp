#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QVector>
#include <QtCore/QtMath>

#include <cmath>
#include <optional>

#include "../include/audiofileservice.h"
#include "../include/pitchdetector.h"
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

QVector<float> downsample(const QVector<float>& in, int factor)
{
    if (factor <= 1 || in.isEmpty()) {
        return in;
    }
    QVector<float> out;
    out.reserve(in.size() / factor + 1);
    for (int i = 0; i + factor - 1 < in.size(); i += factor) {
        float sum = 0.0f;
        for (int k = 0; k < factor; ++k) {
            sum += in[i + k];
        }
        out.push_back(sum / float(factor));
    }
    return out;
}

std::optional<PitchDetector::PitchNote> bestPitchNear(
    const QVector<PitchDetector::PitchNote>& notes,
    float expectedMidi,
    float maxSemitoneError)
{
    std::optional<PitchDetector::PitchNote> best;
    float bestErr = 1.0e9f;
    for (const PitchDetector::PitchNote& note : notes) {
        const float err = std::abs(note.detectedPitch - expectedMidi);
        if (err < bestErr) {
            bestErr = err;
            best = note;
        }
    }
    if (!best || bestErr > maxSemitoneError) {
        return std::nullopt;
    }
    return best;
}

/** Нота, ближайшая по времени и высоте к MIDI ground truth. */
std::optional<PitchDetector::PitchNote> matchByTimeAndPitch(
    const QVector<PitchDetector::PitchNote>& notes,
    qint64 expectedStart,
    float expectedMidi,
    qint64 maxStartDeltaSamples,
    float maxSemitoneError)
{
    std::optional<PitchDetector::PitchNote> best;
    float bestScore = 1.0e9f;
    for (const PitchDetector::PitchNote& note : notes) {
        const float pitchErr = std::abs(note.detectedPitch - expectedMidi);
        if (pitchErr > maxSemitoneError) {
            continue;
        }
        const qint64 dt = qAbs(note.startSample - expectedStart);
        if (dt > maxStartDeltaSamples) {
            continue;
        }
        const float score = float(dt) / float(qMax<qint64>(1, maxStartDeltaSamples))
            + pitchErr;
        if (score < bestScore) {
            bestScore = score;
            best = note;
        }
    }
    return best;
}

} // namespace

/**
 * Интеграционный тест питчера (PitchDetector) на фикстуре tests/midi/test_1.
 * Ground truth — test_1.mid; аудио — test_1.wav (192 kHz → downsample).
 */
class MidiPitchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void parsesMidiGroundTruth();
    void detectsDurationLadderC4();
    void detectsLowAndMidRegisters();
    void doesNotInventSubharmonicNotes();

private:
    QString midPath;
    QString wavPath;
    MidiSmf::Song song;
    QVector<PitchDetector::PitchNote> detected;
    int analysisSampleRate = 0;
};

void MidiPitchTest::initTestCase()
{
    midPath = midiFixture(QStringLiteral("test_1.mid"));
    wavPath = midiFixture(QStringLiteral("test_1.wav"));
    if (midPath.isEmpty() || wavPath.isEmpty()) {
        QSKIP("tests/midi/test_1.mid + test_1.wav required");
    }

    song = MidiSmf::loadFile(midPath);
    QVERIFY2(!song.notes.isEmpty(), "failed to parse test_1.mid");
    QVERIFY(song.bpm > 130.0f && song.bpm < 150.0f);

    const auto decoded = AudioFileService::decode(wavPath);
    QVERIFY2(decoded.ok, qPrintable(decoded.error));
    QVERIFY(!decoded.channels.isEmpty());

    // 192 kHz → 48 kHz: достаточно для f0, заметно быстрее полного файла.
    const int factor = qMax(1, decoded.sampleRate / 48000);
    QVector<float> mono = AudioFileService::toMono(decoded.channels);
    mono = downsample(mono, factor);
    analysisSampleRate = decoded.sampleRate / factor;
    QVERIFY(analysisSampleRate > 0);

    // Первые ~8 с — монофонический duration ladder на C4…D#4 (см. README).
    const int maxSamples = int(8.0 * analysisSampleRate);
    if (mono.size() > maxSamples) {
        mono.resize(maxSamples);
    }

    // Материал участка — ladder C4 и педаль C3 (130 Гц). Нижнюю границу держим
    // на 60 Гц: она задаёт длину окна анализа, а ноты ladder'а длятся 107 мс,
    // поэтому запрашивать 16 Гц (окно ~200 мс) здесь физически бессмысленно.
    PitchDetector::Options opt;
    opt.minFrequencyHz = 60.0f;
    opt.maxFrequencyHz = 2000.0f;
    opt.minNoteDurationMs = 80;
    detected = PitchDetector::detectNotes(mono, analysisSampleRate, opt);
    QVERIFY2(!detected.isEmpty(), "PitchDetector returned no notes on test_1.wav head");
}

void MidiPitchTest::parsesMidiGroundTruth()
{
    QCOMPARE(song.ticksPerQuarter, 128);
    QVERIFY(song.notes.size() >= 200);
    QVERIFY(qFuzzyCompare(song.bpm, 140.0f) || (song.bpm > 139.9f && song.bpm < 140.1f));

    int minP = 127;
    int maxP = 0;
    for (const MidiSmf::Note& n : song.notes) {
        minP = qMin(minP, n.pitch);
        maxP = qMax(maxP, n.pitch);
    }
    QCOMPARE(minP, 12);  // C0
    QCOMPARE(maxP, 119); // B8
}

void MidiPitchTest::detectsDurationLadderC4()
{
    // Эталон первых нот ladder: C4 C#4 D4 D#4 с длительностями 1/16.
    const QVector<int> expectedPitches = {60, 61, 62, 63};
    int matched = 0;
    const qint64 maxDt = MidiSmf::tickToSample(song, song.ticksPerQuarter / 2, analysisSampleRate);

    for (int pitch : expectedPitches) {
        // Берём первую MIDI-ноту этой высоты в начале файла.
        const MidiSmf::Note* gt = nullptr;
        for (const MidiSmf::Note& n : song.notes) {
            if (n.pitch == pitch && n.startTick < song.ticksPerQuarter * 8) {
                gt = &n;
                break;
            }
        }
        QVERIFY2(gt != nullptr, qPrintable(QStringLiteral("MIDI missing pitch %1").arg(pitch)));

        const qint64 start = MidiSmf::tickToSample(song, gt->startTick, analysisSampleRate);
        const auto hit = matchByTimeAndPitch(
            detected, start, float(pitch), maxDt, 0.75f);
        if (hit) {
            ++matched;
        } else {
            qWarning("missing detected note near MIDI %d @ sample %lld", pitch, static_cast<long long>(start));
        }
    }

    QVERIFY2(matched >= 3,
             qPrintable(QStringLiteral("duration ladder C4..D#4: matched %1/4").arg(matched)));
}

void MidiPitchTest::detectsLowAndMidRegisters()
{
    // В первых 8 с есть педаль C3 (~5.1 s) — проверяем, что детектор видит регистр ниже C4.
    const auto c3 = bestPitchNear(detected, 48.0f, 1.0f);
    QVERIFY2(c3.has_value(), "expected C3 pedal in first 8s of test_1");

    // C4 ladder должен доминировать в начале.
    const auto c4 = bestPitchNear(detected, 60.0f, 0.75f);
    QVERIFY2(c4.has_value(), "expected C4 in duration ladder");
    QVERIFY(c4->confidence > 0.1f);
}

/**
 * Регрессия: разностная функция без кумулятивной нормировки одинаково хорошо
 * «объясняет» сигнал периодом T и любым кратным ему (2T, 3T…), из-за чего на
 * чистом ladder'е C4 появлялись ноты на октаву-две ниже (29, 32, 36, 41…).
 * Внутри первых 5 секунд звучат только C4…D#4, значит ничего ниже C3 быть не
 * должно.
 */
void MidiPitchTest::doesNotInventSubharmonicNotes()
{
    const qint64 ladderEnd = qint64(5.0 * analysisSampleRate);

    QVector<float> offenders;
    for (const PitchDetector::PitchNote& note : detected) {
        if (note.startSample >= ladderEnd) {
            continue;
        }
        // C3 (48) — с запасом ниже реального минимума участка (C4 = 60).
        if (note.detectedPitch < 48.0f) {
            offenders.append(note.detectedPitch);
        }
    }

    QStringList shown;
    for (float p : offenders) {
        shown << QString::number(p, 'f', 1);
    }
    QVERIFY2(offenders.isEmpty(),
             qPrintable(QStringLiteral("субгармонические ноты в ladder C4: %1")
                            .arg(shown.join(' '))));
}

QTEST_MAIN(MidiPitchTest)
#include "midi_pitch_test.moc"
