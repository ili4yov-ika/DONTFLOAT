// Пирамида пиков волны: значения должны совпадать с прямым перебором сэмплов.
//
// Отрисовка волны берёт min/max отсюда, поэтому ошибка здесь — это неверная
// форма волны на экране. Раньше пики считались с прореживанием (каждый N-й
// сэмпл), и короткие всплески пропадали — тест на это тоже есть.

#include <QtTest/QTest>

#include "../include/waveformpeaks.h"

#include <cmath>

namespace {

/** Прямой перебор — эталон для сравнения. */
void bruteForceRange(const QVector<float>& samples, qint64 from, qint64 to,
                     float& minValue, float& maxValue)
{
    minValue = samples[int(from)];
    maxValue = minValue;
    for (qint64 i = from + 1; i < to; ++i) {
        minValue = std::min(minValue, samples[int(i)]);
        maxValue = std::max(maxValue, samples[int(i)]);
    }
}

QVector<float> makeSignal(int size)
{
    QVector<float> samples(size);
    for (int i = 0; i < size; ++i) {
        samples[i] = float(0.6 * std::sin(i * 0.01) + 0.2 * std::sin(i * 0.31));
    }
    return samples;
}

} // namespace

class WaveformPeaksTest : public QObject
{
    Q_OBJECT

private slots:
    void testMatchesBruteForceOnManyRanges();
    void testCatchesSingleSampleSpike();
    void testShortRangesAndEdges();
    void testRejectsForeignSamples();
    void testEmptyInput();
};

// Пики не у́же истинных и не шире, чем окно, расширенное на одну корзину
void WaveformPeaksTest::testMatchesBruteForceOnManyRanges()
{
    const QVector<float> samples = makeSignal(200000);
    WaveformPeaks peaks;
    peaks.build(samples);
    QVERIFY(peaks.isValid());
    QCOMPARE(peaks.sampleCount(), qint64(samples.size()));

    const qint64 spans[] = { 1, 7, 255, 256, 257, 1000, 4096, 33333, 100000 };
    for (qint64 span : spans) {
        for (qint64 from = 0; from + span <= samples.size(); from += span * 3 + 13) {
            float peakMin = 0.0f;
            float peakMax = 0.0f;
            QVERIFY(peaks.range(samples, from, from + span, peakMin, peakMax));

            float refMin = 0.0f;
            float refMax = 0.0f;
            bruteForceRange(samples, from, from + span, refMin, refMax);

            // Всплеск не теряется ни при каком масштабе
            QVERIFY2(peakMin <= refMin && peakMax >= refMax,
                     qPrintable(QStringLiteral("узкий диапазон: span=%1 from=%2 [%3..%4] vs [%5..%6]")
                                    .arg(span).arg(from)
                                    .arg(peakMin).arg(peakMax).arg(refMin).arg(refMax)));

            if (span <= WaveformPeaks::kBaseBucketSamples * 2) {
                // Вблизи считается точно по сэмплам
                QCOMPARE(peakMin, refMin);
                QCOMPARE(peakMax, refMax);
                continue;
            }

            // Вдали край может прихватить соседей — но не больше, чем окно,
            // расширенное на одну корзину подходящего уровня
            const qint64 slack = qMax<qint64>(WaveformPeaks::kBaseBucketSamples,
                                              span / WaveformPeaks::kMinBucketsPerRange);
            float wideMin = 0.0f;
            float wideMax = 0.0f;
            bruteForceRange(samples, qMax<qint64>(0, from - slack),
                            qMin<qint64>(samples.size(), from + span + slack),
                            wideMin, wideMax);
            QVERIFY2(peakMin >= wideMin && peakMax <= wideMax,
                     qPrintable(QStringLiteral("слишком широкий диапазон: span=%1 from=%2")
                                    .arg(span).arg(from)));
        }
    }
}

// Одиночный всплеск виден в любом масштабе — прореживание его теряло
void WaveformPeaksTest::testCatchesSingleSampleSpike()
{
    QVector<float> samples = makeSignal(120000);
    const int spikeIndex = 54321;
    samples[spikeIndex] = 0.99f;
    samples[spikeIndex + 1] = -0.98f;

    WaveformPeaks peaks;
    peaks.build(samples);

    float peakMin = 0.0f;
    float peakMax = 0.0f;
    // Отрезок на весь сигнал: всплеск должен попасть в пики
    QVERIFY(peaks.range(samples, 0, samples.size(), peakMin, peakMax));
    QCOMPARE(peakMax, 0.99f);
    QCOMPARE(peakMin, -0.98f);
    // Именно этого прежний способ и не умел: при прореживании такой всплеск
    // между проверяемыми сэмплами просто пропадал

    // И на «пиксельном» отрезке вокруг него
    QVERIFY(peaks.range(samples, spikeIndex - 500, spikeIndex + 500, peakMin, peakMax));
    QCOMPARE(peakMax, 0.99f);
    QCOMPARE(peakMin, -0.98f);
}

// Короткие отрезки и края сигнала считаются напрямую и не выходят за границы
void WaveformPeaksTest::testShortRangesAndEdges()
{
    const QVector<float> samples = makeSignal(5000);
    WaveformPeaks peaks;
    peaks.build(samples);

    float peakMin = 0.0f;
    float peakMax = 0.0f;

    QVERIFY(peaks.range(samples, 0, 1, peakMin, peakMax));
    QCOMPARE(peakMin, samples.first());
    QCOMPARE(peakMax, samples.first());

    QVERIFY(peaks.range(samples, samples.size() - 1, samples.size(), peakMin, peakMax));
    QCOMPARE(peakMin, samples.last());

    // Запрос за концом обрезается по длине сигнала
    QVERIFY(peaks.range(samples, samples.size() - 10, samples.size() + 5000, peakMin, peakMax));
    float refMin = 0.0f;
    float refMax = 0.0f;
    bruteForceRange(samples, samples.size() - 10, samples.size(), refMin, refMax);
    QCOMPARE(peakMin, refMin);
    QCOMPARE(peakMax, refMax);

    // Пустой и перевёрнутый отрезок — отказ, а не мусор
    QVERIFY(!peaks.range(samples, 100, 100, peakMin, peakMax));
    QVERIFY(!peaks.range(samples, 200, 100, peakMin, peakMax));
}

// Пирамида от другого буфера не используется: рисовать по ней нельзя
void WaveformPeaksTest::testRejectsForeignSamples()
{
    const QVector<float> samples = makeSignal(20000);
    WaveformPeaks peaks;
    peaks.build(samples);

    const QVector<float> other = makeSignal(19000);
    float peakMin = 0.0f;
    float peakMax = 0.0f;
    QVERIFY(!peaks.range(other, 0, other.size(), peakMin, peakMax));
}

void WaveformPeaksTest::testEmptyInput()
{
    WaveformPeaks peaks;
    peaks.build({});
    QVERIFY(!peaks.isValid());
    QCOMPARE(peaks.sampleCount(), qint64(0));

    float peakMin = 0.0f;
    float peakMax = 0.0f;
    QVERIFY(!peaks.range({}, 0, 10, peakMin, peakMax));
}

QTEST_APPLESS_MAIN(WaveformPeaksTest)
#include "waveform_peaks_test.moc"
