// Правка высоты нот и её поведение при растяжении дорожки — на реальном
// материале (pitch-test_C140BPM.mp3: одна устойчивая нота E3 ~165 Гц, 8.6 с).
//
// Проходится весь путь редактора:
//   1. детектор находит ноту, пианоролльный разрез делит её на восемь;
//   2. двум случайным нотам высота поднимается, двум другим опускается,
//      PitchCorrection::apply — и результат меряется по звуку, а не по полям;
//   3. метки растягивают один участок и сжимают другой, ноты переносятся
//      теми же метками — проверяется и новая длительность, и что тон при
//      растяжении не поехал;
//   4. внутри растянутого и сжатого участков высота правится ещё раз —
//      коррекция обязана работать по новым координатам.
//
// Материал реальный, поэтому меряется не точное значение, а отношение частот:
// MP3 и Rubber Band дают свою погрешность (тот же допуск 6%, что и в
// pitch_compensation_file_test).

#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRandomGenerator>
#include <QtCore/QVector>

#include "../include/audiofileservice.h"
#include "../include/markerengine.h"
#include "../include/pitchcorrection.h"
#include "../include/pitchdetector.h"
#include "../include/pitchnotesplitcommand.h"
#include "../include/timestretchprocessor.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kPitchTestFile = "pitch-test_C140BPM.mp3";
/**
 * Допуск на «тон не поехал» после растяжения: MP3 + Rubber Band на устойчивой
 * ноте, та же величина, что в pitch_compensation_file_test.
 */
constexpr float kPitchTolerance = 0.06f;
/**
 * Допуск на проверку самого сдвига высоты — строже, и это принципиально:
 * полутон это всего 5.95%, поэтому с допуском 6% проверка «нота уехала на два
 * полутона» одинаково принимала бы и один, и три. Проверено мутацией.
 */
constexpr float kShiftTolerance = 0.025f;
/** Насколько двигаем высоту: два полутона слышно и уверенно меряется. */
constexpr float kSemitoneShift = 2.0f;
constexpr int kNoteCount = 8;
/** Один и тот же посев: выбор нот произвольный, но прогон воспроизводимый. */
constexpr quint32 kSeed = 20260903u;

QString resolveTestDataPath(const QString& filename)
{
    const QString rel = QStringLiteral("tests/source4test/%1").arg(filename);
    if (QFileInfo::exists(rel)) {
        return QFileInfo(rel).absoluteFilePath();
    }
    QDir dir(QDir::current());
    QString path = dir.absoluteFilePath(rel);
    if (QFileInfo::exists(path)) {
        return path;
    }
    dir.cdUp();
    path = dir.absoluteFilePath(rel);
    if (QFileInfo::exists(path)) {
        return path;
    }
    return {};
}

/** Основная частота участка автокорреляцией — как в pitch_compensation_file_test. */
float estimateFundamentalHz(const QVector<float>& samples, int sampleRate,
                            qint64 start, qint64 length)
{
    if (sampleRate <= 0 || length < sampleRate / 25 || start < 0
        || start + length > samples.size()) {
        return 0.0f;
    }

    const int minLag = qMax(2, sampleRate / 2000);
    const int maxLag = int(qMin<qint64>(length / 2, sampleRate / 50));

    float bestNorm = -1.0f;
    int bestLag = 0;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        double corr = 0.0;
        double energy = 0.0;
        const qint64 n = length - lag;
        for (qint64 i = 0; i < n; ++i) {
            const float a = samples[start + i];
            const float b = samples[start + i + lag];
            corr += double(a) * b;
            energy += double(a) * a;
        }
        if (energy <= 1e-12) {
            continue;
        }
        const float norm = float(corr / energy);
        if (norm > bestNorm) {
            bestNorm = norm;
            bestLag = lag;
        }
    }

    return bestLag > 0 ? float(sampleRate) / float(bestLag) : 0.0f;
}

/** Частота в середине ноты: края смазаны кроссфейдом сегментов, их не берём. */
float pitchOfNote(const QVector<float>& mono, int sampleRate,
                  const PitchDetector::PitchNote& note)
{
    const qint64 length = note.endSample - note.startSample;
    if (length <= 0) {
        return 0.0f;
    }
    const qint64 from = note.startSample + length / 4;
    const qint64 span = length / 2;
    if (from + span > mono.size()) {
        return 0.0f;
    }
    return estimateFundamentalHz(mono, sampleRate, from, span);
}

MarkerData makeMarker(qint64 original, qint64 target, int sampleRate,
                      bool fixed, bool endMarker)
{
    MarkerData m(original, fixed, endMarker, sampleRate);
    m.position = target;
    m.originalPosition = original;
    m.updateTimeFromSamples(sampleRate);
    return m;
}

/**
 * Сэмпл исходного таймлайна → сэмпл растянутого, кусочно-линейно по меткам.
 * Тем же способом ведёт ноты за метками главное окно (warpNotesThroughMarkers).
 */
qint64 mapThroughMarkers(qint64 sample, const QVector<MarkerData>& sorted)
{
    if (sorted.isEmpty()) {
        return sample;
    }
    if (sample <= sorted.first().originalPosition) {
        return sorted.first().position + (sample - sorted.first().originalPosition);
    }
    for (int i = 0; i + 1 < sorted.size(); ++i) {
        const MarkerData& a = sorted[i];
        const MarkerData& b = sorted[i + 1];
        if (sample >= a.originalPosition && sample <= b.originalPosition) {
            const qint64 origSpan = b.originalPosition - a.originalPosition;
            if (origSpan <= 0) {
                return a.position;
            }
            const double t = double(sample - a.originalPosition) / double(origSpan);
            const qint64 dispSpan = b.position - a.position;
            return a.position + qint64(std::llround(t * double(dispSpan)));
        }
    }
    const MarkerData& last = sorted.last();
    return last.position + (sample - last.originalPosition);
}

QVector<PitchDetector::PitchNote> warpNotes(const QVector<PitchDetector::PitchNote>& notes,
                                            const QVector<MarkerData>& markers)
{
    QVector<MarkerData> sorted = markers;
    std::sort(sorted.begin(), sorted.end(), [](const MarkerData& a, const MarkerData& b) {
        return a.originalPosition < b.originalPosition;
    });

    QVector<PitchDetector::PitchNote> out = notes;
    for (PitchDetector::PitchNote& note : out) {
        const qint64 start = mapThroughMarkers(note.startSample, sorted);
        const qint64 end = mapThroughMarkers(note.endSample, sorted);
        note.startSample = qMin(start, end);
        note.endSample = qMax(start, end);
        // Звук ноты после переноса лежит там же, где она нарисована
        note.sourceStartSample = -1;
        note.sourceEndSample = -1;
    }
    return out;
}

void skipIfCi()
{
    if (qEnvironmentVariableIsSet("CI") || qEnvironmentVariableIsSet("GITHUB_ACTIONS")) {
        QSKIP("Интеграционный тест на MP3 пропущен в CI (декодер и Rubber Band "
              "зависят от окружения, долгий прогон).");
    }
}

} // namespace

class PitchNoteStretchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testDetectedNoteSplitsIntoEight();
    void testTwoNotesUpTwoNotesDown();
    void testStretchMovesNotesAndKeepsPitch();
    void testPitchEditsInsideStretchedAndCompressedRegions();

private:
    QString m_filePath;
    QVector<QVector<float>> m_channels;
    QVector<float> m_mono;
    int m_sampleRate = 0;
    /** Восемь нот из разреза исходной — общая заготовка для всех проверок. */
    QVector<PitchDetector::PitchNote> m_notes;
    float m_referenceHz = 0.0f;

    void requireMaterial();
    /** Метки: первая треть растянута ×1.4, вторая сжата ×0.7, хвост как был. */
    QVector<MarkerData> makeStretchMarkers(qint64* outFirstBoundary,
                                           qint64* outSecondBoundary) const;
    /** Ожидаемое отношение частот при сдвиге на полутона. */
    static float ratioForSemitones(float semitones)
    {
        return std::pow(2.0f, semitones / 12.0f);
    }
};

void PitchNoteStretchTest::initTestCase()
{
    skipIfCi();

    m_filePath = resolveTestDataPath(QString::fromUtf8(kPitchTestFile));
    if (m_filePath.isEmpty()) {
        return;
    }

    const AudioFileService::DecodeResult res = AudioFileService::decode(m_filePath);
    QVERIFY2(res.ok, qPrintable(res.error));
    QVERIFY(!res.channels.isEmpty());

    m_channels = res.channels;
    m_sampleRate = res.sampleRate;
    m_mono = AudioFileService::toMono(m_channels);
    QVERIFY(m_sampleRate > 0);
    QVERIFY(m_mono.size() > m_sampleRate);

    // Детектор на этом материале даёт одну длинную ноту — её и режем
    QVector<PitchDetector::PitchNote> detected =
        PitchDetector::detectNotes(m_mono, m_sampleRate);
    QVERIFY2(!detected.isEmpty(), "детектор не нашёл ни одной ноты");

    // Оставляем самую длинную: разрез должен идти по устойчивому тону
    std::sort(detected.begin(), detected.end(),
              [](const PitchDetector::PitchNote& a, const PitchDetector::PitchNote& b) {
                  return (a.endSample - a.startSample) > (b.endSample - b.startSample);
              });
    m_notes = { detected.first() };

    // Пианоролльный разрез — тот же, которым делит ноты пользователь
    const qint64 start = m_notes.first().startSample;
    const qint64 length = m_notes.first().endSample - start;
    for (int i = 1; i < kNoteCount; ++i) {
        const qint64 cut = start + (length * i) / kNoteCount;
        PitchNoteSplitCommand split(&m_notes, m_notes.size() - 1, cut, QStringLiteral("split"));
        split.redo();
    }

    m_referenceHz = pitchOfNote(m_mono, m_sampleRate, m_notes.first());
    QVERIFY2(m_referenceHz > 50.0f && m_referenceHz < 4000.0f,
             "не удалось измерить высоту исходной ноты");
}

void PitchNoteStretchTest::requireMaterial()
{
    if (m_filePath.isEmpty() || m_mono.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("Файл %1 недоступен").arg(kPitchTestFile)));
    }
}

QVector<MarkerData> PitchNoteStretchTest::makeStretchMarkers(qint64* outFirstBoundary,
                                                             qint64* outSecondBoundary) const
{
    // Границы участков ставим ровно по границам нот, чтобы нота целиком
    // попадала в растянутый либо в сжатый участок, а не в оба сразу
    const qint64 first = m_notes[kNoteCount / 4 * 1].endSample;   // конец ноты 1
    const qint64 second = m_notes[kNoteCount / 2 + 1].endSample;  // конец ноты 5
    const qint64 total = m_mono.size();

    const qint64 firstTarget = qint64(first * 1.4);
    const qint64 secondTarget = firstTarget + qint64((second - first) * 0.7);
    const qint64 endTarget = secondTarget + (total - second);

    if (outFirstBoundary) {
        *outFirstBoundary = first;
    }
    if (outSecondBoundary) {
        *outSecondBoundary = second;
    }

    return {
        makeMarker(0, 0, m_sampleRate, true, false),
        makeMarker(first, firstTarget, m_sampleRate, false, false),
        makeMarker(second, secondTarget, m_sampleRate, false, false),
        makeMarker(total, endTarget, m_sampleRate, false, true),
    };
}

// Разрез даёт ровно восемь нот, и все они на исходной высоте
void PitchNoteStretchTest::testDetectedNoteSplitsIntoEight()
{
    requireMaterial();

    QCOMPARE(m_notes.size(), qsizetype(kNoteCount));

    for (int i = 0; i < m_notes.size(); ++i) {
        const PitchDetector::PitchNote& note = m_notes[i];
        QVERIFY2(note.endSample > note.startSample, "нота нулевой длины");
        QVERIFY2(!note.isMovedInTime(), "разрез не должен считаться переносом ноты");
        if (i > 0) {
            QCOMPARE(note.startSample, m_notes[i - 1].endSample);
        }

        const float hz = pitchOfNote(m_mono, m_sampleRate, note);
        QVERIFY2(qAbs(hz - m_referenceHz) / m_referenceHz < kPitchTolerance,
                 qPrintable(QStringLiteral("нота %1: высота %2 Гц вместо %3 Гц")
                                .arg(i).arg(double(hz)).arg(double(m_referenceHz))));
    }
}

// Двум случайным нотам поднимаем высоту, двум другим опускаем — и слышим это
void PitchNoteStretchTest::testTwoNotesUpTwoNotesDown()
{
    requireMaterial();

    QRandomGenerator rng(kSeed);
    QVector<int> order;
    for (int i = 0; i < kNoteCount; ++i) {
        order.append(i);
    }
    std::shuffle(order.begin(), order.end(), rng);

    const QVector<int> up = { order[0], order[1] };
    const QVector<int> down = { order[2], order[3] };
    const QVector<int> untouched = { order[4], order[5] };

    QVector<PitchDetector::PitchNote> notes = m_notes;
    for (int index : up) {
        notes[index].midiPitch = notes[index].detectedPitch + kSemitoneShift;
    }
    for (int index : down) {
        notes[index].midiPitch = notes[index].detectedPitch - kSemitoneShift;
    }

    QVERIFY2(PitchCorrection::hasPendingEdits(notes), "правки высоты не увидены");

    const QVector<QVector<float>> corrected =
        PitchCorrection::apply(m_channels, notes, m_sampleRate);
    QVERIFY(!corrected.isEmpty() && !corrected[0].isEmpty());
    const QVector<float> mono = AudioFileService::toMono(corrected);

    const auto checkShift = [&](int index, float semitones) {
        const float hz = pitchOfNote(mono, m_sampleRate, notes[index]);
        QVERIFY2(hz > 0.0f, qPrintable(QStringLiteral("нота %1: тон не измеряется").arg(index)));
        const float expected = m_referenceHz * ratioForSemitones(semitones);
        const float error = qAbs(hz - expected) / expected;
        QVERIFY2(error < kShiftTolerance,
                 qPrintable(QStringLiteral("нота %1 на %2 полутона: %3 Гц вместо %4 Гц (%5%)")
                                .arg(index).arg(double(semitones))
                                .arg(double(hz)).arg(double(expected))
                                .arg(double(error * 100.0f), 0, 'f', 1)));
    };

    for (int index : up) {
        checkShift(index, kSemitoneShift);
    }
    for (int index : down) {
        checkShift(index, -kSemitoneShift);
    }
    for (int index : untouched) {
        checkShift(index, 0.0f);
    }
}

// Метки тянут участки, ноты едут за ними, а высота при этом не должна поехать
void PitchNoteStretchTest::testStretchMovesNotesAndKeepsPitch()
{
    requireMaterial();

    qint64 firstBoundary = 0;
    qint64 secondBoundary = 0;
    const QVector<MarkerData> markers = makeStretchMarkers(&firstBoundary, &secondBoundary);

    QString error;
    QVERIFY2(TimeStretchProcessor::validateMarkers(markers, m_mono.size(), &error),
             qPrintable(error));

    const TimeStretchProcessor::StretchResult stretched =
        TimeStretchProcessor::applyMarkerStretch(m_channels, markers, m_sampleRate, true);
    QVERIFY(!stretched.audioData.isEmpty() && !stretched.audioData[0].isEmpty());
    const QVector<float> mono = AudioFileService::toMono(stretched.audioData);

    const QVector<PitchDetector::PitchNote> moved = warpNotes(m_notes, markers);
    QCOMPARE(moved.size(), m_notes.size());

    int checkedStretched = 0;
    int checkedCompressed = 0;

    for (int i = 0; i < moved.size(); ++i) {
        const qint64 before = m_notes[i].endSample - m_notes[i].startSample;
        const qint64 after = moved[i].endSample - moved[i].startSample;
        QVERIFY2(before > 0 && after > 0, "нота потеряла длину при переносе");
        const double factor = double(after) / double(before);

        const bool inStretched = m_notes[i].endSample <= firstBoundary;
        const bool inCompressed = m_notes[i].startSample >= firstBoundary
            && m_notes[i].endSample <= secondBoundary;

        if (inStretched) {
            QVERIFY2(qAbs(factor - 1.4) < 0.05,
                     qPrintable(QStringLiteral("нота %1 в растянутом участке: ×%2 вместо ×1.4")
                                    .arg(i).arg(factor)));
            ++checkedStretched;
        } else if (inCompressed) {
            QVERIFY2(qAbs(factor - 0.7) < 0.05,
                     qPrintable(QStringLiteral("нота %1 в сжатом участке: ×%2 вместо ×0.7")
                                    .arg(i).arg(factor)));
            ++checkedCompressed;
        }

        if (!inStretched && !inCompressed) {
            continue;
        }

        // Тонкомпенсация: участок стал длиннее или короче, тон остался прежним
        const float hz = pitchOfNote(mono, m_sampleRate, moved[i]);
        QVERIFY2(hz > 0.0f, qPrintable(QStringLiteral("нота %1: тон не измеряется").arg(i)));
        const float shift = qAbs(hz - m_referenceHz) / m_referenceHz;
        QVERIFY2(shift < kPitchTolerance,
                 qPrintable(QStringLiteral("нота %1 после растяжения: %2 Гц вместо %3 Гц")
                                .arg(i).arg(double(hz)).arg(double(m_referenceHz))));
    }

    QVERIFY2(checkedStretched > 0, "ни одна нота не попала в растянутый участок");
    QVERIFY2(checkedCompressed > 0, "ни одна нота не попала в сжатый участок");
}

// Высота правится уже внутри растянутого и сжатого участков
void PitchNoteStretchTest::testPitchEditsInsideStretchedAndCompressedRegions()
{
    requireMaterial();

    qint64 firstBoundary = 0;
    qint64 secondBoundary = 0;
    const QVector<MarkerData> markers = makeStretchMarkers(&firstBoundary, &secondBoundary);

    const TimeStretchProcessor::StretchResult stretched =
        TimeStretchProcessor::applyMarkerStretch(m_channels, markers, m_sampleRate, true);
    QVERIFY(!stretched.audioData.isEmpty() && !stretched.audioData[0].isEmpty());

    QVector<PitchDetector::PitchNote> notes = warpNotes(m_notes, markers);

    // Ноты, целиком лежащие в растянутом и в сжатом участке
    QVector<int> inStretched;
    QVector<int> inCompressed;
    for (int i = 0; i < m_notes.size(); ++i) {
        if (m_notes[i].endSample <= firstBoundary) {
            inStretched.append(i);
        } else if (m_notes[i].startSample >= firstBoundary
                   && m_notes[i].endSample <= secondBoundary) {
            inCompressed.append(i);
        }
    }
    QVERIFY(!inStretched.isEmpty() && !inCompressed.isEmpty());

    QRandomGenerator rng(kSeed + 1);
    const int upIndex = inStretched[int(rng.bounded(quint32(inStretched.size())))];
    const int downIndex = inCompressed[int(rng.bounded(quint32(inCompressed.size())))];

    notes[upIndex].midiPitch = notes[upIndex].detectedPitch + kSemitoneShift;
    notes[downIndex].midiPitch = notes[downIndex].detectedPitch - kSemitoneShift;
    QVERIFY(PitchCorrection::hasPendingEdits(notes));

    const QVector<QVector<float>> corrected =
        PitchCorrection::apply(stretched.audioData, notes, m_sampleRate);
    QVERIFY(!corrected.isEmpty() && !corrected[0].isEmpty());
    QCOMPARE(corrected[0].size(), stretched.audioData[0].size());
    const QVector<float> mono = AudioFileService::toMono(corrected);

    const auto checkShift = [&](int index, float semitones, const char* where) {
        const float hz = pitchOfNote(mono, m_sampleRate, notes[index]);
        QVERIFY2(hz > 0.0f, qPrintable(QStringLiteral("нота %1 (%2): тон не измеряется")
                                           .arg(index).arg(QString::fromUtf8(where))));
        const float expected = m_referenceHz * ratioForSemitones(semitones);
        const float error = qAbs(hz - expected) / expected;
        QVERIFY2(error < kShiftTolerance,
                 qPrintable(QStringLiteral("нота %1 (%2) на %3 полутона: %4 Гц вместо %5 Гц (%6%)")
                                .arg(index).arg(QString::fromUtf8(where))
                                .arg(double(semitones)).arg(double(hz)).arg(double(expected))
                                .arg(double(error * 100.0f), 0, 'f', 1)));
    };

    checkShift(upIndex, kSemitoneShift, "растянутый участок");
    checkShift(downIndex, -kSemitoneShift, "сжатый участок");

    // Соседние ноты, которых не трогали, остались на своей высоте
    for (int index : inStretched) {
        if (index != upIndex) {
            checkShift(index, 0.0f, "растянутый участок, без правки");
        }
    }
    for (int index : inCompressed) {
        if (index != downIndex) {
            checkShift(index, 0.0f, "сжатый участок, без правки");
        }
    }
}

QTEST_MAIN(PitchNoteStretchTest)
#include "pitch_note_stretch_test.moc"
