// Выравнивание долей по сетке: доли действительно едут на сетку, а звук
// остаётся звуком.
//
// Раньше «выровнять доли» в редакции Scratch звало BPMAnalyzer::alignToBeatGrid,
// которая складывала все сэмплы доли в один отсчёт — на выходе получался щелчок
// на каждую долю вместо музыки. А метки коррекции в главном окне строились
// «наоборот»: источником сегмента бралась линия сетки, а целью — фактическая
// доля, поэтому растяжение уводило долю ещё дальше от сетки.
//
// Тест держит обе вещи: и стороны меток, и то, что после выравнивания доли
// стоят на сетке, а энергия сигнала никуда не делась.

#include <QtTest/QTest>

#include "../include/markerengine.h"
#include "../include/timestretchprocessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr int kSampleRate = 44100;
constexpr float kBpm = 120.0f;                  // доля = 22050 сэмплов
constexpr int kBeatInterval = 22050;
constexpr int kBurstSamples = 2205;             // 50 мс звука на долю

/** Дорожка: короткие всплески синуса на заданных позициях, между ними тишина. */
QVector<QVector<float>> makeBeatTrack(const QVector<qint64>& beatPositions, int totalSamples)
{
    QVector<float> mono(totalSamples, 0.0f);
    for (qint64 pos : beatPositions) {
        for (int i = 0; i < kBurstSamples; ++i) {
            const qint64 idx = pos + i;
            if (idx < 0 || idx >= totalSamples) {
                continue;
            }
            // Затухающий всплеск: у него чёткое начало, по нему и ищем долю
            const double t = double(i) / double(kSampleRate);
            const double env = std::exp(-30.0 * t);
            mono[int(idx)] = float(0.8 * env * std::sin(2.0 * M_PI * 440.0 * t));
        }
    }
    return { mono };
}

/** Позиции начала всплесков: подъём огибающей выше порога. */
QVector<qint64> findBurstStarts(const QVector<float>& samples, int minDistance)
{
    QVector<qint64> onsets;
    float peak = 0.0f;
    for (float v : samples) {
        peak = std::max(peak, std::fabs(v));
    }
    if (peak <= 0.0f) {
        return onsets;
    }
    const float threshold = peak * 0.35f;
    qint64 last = -minDistance;
    for (int i = 0; i < samples.size(); ++i) {
        if (std::fabs(samples[i]) < threshold) {
            continue;
        }
        if (i - last < minDistance) {
            continue;
        }
        onsets.append(i);
        last = i;
    }
    return onsets;
}

double rms(const QVector<float>& samples)
{
    if (samples.isEmpty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float v : samples) {
        sum += double(v) * double(v);
    }
    return std::sqrt(sum / double(samples.size()));
}

/** Насколько доли не попадают на сетку (в сэмплах, суммарно). */
qint64 totalGridError(const QVector<qint64>& beats, qint64 gridStart, double interval)
{
    qint64 error = 0;
    for (qint64 beat : beats) {
        const double index = std::round((double(beat) - double(gridStart)) / interval);
        const qint64 line = gridStart + qint64(std::llround(index * interval));
        error += std::llabs(beat - line);
    }
    return error;
}

} // namespace

class BeatAlignTest : public QObject
{
    Q_OBJECT

private slots:
    void testMarkersMapBeatsOntoGrid();
    void testMarkersKeepTrackBoundaries();
    void testDuplicateBeatsOnOneGridLineCollapse();
    void testAlignmentMovesBeatsCloserToGrid();
    void testAlignmentKeepsAudioAlive();
};

// Метка ведёт «из доли на сетку», а не наоборот
void BeatAlignTest::testMarkersMapBeatsOntoGrid()
{
    const QVector<qint64> beats { 0, 22050 + 900, 44100 - 700, 66150 + 400 };
    const int total = 5 * kBeatInterval;

    const QVector<MarkerData> markers = TimeStretchProcessor::buildBeatAlignmentMarkers(
        beats, double(kBeatInterval), 0, total, kSampleRate);
    QVERIFY2(markers.size() >= 4, "меток должно хватить на доли и края");

    for (const MarkerData& m : markers) {
        if (m.isFixed || m.isEndMarker) {
            continue;  // края закреплены «сами в себя»
        }
        // Цель метки — линия сетки
        QCOMPARE(m.position % kBeatInterval, qint64(0));
        // Источник — фактическая доля, и она рядом со своей линией
        QVERIFY2(std::llabs(m.originalPosition - m.position) < kBeatInterval / 2,
                 "источник метки должен быть той самой долей, а не соседней");
        QVERIFY2(beats.contains(m.originalPosition),
                 "источником обязана быть фактическая доля из разметки");
    }

    // Метки идут по возрастанию и по источнику, и по цели: иначе сегмент
    // вывернется наизнанку при растяжении
    for (int i = 1; i < markers.size(); ++i) {
        QVERIFY(markers[i].originalPosition > markers[i - 1].originalPosition);
        QVERIFY(markers[i].position > markers[i - 1].position);
    }
}

// Края закреплены — длина дорожки не меняется
void BeatAlignTest::testMarkersKeepTrackBoundaries()
{
    const QVector<qint64> beats { 22050 + 900, 44100 - 700 };
    const int total = 4 * kBeatInterval;

    const QVector<MarkerData> markers = TimeStretchProcessor::buildBeatAlignmentMarkers(
        beats, double(kBeatInterval), 0, total, kSampleRate);
    QVERIFY(markers.size() >= 3);

    QCOMPARE(markers.first().originalPosition, qint64(0));
    QCOMPARE(markers.first().position, qint64(0));
    QVERIFY(markers.first().isFixed);

    QCOMPARE(markers.last().originalPosition, qint64(total - 1));
    QCOMPARE(markers.last().position, qint64(total - 1));
    QVERIFY(markers.last().isEndMarker);
}

// Два срабатывания детектора на одной линии сетки дают одну метку
void BeatAlignTest::testDuplicateBeatsOnOneGridLineCollapse()
{
    // 22050 и 22050+300 сядут на одну линию 22050; ближе к ней первая
    const QVector<qint64> beats { 22050 + 300, 22050 + 30, 44100 - 500 };
    const int total = 4 * kBeatInterval;

    const QVector<MarkerData> markers = TimeStretchProcessor::buildBeatAlignmentMarkers(
        beats, double(kBeatInterval), 0, total, kSampleRate);

    int onFirstLine = 0;
    for (const MarkerData& m : markers) {
        if (m.position == kBeatInterval) {
            ++onFirstLine;
            QCOMPARE(m.originalPosition, qint64(22050 + 30));  // выбрана ближняя
        }
    }
    QCOMPARE(onFirstLine, 1);
}

// Главное: после выравнивания доли ближе к сетке, чем были
void BeatAlignTest::testAlignmentMovesBeatsCloserToGrid()
{
    const QVector<qint64> beats { 0, 22050 + 1200, 44100 - 900, 66150 + 600 };
    const int total = 5 * kBeatInterval;
    const QVector<QVector<float>> audio = makeBeatTrack(beats, total);

    const qint64 errorBefore = totalGridError(beats, 0, double(kBeatInterval));
    QVERIFY2(errorBefore > 2000, "исходные доли должны быть заметно кривыми");

    const TimeStretchProcessor::StretchResult result = TimeStretchProcessor::alignBeatsToGrid(
        audio, beats, kBpm, kSampleRate, 0);
    QVERIFY(!result.audioData.isEmpty());
    QVERIFY(!result.audioData[0].isEmpty());

    const QVector<qint64> after = findBurstStarts(result.audioData[0], kBeatInterval / 2);
    QVERIFY2(after.size() >= 3, "всплески должны остаться различимыми");

    const qint64 errorAfter = totalGridError(after, 0, double(kBeatInterval));
    QVERIFY2(errorAfter < errorBefore / 3,
             qPrintable(QStringLiteral("выравнивание не приблизило доли к сетке: было %1, стало %2")
                            .arg(errorBefore)
                            .arg(errorAfter)));

    // Ни одна доля не должна остаться дальше 10 мс от своей линии: остаток —
    // это размазанная растяжением атака, а не промах выравнивания
    const qint64 tolerance = kSampleRate / 100;
    for (qint64 onset : after) {
        const qint64 line =
            qint64(std::llround(double(onset) / double(kBeatInterval))) * kBeatInterval;
        QVERIFY2(std::llabs(onset - line) <= tolerance,
                 qPrintable(QStringLiteral("доля на %1 далеко от линии %2").arg(onset).arg(line)));
    }

    // Ошибка не должна расти к концу дорожки: так проявлялся снос кроссфейдами,
    // когда каждый стык съедал свои ~10 мс
    const qint64 firstError = std::llabs(after.first()
        - qint64(std::llround(double(after.first()) / double(kBeatInterval))) * kBeatInterval);
    const qint64 lastError = std::llabs(after.last()
        - qint64(std::llround(double(after.last()) / double(kBeatInterval))) * kBeatInterval);
    QVERIFY2(lastError <= firstError + tolerance,
             qPrintable(QStringLiteral("ошибка копится к концу: %1 → %2").arg(firstError).arg(lastError)));
}

// Выравнивание не должно превращать дорожку в щелчки или тишину
void BeatAlignTest::testAlignmentKeepsAudioAlive()
{
    const QVector<qint64> beats { 0, 22050 + 1200, 44100 - 900, 66150 + 600 };
    const int total = 5 * kBeatInterval;
    const QVector<QVector<float>> audio = makeBeatTrack(beats, total);

    const TimeStretchProcessor::StretchResult result = TimeStretchProcessor::alignBeatsToGrid(
        audio, beats, kBpm, kSampleRate, 0);
    QVERIFY(!result.audioData.isEmpty());

    const double before = rms(audio[0]);
    const double after = rms(result.audioData[0]);
    QVERIFY2(after > before * 0.5,
             qPrintable(QStringLiteral("сигнал почти исчез: RMS %1 → %2").arg(before).arg(after)));
    QVERIFY2(after < before * 2.0,
             qPrintable(QStringLiteral("сигнал раздулся: RMS %1 → %2").arg(before).arg(after)));

    // Длина дорожки сохранена: клип в DAW не должен менять размер
    const qint64 lengthDelta = std::llabs(qint64(result.audioData[0].size()) - qint64(total));
    QVERIFY2(lengthDelta < kSampleRate / 10,
             qPrintable(QStringLiteral("длина уехала на %1 сэмплов").arg(lengthDelta)));

    // Ни одного «взорванного» отсчёта — это и был признак старой реализации,
    // складывавшей всю долю в один сэмпл
    float peak = 0.0f;
    for (float v : result.audioData[0]) {
        peak = std::max(peak, std::fabs(v));
    }
    QVERIFY2(peak < 4.0f, qPrintable(QStringLiteral("пик выхода %1 — похоже на щелчок").arg(peak)));
}

QTEST_MAIN(BeatAlignTest)
#include "beat_align_test.moc"
