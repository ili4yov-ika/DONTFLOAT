#include <QtTest/QTest>
#include <QtCore/QVector>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QtMath>
#include <cmath>
#include "../include/timestretchprocessor.h"
#include "../include/markerengine.h"
#include "../include/audiofileservice.h"

namespace {

constexpr const char* kPitchTestFile = "pitch-test_C140BPM.mp3";
constexpr float kPitchTolerance = 0.06f; // 6% — MP3 + Rubber Band на устойчивой ноте

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

QVector<float> loadMono(const QString& filePath, int& sampleRate)
{
    sampleRate = 0;
    const AudioFileService::DecodeResult res = AudioFileService::decode(filePath);
    if (!res.ok || res.channels.isEmpty()) {
        return {};
    }
    sampleRate = res.sampleRate;
    return AudioFileService::toMono(res.channels);
}

float estimateFundamentalHz(const QVector<float>& samples, int sampleRate, int start, int length)
{
    if (sampleRate <= 0 || length < sampleRate / 25 || start < 0 || start + length > samples.size()) {
        return 0.0f;
    }

    const int minLag = qMax(2, sampleRate / 2000);
    const int maxLag = qMin(length / 2, sampleRate / 50);

    float bestNorm = -1.0f;
    int bestLag = 0;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        double corr = 0.0;
        double energy = 0.0;
        const int n = length - lag;
        for (int i = 0; i < n; ++i) {
            const float a = samples[start + i];
            const float b = samples[start + i + lag];
            corr += static_cast<double>(a) * b;
            energy += static_cast<double>(a) * a;
        }
        if (energy <= 1e-12) {
            continue;
        }
        const float norm = static_cast<float>(corr / energy);
        if (norm > bestNorm) {
            bestNorm = norm;
            bestLag = lag;
        }
    }

    return bestLag > 0 ? static_cast<float>(sampleRate) / static_cast<float>(bestLag) : 0.0f;
}

float measureStablePitch(const QVector<float>& samples, int sampleRate)
{
    if (samples.size() < sampleRate / 4) {
        return 0.0f;
    }
    const int start = samples.size() * 2 / 10;
    const int end = samples.size() * 8 / 10;
    return estimateFundamentalHz(samples, sampleRate, start, end - start);
}

MarkerData makeMarker(qint64 originalPos, qint64 currentPos, int sampleRate, bool fixed, bool endMarker)
{
    MarkerData m(originalPos, fixed, endMarker, sampleRate);
    m.position = currentPos;
    m.originalPosition = originalPos;
    m.updateTimeFromSamples(sampleRate);
    return m;
}

QVector<MarkerData> wholeFileMarkers(qint64 audioSamples, qint64 endPosition, int sampleRate)
{
    return {
        makeMarker(0, 0, sampleRate, true, false),
        makeMarker(audioSamples, endPosition, sampleRate, false, true),
    };
}

QVector<MarkerData> splitFileMarkers(qint64 audioSamples,
                                     qint64 splitOriginal,
                                     qint64 splitPosition,
                                     qint64 endPosition,
                                     int sampleRate)
{
    return {
        makeMarker(0, 0, sampleRate, true, false),
        makeMarker(splitOriginal, splitPosition, sampleRate, false, false),
        makeMarker(audioSamples, endPosition, sampleRate, false, true),
    };
}

void skipIfCi()
{
    if (qEnvironmentVariableIsSet("CI") || qEnvironmentVariableIsSet("GITHUB_ACTIONS")) {
        QSKIP("Интеграционный тест тонкомпенсации на MP3 пропущен в CI "
              "(декодер и Rubber Band зависят от окружения, долгий прогон).");
    }
}

} // namespace

class PitchCompensationFileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testPitchTestFileHasStableTone();
    void testWholeFileCompressHalf();
    void testWholeFileStretchOneAndHalf();
    void testWholeFileStretchDouble();
    void testFirstHalfCompressSecondUnchanged();
    void testSecondHalfStretchFirstUnchanged();
    void testWithoutPitchCompensationPitchShifts();

private:
    QString m_filePath;
    QVector<float> m_mono;
    QVector<QVector<float>> m_channels;
    int m_sampleRate = 0;
    float m_referenceHz = 0.0f;
    qint64 m_audioSamples = 0;

    void requireLoadedFile();
    bool measurePitchAfterStretch(const QVector<MarkerData>& markers, bool preservePitch, float* outHz);
};

void PitchCompensationFileTest::initTestCase()
{
    skipIfCi();

    m_filePath = resolveTestDataPath(QString::fromUtf8(kPitchTestFile));
    if (m_filePath.isEmpty()) {
        QWARN(qPrintable(QStringLiteral("Тестовый файл не найден: %1").arg(kPitchTestFile)));
        return;
    }

    const AudioFileService::DecodeResult res = AudioFileService::decode(m_filePath);
    QVERIFY2(res.ok, qPrintable(res.error));
    QVERIFY(!res.channels.isEmpty());

    m_sampleRate = res.sampleRate;
    m_channels = res.channels;
    m_mono = AudioFileService::toMono(m_channels);
    m_audioSamples = m_mono.size();
    QVERIFY(m_audioSamples > m_sampleRate);
    QVERIFY(m_sampleRate > 0);

    m_referenceHz = measureStablePitch(m_mono, m_sampleRate);
    qDebug() << "Файл:" << m_filePath
             << "SR:" << m_sampleRate
             << "сэмплов:" << m_audioSamples
             << "длительность:" << (double(m_audioSamples) / m_sampleRate) << "с"
             << "f0:" << m_referenceHz << "Hz";
    QVERIFY2(m_referenceHz > 50.0f && m_referenceHz < 4000.0f,
             "Не удалось оценить устойчивую высоту тона на pitch-test файле");
}

void PitchCompensationFileTest::requireLoadedFile()
{
    if (m_filePath.isEmpty() || m_mono.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("Файл %1 недоступен").arg(kPitchTestFile)));
    }
}

bool PitchCompensationFileTest::measurePitchAfterStretch(const QVector<MarkerData>& markers,
                                                         bool preservePitch,
                                                         float* outHz)
{
    if (!outHz) {
        return false;
    }

    QString err;
    if (!TimeStretchProcessor::validateMarkers(markers, m_audioSamples, &err)) {
        qWarning() << "Метки невалидны:" << err;
        return false;
    }

    const TimeStretchProcessor::StretchResult result =
        TimeStretchProcessor::applyMarkerStretch(m_channels, markers, m_sampleRate, preservePitch);

    if (result.audioData.isEmpty() || result.audioData[0].size() <= m_sampleRate / 4) {
        return false;
    }

    const QVector<float> outMono = AudioFileService::toMono(result.audioData);
    const float hz = measureStablePitch(outMono, m_sampleRate);
    qDebug() << "  f0 после обработки:" << hz << "Hz, длина:" << outMono.size();
    if (hz <= 50.0f) {
        return false;
    }

    *outHz = hz;
    return true;
}

void PitchCompensationFileTest::testPitchTestFileHasStableTone()
{
    requireLoadedFile();
    QCOMPARE_GT(m_referenceHz, 80.0f);
    QCOMPARE_LT(m_referenceHz, 2000.0f);
}

void PitchCompensationFileTest::testWholeFileCompressHalf()
{
    requireLoadedFile();
    qDebug() << "\n=== Сжатие всего файла ×0.5 (метка конца L → L/2) ===";

    const QVector<MarkerData> markers = wholeFileMarkers(m_audioSamples, m_audioSamples / 2, m_sampleRate);
    float hz = 0.0f;
    QVERIFY(measurePitchAfterStretch(markers, true, &hz));
    const float rel = qAbs(hz - m_referenceHz) / m_referenceHz;
    QVERIFY2(rel <= kPitchTolerance,
               qPrintable(QStringLiteral("Тон сместился на %1% (ожидалось ≤%2%)")
                              .arg(rel * 100.0f, 0, 'f', 2)
                              .arg(kPitchTolerance * 100.0f, 0, 'f', 1)));
}

void PitchCompensationFileTest::testWholeFileStretchOneAndHalf()
{
    requireLoadedFile();
    qDebug() << "\n=== Растяжение всего файла ×1.5 ===";

    const qint64 newEnd = m_audioSamples + m_audioSamples / 2;
    const QVector<MarkerData> markers = wholeFileMarkers(m_audioSamples, newEnd, m_sampleRate);
    float hz = 0.0f;
    QVERIFY(measurePitchAfterStretch(markers, true, &hz));
    const float rel = qAbs(hz - m_referenceHz) / m_referenceHz;
    QVERIFY2(rel <= kPitchTolerance,
               qPrintable(QStringLiteral("Тон сместился на %1%").arg(rel * 100.0f, 0, 'f', 2)));
}

void PitchCompensationFileTest::testWholeFileStretchDouble()
{
    requireLoadedFile();
    qDebug() << "\n=== Растяжение всего файла ×2.0 ===";

    const QVector<MarkerData> markers = wholeFileMarkers(m_audioSamples, m_audioSamples * 2, m_sampleRate);
    float hz = 0.0f;
    QVERIFY(measurePitchAfterStretch(markers, true, &hz));
    const float rel = qAbs(hz - m_referenceHz) / m_referenceHz;
    QVERIFY2(rel <= kPitchTolerance,
               qPrintable(QStringLiteral("Тон сместился на %1%").arg(rel * 100.0f, 0, 'f', 2)));
}

void PitchCompensationFileTest::testFirstHalfCompressSecondUnchanged()
{
    requireLoadedFile();
    qDebug() << "\n=== Сжатие первой половины ×0.5, вторая без изменений ===";

    const qint64 half = m_audioSamples / 2;
    const qint64 quarter = m_audioSamples / 4;
    const qint64 endPos = quarter + half; // L/4 + L/2 = 3L/4

    const QVector<MarkerData> markers =
        splitFileMarkers(m_audioSamples, half, quarter, endPos, m_sampleRate);
    float hz = 0.0f;
    QVERIFY(measurePitchAfterStretch(markers, true, &hz));
    const float rel = qAbs(hz - m_referenceHz) / m_referenceHz;
    QVERIFY2(rel <= kPitchTolerance,
               qPrintable(QStringLiteral("Тон сместился на %1%").arg(rel * 100.0f, 0, 'f', 2)));
}

void PitchCompensationFileTest::testSecondHalfStretchFirstUnchanged()
{
    requireLoadedFile();
    qDebug() << "\n=== Растяжение второй половины ×1.5, первая без изменений ===";

    const qint64 half = m_audioSamples / 2;
    const qint64 tailOrig = m_audioSamples - half;
    const qint64 tailNew = tailOrig + tailOrig / 2;
    const qint64 endPos = half + tailNew;

    const QVector<MarkerData> markers =
        splitFileMarkers(m_audioSamples, half, half, endPos, m_sampleRate);
    float hz = 0.0f;
    QVERIFY(measurePitchAfterStretch(markers, true, &hz));
    const float rel = qAbs(hz - m_referenceHz) / m_referenceHz;
    QVERIFY2(rel <= kPitchTolerance,
               qPrintable(QStringLiteral("Тон сместился на %1%").arg(rel * 100.0f, 0, 'f', 2)));
}

void PitchCompensationFileTest::testWithoutPitchCompensationPitchShifts()
{
    requireLoadedFile();
    qDebug() << "\n=== Без тонкомпенсации: сжатие ×0.5 должно повысить тон ===";

    const QVector<MarkerData> markers = wholeFileMarkers(m_audioSamples, m_audioSamples / 2, m_sampleRate);
    float hz = 0.0f;
    QVERIFY(measurePitchAfterStretch(markers, false, &hz));
    const float ratio = hz / m_referenceHz;
    qDebug() << "  отношение f_out/f_in:" << ratio << "(ожидалось ~2.0 без тонкомпенсации)";
    QVERIFY2(ratio > 1.35f && ratio < 2.8f,
             qPrintable(QStringLiteral("Без тонкомпенсации тон должен заметно вырасти, получено ×%1")
                            .arg(ratio, 0, 'f', 2)));
}

QTEST_MAIN(PitchCompensationFileTest)
#include "pitch_compensation_file_test.moc"
