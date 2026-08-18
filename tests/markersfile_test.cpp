#include <QtTest/QTest>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include "../include/markersfile.h"
#include "../include/markerengine.h"
#include "../include/timeutils.h"

#include <cmath>

class MarkersFileTest : public QObject
{
    Q_OBJECT

private slots:
    void testFormatAndParseRoundTrip();
    void testDetectOnsetSamples();
};

// Общий алгоритм кнопки «OD»: им пользуются и главное окно, и плагины
void MarkersFileTest::testDetectOnsetSamples()
{
    const int sampleRate = 44100;
    const int clickCount = 4;
    const int stepSamples = sampleRate / 2;  // клик каждые 500 мс

    // Первый клик не в нуле: нарастание огибающей считается от предыдущего
    // сэмпла, поэтому транзиент в самом начале файла алгоритм не видит
    const int firstClickSamples = sampleRate / 10;

    QVector<float> mono(firstClickSamples + clickCount * stepSamples, 0.0f);
    for (int click = 0; click < clickCount; ++click) {
        const int start = firstClickSamples + click * stepSamples;
        // Короткий затухающий импульс — транзиент
        for (int i = 0; i < 400 && start + i < mono.size(); ++i) {
            mono[start + i] = 0.9f * std::exp(-float(i) / 60.0f);
        }
    }

    const QVector<qint64> onsets =
        MarkerUtils::detectOnsetSamples(QVector<QVector<float>>{ mono }, sampleRate);
    QCOMPARE(onsets.size(), clickCount);

    const qint64 tolerance = sampleRate / 50;  // 20 мс
    for (int click = 0; click < clickCount; ++click) {
        const qint64 expected = firstClickSamples + qint64(click) * stepSamples;
        QVERIFY(qAbs(onsets[click] - expected) <= tolerance);
    }

    // Тишина транзиентов не даёт
    QVERIFY(MarkerUtils::detectOnsetSamples(
                QVector<QVector<float>>{ QVector<float>(sampleRate, 0.0f) }, sampleRate)
                .isEmpty());
}

void MarkersFileTest::testFormatAndParseRoundTrip()
{
    const int sampleRate = 44100;
    // 52138 сэмплов → 1182 ms → 0:01:18 (M:SS:CC)
    const qint64 samplePos = (1182LL * sampleRate) / 1000;
    const QString formatted = MarkersFile::formatTimeMssCc(samplePos, sampleRate);
    QCOMPARE(formatted, QStringLiteral("0:01:18"));

    MarkersFileMeta meta;
    meta.bpm = 60.f;
    meta.beatsPerBar = 4;
    meta.sampleRate = sampleRate;
    meta.gridStartSample = 0;

    QVector<Marker> markers;
    markers.append(Marker(0, true, sampleRate));
    Marker m(samplePos, sampleRate);
    markers.append(m);

    const QString path = QDir::temp().filePath(QStringLiteral("dontfloat_markers_test.txt"));
    QString err;
    QVERIFY(MarkersFile::writeFile(path, meta, markers, sampleRate, &err));

    MarkersFileMeta readMeta;
    QVector<MarkersFileEntry> entries;
    QVERIFY(MarkersFile::readFile(path, &readMeta, &entries, &err));
    QCOMPARE(entries.size(), 2);

    const MarkersFileEntry* beatEntry = nullptr;
    for (const MarkersFileEntry& e : entries) {
        if (e.samplePosition > 0) {
            beatEntry = &e;
            break;
        }
    }
    QVERIFY(beatEntry);
    const qint64 maxSampleDelta = qMax<qint64>(1, sampleRate / 100); // точность формата M:SS:CC — 10 ms
    QVERIFY(qAbs(beatEntry->samplePosition - samplePos) <= maxSampleDelta);

    QFile::remove(path);
}

QTEST_MAIN(MarkersFileTest)
#include "markersfile_test.moc"
