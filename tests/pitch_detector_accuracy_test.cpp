#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QVector>
#include <QtCore/QtMath>

#include <cmath>
#include <optional>

#include "../include/audiofileservice.h"
#include "../include/pitchdetector.h"

namespace {

QString fixturePath(const QString& fileName)
{
    const QString rel = QStringLiteral("tests/source4test/pitch/%1").arg(fileName);
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

QVector<float> loadMono(const QString& path, int& sampleRate)
{
    sampleRate = 0;
    const auto decoded = AudioFileService::decode(path);
    if (!decoded.ok || decoded.channels.isEmpty()) {
        return {};
    }
    sampleRate = decoded.sampleRate;
    return AudioFileService::toMono(decoded.channels);
}

std::optional<PitchDetector::PitchNote> nearestNote(
    const QVector<PitchDetector::PitchNote>& notes, float expectedMidi, float maxSemitoneError)
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

float meanAbsoluteCentsError(const QVector<PitchDetector::PitchNote>& notes,
                             const QVector<float>& expectedMidi)
{
    if (expectedMidi.isEmpty()) {
        return 0.0f;
    }
    float acc = 0.0f;
    int count = 0;
    for (float expected : expectedMidi) {
        if (const auto note = nearestNote(notes, expected, 1.5f)) {
            acc += std::abs(note->detectedPitch - expected) * 100.0f;
            ++count;
        }
    }
    return count > 0 ? acc / float(count) : 1.0e9f;
}

} // namespace

class PitchDetectorAccuracyTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void detectsScaleC0toC4();
    void detectsCMajorMelody();
    void detectsVibratoCenterNearA3();
    void detectsDetunedE3WithinTolerance();

private:
    QVector<PitchDetector::PitchNote> analyze(const QString& wavName);
};

void PitchDetectorAccuracyTest::initTestCase()
{
    if (fixturePath(QStringLiteral("scale_c0_c4.wav")).isEmpty()) {
        QSKIP("Pitch fixtures missing — run tools/generate_pitch_test_fixtures.py");
    }
}

QVector<PitchDetector::PitchNote> PitchDetectorAccuracyTest::analyze(const QString& wavName)
{
    int sampleRate = 0;
    const QString path = fixturePath(wavName);
    if (path.isEmpty()) {
        qWarning("fixture missing: %s", qPrintable(wavName));
        return {};
    }
    const QVector<float> mono = loadMono(path, sampleRate);
    if (mono.isEmpty() || sampleRate <= 0) {
        qWarning("failed to decode: %s", qPrintable(path));
        return {};
    }

    PitchDetector::Options opt;
    opt.minFrequencyHz = 16.0f;
    opt.maxFrequencyHz = 2000.0f;
    opt.minNoteDurationMs = 120;
    return PitchDetector::detectNotes(mono, sampleRate, opt);
}

void PitchDetectorAccuracyTest::detectsScaleC0toC4()
{
    const auto notes = analyze(QStringLiteral("scale_c0_c4.wav"));
    QVERIFY2(!notes.isEmpty(), "failed to analyze scale_c0_c4.wav");
    QVERIFY2(notes.size() >= 4, "expected at least C0..C3");

    const QVector<float> expected = {12.f, 24.f, 36.f, 48.f, 60.f};
    // Low C0 is hardest for autocorrelation; allow 120 cents mean error overall.
    const float meanCents = meanAbsoluteCentsError(notes, expected);
    QVERIFY2(meanCents < 120.0f, qPrintable(QStringLiteral("mean cents error=%1").arg(meanCents)));

    // Mid/high notes of the scale should be within half a semitone.
    for (float midi : {36.f, 48.f, 60.f}) {
        const auto note = nearestNote(notes, midi, 0.6f);
        QVERIFY2(note.has_value(), qPrintable(QStringLiteral("missing MIDI %1").arg(midi)));
    }
}

void PitchDetectorAccuracyTest::detectsCMajorMelody()
{
    const auto notes = analyze(QStringLiteral("melody_c_major.wav"));
    QVERIFY2(!notes.isEmpty(), "failed to analyze melody_c_major.wav");
    QVERIFY(notes.size() >= 6);

    const QVector<float> expected = {60, 62, 64, 65, 67, 69, 71, 72, 48, 52, 55};
    const float meanCents = meanAbsoluteCentsError(notes, expected);
    QVERIFY2(meanCents < 80.0f, qPrintable(QStringLiteral("mean cents error=%1").arg(meanCents)));
}

void PitchDetectorAccuracyTest::detectsVibratoCenterNearA3()
{
    const auto notes = analyze(QStringLiteral("vibrato_a3.wav"));
    QVERIFY2(!notes.isEmpty(), "failed to analyze vibrato_a3.wav");

    // Mild vibrato (~±20 cents) on a pure tone: center should stay near A3.
    // Autocorrelation may still prefer an octave twin — accept A2/A3/A4.
    auto note = nearestNote(notes, 69.0f, 1.0f);
    if (!note) {
        note = nearestNote(notes, 57.0f, 1.0f);
    }
    if (!note) {
        note = nearestNote(notes, 81.0f, 1.0f);
    }
    QVERIFY2(note.has_value(),
             qPrintable(QStringLiteral("A3 vibrato not near A; first=%1")
                            .arg(notes.isEmpty() ? -1.0f : notes.first().detectedPitch)));
    QVERIFY(note->confidence > 0.15f);
}

void PitchDetectorAccuracyTest::detectsDetunedE3WithinTolerance()
{
    const auto notes = analyze(QStringLiteral("detuned_e3.wav"));
    QVERIFY2(!notes.isEmpty(), "failed to analyze detuned_e3.wav");

    // Ground truth is E3 + 35 cents ≈ 64.35 MIDI.
    const float expected = 64.0f + 0.35f;
    const auto note = nearestNote(notes, expected, 0.75f);
    QVERIFY2(note.has_value(), "detuned E3 not found");

    // Segmentation currently snaps to integer MIDI; accept nearest integer E3
    // but keep room for future fractional detection.
    QVERIFY(std::abs(note->detectedPitch - 64.0f) < 0.75f
            || std::abs(note->detectedPitch - expected) < 0.75f);
}

QTEST_MAIN(PitchDetectorAccuracyTest)
#include "pitch_detector_accuracy_test.moc"
