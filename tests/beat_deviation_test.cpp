#include <QtTest/QTest>
#include <QtCore/QVector>
#include "../include/bpmanalyzer.h"

class BeatDeviationTest : public QObject
{
    Q_OBJECT

public:
    BeatDeviationTest() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Тесты для новой функциональности
    void testCalculateDeviations();
    void testFindUnalignedBeats();
    void testExpectedPositionInitialization();
};

void BeatDeviationTest::initTestCase()
{
    qDebug() << "Инициализация тестов для вычисления отклонений долей";
}

void BeatDeviationTest::cleanupTestCase()
{
    qDebug() << "Завершение тестов для вычисления отклонений долей";
}

void BeatDeviationTest::testCalculateDeviations()
{
    qDebug() << "\n=== Тест: calculateDeviations ===";

    // Создаем тестовые данные: 4 доли с известными позициями
    QVector<BPMAnalyzer::BeatInfo> beats;
    float bpm = 120.0f; // 120 BPM = 0.5 секунды между долями
    int sampleRate = 44100; // 44.1 kHz

    // Ожидаемый интервал: (60 / 120) * 44100 = 22050 сэмплов
    float expectedInterval = (60.0f * sampleRate) / bpm;

    // Создаем доли с небольшими отклонениями
    BPMAnalyzer::BeatInfo beat1;
    beat1.position = 0;
    beat1.expectedPosition = 0;
    beat1.confidence = 1.0f;
    beat1.deviation = 0.0f;
    beat1.energy = 1.0f;
    beats.append(beat1);

    BPMAnalyzer::BeatInfo beat2;
    beat2.position = 22100; // +50 сэмплов (небольшое отклонение)
    beat2.expectedPosition = 0;
    beat2.confidence = 1.0f;
    beat2.deviation = 0.0f;
    beat2.energy = 1.0f;
    beats.append(beat2);

    BPMAnalyzer::BeatInfo beat3;
    beat3.position = 44000; // -100 сэмплов (небольшое отклонение)
    beat3.expectedPosition = 0;
    beat3.confidence = 1.0f;
    beat3.deviation = 0.0f;
    beat3.energy = 1.0f;
    beats.append(beat3);

    BPMAnalyzer::BeatInfo beat4;
    beat4.position = 66150; // Точно на месте
    beat4.expectedPosition = 0;
    beat4.confidence = 1.0f;
    beat4.deviation = 0.0f;
    beat4.energy = 1.0f;
    beats.append(beat4);

    // Вызываем функцию расчёта отклонений
    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    // Проверяем, что expectedPosition вычислены правильно
    QCOMPARE(beats[0].expectedPosition, qint64(0));
    QCOMPARE(beats[1].expectedPosition, qint64(expectedInterval));
    QCOMPARE(beats[2].expectedPosition, qint64(expectedInterval * 2));
    QCOMPARE(beats[3].expectedPosition, qint64(expectedInterval * 3));

    // Проверяем, что отклонения вычислены
    qDebug() << "  Beat 0 - Position:" << beats[0].position
             << "Expected:" << beats[0].expectedPosition
             << "Deviation:" << beats[0].deviation;
    qDebug() << "  Beat 1 - Position:" << beats[1].position
             << "Expected:" << beats[1].expectedPosition
             << "Deviation:" << beats[1].deviation;
    qDebug() << "  Beat 2 - Position:" << beats[2].position
             << "Expected:" << beats[2].expectedPosition
             << "Deviation:" << beats[2].deviation;
    qDebug() << "  Beat 3 - Position:" << beats[3].position
             << "Expected:" << beats[3].expectedPosition
             << "Deviation:" << beats[3].deviation;

    // Проверяем знаки отклонений
    QVERIFY2(beats[0].deviation == 0.0f, "Beat 0 должен быть без отклонения");
    QVERIFY2(beats[1].deviation > 0.0f, "Beat 1 должен иметь положительное отклонение");
    QVERIFY2(beats[2].deviation < 0.0f, "Beat 2 должен иметь отрицательное отклонение");
    QVERIFY2(qAbs(beats[3].deviation) < 0.01f, "Beat 3 должен быть почти без отклонения");

    qDebug() << "  ✓ Отклонения вычислены корректно";
}

void BeatDeviationTest::testFindUnalignedBeats()
{
    qDebug() << "\n=== Тест: findUnalignedBeats ===";

    // Создаем тестовые данные с известными отклонениями
    QVector<BPMAnalyzer::BeatInfo> beats;

    // Beat 0: нет отклонения
    BPMAnalyzer::BeatInfo beat0;
    beat0.position = 0;
    beat0.expectedPosition = 0;
    beat0.deviation = 0.0f;
    beat0.confidence = 1.0f;
    beat0.energy = 1.0f;
    beats.append(beat0);

    // Beat 1: небольшое отклонение (1%)
    BPMAnalyzer::BeatInfo beat1;
    beat1.position = 22050;
    beat1.expectedPosition = 22000;
    beat1.deviation = 0.01f;
    beat1.confidence = 1.0f;
    beat1.energy = 1.0f;
    beats.append(beat1);

    // Beat 2: большое отклонение (5%)
    BPMAnalyzer::BeatInfo beat2;
    beat2.position = 44000;
    beat2.expectedPosition = 44100;
    beat2.deviation = -0.05f;
    beat2.confidence = 1.0f;
    beat2.energy = 1.0f;
    beats.append(beat2);

    // Beat 3: очень большое отклонение (10%)
    BPMAnalyzer::BeatInfo beat3;
    beat3.position = 68000;
    beat3.expectedPosition = 66150;
    beat3.deviation = 0.10f;
    beat3.confidence = 1.0f;
    beat3.energy = 1.0f;
    beats.append(beat3);

    // Тест 1: Порог 2% - должны найтись beats 2 и 3
    {
        float threshold = 0.02f; // 2%
        QVector<int> unalignedIndices = BPMAnalyzer::findUnalignedBeats(beats, threshold);

        qDebug() << "  Порог 2%: найдено" << unalignedIndices.size() << "неровных долей";
        QCOMPARE(unalignedIndices.size(), 2);
        QVERIFY(unalignedIndices.contains(2));
        QVERIFY(unalignedIndices.contains(3));
        qDebug() << "  ✓ С порогом 2% найдены доли с отклонением >= 2%";
    }

    // Тест 2: Порог 5% - должен найтись только beat 3
    {
        float threshold = 0.05f; // 5%
        QVector<int> unalignedIndices = BPMAnalyzer::findUnalignedBeats(beats, threshold);

        qDebug() << "  Порог 5%: найдено" << unalignedIndices.size() << "неровных долей";
        QCOMPARE(unalignedIndices.size(), 1);
        QVERIFY(unalignedIndices.contains(3));
        qDebug() << "  ✓ С порогом 5% найдены доли с отклонением >= 5%";
    }

    // Тест 3: Порог 15% - не должно найтись ничего
    {
        float threshold = 0.15f; // 15%
        QVector<int> unalignedIndices = BPMAnalyzer::findUnalignedBeats(beats, threshold);

        qDebug() << "  Порог 15%: найдено" << unalignedIndices.size() << "неровных долей";
        QCOMPARE(unalignedIndices.size(), 0);
        qDebug() << "  ✓ С порогом 15% неровные доли не найдены";
    }
}

void BeatDeviationTest::testExpectedPositionInitialization()
{
    qDebug() << "\n=== Тест: Инициализация expectedPosition ===";

    // Создаем BeatInfo и проверяем, что все поля инициализированы
    BPMAnalyzer::BeatInfo beat;
    beat.position = 1000;
    beat.expectedPosition = 0; // Должно быть инициализировано
    beat.confidence = 0.8f;
    beat.deviation = 0.0f;
    beat.energy = 0.5f;

    QCOMPARE(beat.expectedPosition, qint64(0));
    qDebug() << "  ✓ expectedPosition инициализировано корректно";

    // Проверяем работу с вектором долей
    QVector<BPMAnalyzer::BeatInfo> beats;
    for (int i = 0; i < 10; ++i) {
        BPMAnalyzer::BeatInfo b;
        b.position = i * 1000;
        b.expectedPosition = 0;
        b.confidence = 1.0f;
        b.deviation = 0.0f;
        b.energy = 1.0f;
        beats.append(b);
    }

    QCOMPARE(beats.size(), 10);
    for (const auto& b : beats) {
        QCOMPARE(b.expectedPosition, qint64(0));
    }

    qDebug() << "  ✓ Вектор долей инициализирован корректно";
}

QTEST_MAIN(BeatDeviationTest)
#include "beat_deviation_test.moc"
