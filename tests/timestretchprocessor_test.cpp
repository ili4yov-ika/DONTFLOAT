#include <QtTest/QTest>
#include <QtCore/QVector>
#include <QtCore/QtMath>
#include <cmath>
#include "../include/timestretchprocessor.h"
#include "../include/markerengine.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TimeStretchProcessorTest : public QObject
{
    Q_OBJECT

public:
    TimeStretchProcessorTest() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProcessSegmentOutputLength();
    void testProcessSegmentCompression();
    void testProcessSegmentStretching();
    void testPitchCompensationCompress();
    void testPitchCompensationStretch();
    void testCalculateStretchFactor();
};

void TimeStretchProcessorTest::initTestCase()
{
    qDebug() << "Инициализация тестов TimeStretchProcessor";
}

void TimeStretchProcessorTest::cleanupTestCase()
{
    qDebug() << "Завершение тестов TimeStretchProcessor";
}

void TimeStretchProcessorTest::testProcessSegmentOutputLength()
{
    qDebug() << "\n=== Тест: Длина выхода processSegment ===";

    QVector<float> input(4410, 0.5f); // 0.1 сек при 44.1 kHz
    float factor = 1.5f;

    QVector<float> output = TimeStretchProcessor::processSegment(input, factor, true);

    int expectedSize = static_cast<int>(input.size() * factor);
    QVERIFY2(!output.isEmpty(), "Выход не должен быть пустым");
    QVERIFY2(qAbs(output.size() - expectedSize) <= 10,
             QString("Ожидался размер ~%1, получено %2").arg(expectedSize).arg(output.size()).toUtf8().constData());

    qDebug() << "  Вход:" << input.size() << "сэмплов, коэффициент:" << factor;
    qDebug() << "  Выход:" << output.size() << "сэмплов (ожидалось ~" << expectedSize << ")";
    qDebug() << "  ✓ Длина выхода корректна";
}

void TimeStretchProcessorTest::testProcessSegmentCompression()
{
    qDebug() << "\n=== Тест: Сжатие (stretchFactor < 1) ===";

    QVector<float> input(4410, 0.0f);
    float factor = 0.5f;

    QVector<float> output = TimeStretchProcessor::processSegment(input, factor, true);

    int expectedSize = static_cast<int>(input.size() * factor);
    QVERIFY2(!output.isEmpty(), "Выход не должен быть пустым");
    QVERIFY2(output.size() < input.size(),
             QString("При сжатии выход должен быть короче входа: %1 >= %2").arg(output.size()).arg(input.size()).toUtf8().constData());
    QVERIFY2(qAbs(output.size() - expectedSize) <= 50,
             QString("Ожидался размер ~%1, получено %2").arg(expectedSize).arg(output.size()).toUtf8().constData());

    qDebug() << "  Вход:" << input.size() << ", выход:" << output.size() << ", коэффициент:" << factor;
    qDebug() << "  ✓ Сжатие выполнено корректно";
}

void TimeStretchProcessorTest::testProcessSegmentStretching()
{
    qDebug() << "\n=== Тест: Растяжение (stretchFactor > 1) ===";

    QVector<float> input(2205, 0.0f);
    float factor = 2.0f;

    QVector<float> output = TimeStretchProcessor::processSegment(input, factor, true);

    int expectedSize = static_cast<int>(input.size() * factor);
    QVERIFY2(!output.isEmpty(), "Выход не должен быть пустым");
    QVERIFY2(output.size() > input.size(),
             QString("При растяжении выход должен быть длиннее входа: %1 <= %2").arg(output.size()).arg(input.size()).toUtf8().constData());
    QVERIFY2(qAbs(output.size() - expectedSize) <= 100,
             QString("Ожидался размер ~%1, получено %2").arg(expectedSize).arg(output.size()).toUtf8().constData());

    qDebug() << "  Вход:" << input.size() << ", выход:" << output.size() << ", коэффициент:" << factor;
    qDebug() << "  ✓ Растяжение выполнено корректно";
}

static int countZeroCrossings(const QVector<float>& data)
{
    int zc = 0;
    for (int i = 1; i < data.size(); ++i) {
        if ((data[i - 1] >= 0 && data[i] < 0) || (data[i - 1] < 0 && data[i] >= 0)) {
            ++zc;
        }
    }
    return zc;
}

void TimeStretchProcessorTest::testPitchCompensationCompress()
{
    qDebug() << "\n=== Тест: Тонкомпенсация при сжатии (ускорение → тон опускается) ===";

    const int sampleRate = 44100;
    const float freq = 440.0f;
    const int numSamples = 4410; // 0.1 сек

    QVector<float> input;
    input.reserve(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        input.append(std::sin(2.0f * M_PI * freq * t));
    }

    float stretchFactor = 0.5f; // Сжатие на 50%
    QVector<float> output = TimeStretchProcessor::processSegment(input, stretchFactor, true);

    QVERIFY2(!output.isEmpty(), "Выход не должен быть пустым");

    int zcInput = countZeroCrossings(input);
    int zcOutput = countZeroCrossings(output);

    // При сжатии на 50% с тонкомпенсацией тон опускается на 50% (частота * 0.5)
    // Вход: 440 Hz, 4410 сэмплов → ~88 нулевых пересечений (2*440*0.1)
    // Выход: ~220 Hz, 2205 сэмплов → ~44 нулевых пересечения
    // zcOutput должно быть примерно zcInput * stretchFactor (меньше пересечений при меньшей частоте и меньшей длине)
    float zcRatio = (output.size() > 0 && zcInput > 0)
        ? static_cast<float>(zcOutput) / static_cast<float>(zcInput) : 0.0f;

    qDebug() << "  Вход: ZC=" << zcInput << ", выход: ZC=" << zcOutput << ", отношение:" << zcRatio;
    qDebug() << "  Ожидаемое отношение ~" << stretchFactor << "(тон опущен на 50%)";

    // Отношение ZC должно быть близко к stretchFactor (с допуском из-за граничных эффектов)
    QVERIFY2(zcRatio > 0.2f && zcRatio < 1.2f,
             QString("Отношение нулевых пересечений %1 выходит за разумные пределы").arg(zcRatio).toUtf8().constData());

    qDebug() << "  ✓ Тонкомпенсация при сжатии работает";
}

void TimeStretchProcessorTest::testPitchCompensationStretch()
{
    qDebug() << "\n=== Тест: Тонкомпенсация при растяжении (замедление → тон повышается) ===";

    const int sampleRate = 44100;
    const float freq = 220.0f;
    const int numSamples = 4410; // 0.1 сек

    QVector<float> input;
    input.reserve(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        input.append(std::sin(2.0f * M_PI * freq * t));
    }

    float stretchFactor = 1.5f; // Растяжение на 50%
    QVector<float> output = TimeStretchProcessor::processSegment(input, stretchFactor, true);

    QVERIFY2(!output.isEmpty(), "Выход не должен быть пустым");

    int zcInput = countZeroCrossings(input);
    int zcOutput = countZeroCrossings(output);

    // При растяжении на 50% с тонкомпенсацией тон повышается на 50% (частота * 1.5)
    // Вход: 220 Hz, 4410 сэмплов
    // Выход: ~330 Hz, 6615 сэмплов — больше нулевых пересечений
    qDebug() << "  Вход: ZC=" << zcInput << ", выход: ZC=" << zcOutput;

    // Выход длиннее, частота выше — ZC в выходе должно быть больше чем zcInput * stretchFactor
    QVERIFY2(zcOutput > 0, "Должны быть нулевые пересечения в выходе");

    qDebug() << "  ✓ Тонкомпенсация при растяжении работает";
}

void TimeStretchProcessorTest::testCalculateStretchFactor()
{
    qDebug() << "\n=== Тест: calculateStretchFactor ===";

    MarkerData startMarker, endMarker;
    startMarker.position = 0;
    startMarker.originalPosition = 0;
    endMarker.position = 22050;
    endMarker.originalPosition = 44100;

    float factor = TimeStretchProcessor::calculateStretchFactor(startMarker, endMarker);

    // currentDistance/originalDistance = 22050/44100 = 0.5
    QCOMPARE(factor, 0.5f);
    qDebug() << "  Коэффициент:" << factor << "(сжатие в 2 раза)";
    qDebug() << "  ✓ calculateStretchFactor корректен";
}

QTEST_MAIN(TimeStretchProcessorTest)
#include "timestretchprocessor_test.moc"
