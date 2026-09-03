#include <QtTest/QTest>
#include <QtCore/QVector>

#include <cmath>
#include <limits>

#include "../include/bpmanalyzer.h"

namespace {

/** Идеальная сетка из n долей с шагом interval сэмплов. */
QVector<BPMAnalyzer::BeatInfo> makeGrid(int n, double interval, qint64 origin = 0)
{
    QVector<BPMAnalyzer::BeatInfo> beats;
    beats.reserve(n);
    for (int i = 0; i < n; ++i) {
        BPMAnalyzer::BeatInfo beat;
        beat.position = origin + qint64(std::llround(i * interval));
        beat.expectedPosition = 0;
        beat.confidence = 1.0f;
        beat.deviation = 0.0f;
        beat.energy = 1.0f;
        beats.append(beat);
    }
    return beats;
}

float maxAbsDeviation(const QVector<BPMAnalyzer::BeatInfo>& beats)
{
    float worst = 0.0f;
    for (const auto& beat : beats) {
        worst = qMax(worst, qAbs(beat.deviation));
    }
    return worst;
}

} // namespace

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

    // Адаптивные пороги и группировка
    void testAdaptiveThreshold();
    void testRegionGrouping();
    void testCorrectionSelection();

    // Устойчивость поиска неровных долей
    void testMissingBeatDoesNotShiftRest();
    void testSpuriousBeatIsIsolated();
    void testShiftedFirstBeatDoesNotFlagTrack();
    void testExplicitGridStartIsRespected();
    void testTempoDriftIsCompensatedOnDemand();
    void testLongTrackKeepsSampleAccuracy();
    void testDegenerateInputIsSafe();
    void testStatsSummariseDeviations();
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

void BeatDeviationTest::testMissingBeatDoesNotShiftRest()
{
    // Детектор не нашёл одну долю. Сопоставление по порядковому номеру сдвигало
    // бы всю оставшуюся часть трека на целый интервал.
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(64, interval);
    beats.remove(20);

    const BPMAnalyzer::DeviationStats stats =
        BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate, BPMAnalyzer::DeviationOptions());

    QVERIFY2(maxAbsDeviation(beats) < 0.001f, "пропуск доли не должен сдвигать сетку");
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(beats, 0.02f).size(), qsizetype(0));
    QCOMPARE(stats.gapCount, 1);
    QCOMPARE(stats.duplicateCount, 0);
}

void BeatDeviationTest::testSpuriousBeatIsIsolated()
{
    // Ложное срабатывание между долями: неровной считается только сама лишняя доля.
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(64, interval);
    BPMAnalyzer::BeatInfo spurious = beats[20];
    spurious.position += qint64(interval / 2);
    beats.insert(21, spurious);

    const BPMAnalyzer::DeviationStats stats =
        BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate, BPMAnalyzer::DeviationOptions());

    const QVector<int> unaligned = BPMAnalyzer::findUnalignedBeats(beats, 0.02f);
    QCOMPARE(unaligned.size(), qsizetype(1));
    QCOMPARE(unaligned.first(), 21);
    QCOMPARE(stats.duplicateCount, 1);
    QVERIFY2(qAbs(beats.last().deviation) < 0.001f, "хвост трека должен остаться ровным");
}

void BeatDeviationTest::testShiftedFirstBeatDoesNotFlagTrack()
{
    // Первая доля — затакт/шум со сдвигом 10%. Опора на неё раньше объявляла
    // неровным весь трек; фаза сетки берётся по медиане остатков.
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(64, interval);
    beats[0].position += qint64(interval * 0.10);

    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    const QVector<int> unaligned = BPMAnalyzer::findUnalignedBeats(beats, 0.02f);
    QCOMPARE(unaligned.size(), qsizetype(1));
    QCOMPARE(unaligned.first(), 0);
    QVERIFY(qAbs(beats[0].deviation - 0.10f) < 0.005f);
}

void BeatDeviationTest::testExplicitGridStartIsRespected()
{
    // Заданная опорная линия (та, что нарисована на волне) не подменяется
    // автоматической оценкой: ожидаемые позиции ложатся ровно на сетку.
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;
    const qint64 gridStart = 5000;

    // Все доли равномерно опаздывают на 4% — сетка при этом не должна «подъехать».
    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(32, interval, gridStart + qint64(interval * 0.04));

    BPMAnalyzer::DeviationOptions options;
    options.gridStartSample = gridStart;
    const BPMAnalyzer::DeviationStats stats =
        BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate, options);

    QCOMPARE(stats.gridStartSample, gridStart);
    QCOMPARE(beats[0].expectedPosition, gridStart);
    QCOMPARE(beats[10].expectedPosition, gridStart + qint64(std::llround(10 * interval)));
    for (const auto& beat : beats) {
        QVERIFY(qAbs(beat.deviation - 0.04f) < 0.005f);
    }
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(beats, 0.02f).size(), qsizetype(beats.size()));
}

void BeatDeviationTest::testTempoDriftIsCompensatedOnDemand()
{
    // Ровный трек на 120.5 BPM при сетке 120: без уточнения темпа отклонения
    // растут линейно и «неровным» становится весь конец трека.
    const int sampleRate = 44100;
    const float gridBPM = 120.0f;
    const double realInterval = (60.0 * sampleRate) / 120.5;

    QVector<BPMAnalyzer::BeatInfo> drifting = makeGrid(64, realInterval);
    BPMAnalyzer::calculateDeviations(drifting, gridBPM, sampleRate);
    QVERIFY2(BPMAnalyzer::findUnalignedBeats(drifting, 0.02f).size() > 10,
             "без уточнения темпа дрейф ожидаемо виден");

    QVector<BPMAnalyzer::BeatInfo> refined = makeGrid(64, realInterval);
    refined[30].position += qint64(realInterval * 0.09);  // настоящая неровность

    BPMAnalyzer::DeviationOptions options;
    options.refineTempo = true;
    const BPMAnalyzer::DeviationStats stats =
        BPMAnalyzer::calculateDeviations(refined, gridBPM, sampleRate, options);

    QVERIFY2(qAbs(stats.gridBPM - 120.5f) < 0.05f, "темп сетки должен уточниться до реального");

    const QVector<int> unaligned = BPMAnalyzer::findUnalignedBeats(refined, 0.02f);
    QCOMPARE(unaligned.size(), qsizetype(1));
    QCOMPARE(unaligned.first(), 30);  // джиттер не растворился в уточнении темпа
}

void BeatDeviationTest::testLongTrackKeepsSampleAccuracy()
{
    // 192 kHz, ~14 минут: позиции выходят за диапазон точных целых float.
    const int sampleRate = 192000;
    const float bpm = 140.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(2000, interval);
    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    for (int i = 0; i < beats.size(); ++i) {
        const double ideal = i * interval;
        QVERIFY2(qAbs(double(beats[i].expectedPosition) - ideal) <= 1.0,
                 qPrintable(QStringLiteral("beat %1: expected=%2 ideal=%3")
                                .arg(i)
                                .arg(beats[i].expectedPosition)
                                .arg(ideal)));
    }
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(beats, 0.001f).size(), qsizetype(0));
}

void BeatDeviationTest::testDegenerateInputIsSafe()
{
    // Пустой вход и некорректные параметры не должны ничего портить.
    QVector<BPMAnalyzer::BeatInfo> empty;
    QCOMPARE(BPMAnalyzer::calculateDeviations(empty, 120.0f, 44100, BPMAnalyzer::DeviationOptions()).beatCount, 0);
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(empty, 0.02f).size(), qsizetype(0));

    QVector<BPMAnalyzer::BeatInfo> single = makeGrid(1, 22050.0, 12345);
    single[0].expectedPosition = -999;
    single[0].deviation = 7.0f;
    BPMAnalyzer::calculateDeviations(single, 120.0f, 44100);
    QCOMPARE(single[0].expectedPosition, qint64(12345));
    QCOMPARE(single[0].deviation, 0.0f);

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(8, 22050.0);
    BPMAnalyzer::calculateDeviations(beats, 0.0f, 44100);   // BPM не определён
    BPMAnalyzer::calculateDeviations(beats, 120.0f, 0);     // нет частоты дискретизации
    QCOMPARE(beats[3].deviation, 0.0f);

    // Нечисловое отклонение не должно попадать в список неровных долей.
    beats[4].deviation = std::numeric_limits<float>::quiet_NaN();
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(beats, 0.02f).size(), qsizetype(0));

    // Доля с низкой уверенностью отсеивается порогом minConfidence.
    beats[4].deviation = 0.2f;
    beats[4].confidence = 0.1f;
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(beats, 0.02f).size(), qsizetype(1));
    QCOMPARE(BPMAnalyzer::findUnalignedBeats(beats, 0.02f, 0.5f).size(), qsizetype(0));
}

void BeatDeviationTest::testStatsSummariseDeviations()
{
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(16, interval);
    beats[5].position += qint64(interval * 0.08);
    beats[9].position -= qint64(interval * 0.04);

    const BPMAnalyzer::DeviationStats stats =
        BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate, BPMAnalyzer::DeviationOptions());

    QCOMPARE(stats.beatCount, 16);
    QCOMPARE(stats.gapCount, 0);
    QCOMPARE(stats.duplicateCount, 0);
    QVERIFY(qAbs(stats.maxAbsDeviation - 0.08f) < 0.005f);
    QVERIFY2(stats.medianAbsDeviation < 0.001f, "медиана устойчива к двум выбросам");
    QVERIFY(stats.meanAbsDeviation > 0.0f && stats.meanAbsDeviation < stats.maxAbsDeviation);
    QVERIFY(stats.rmsDeviation > 0.0f && stats.rmsDeviation < stats.maxAbsDeviation);
    QVERIFY(qAbs(stats.gridBPM - bpm) < 0.001f);
}

void BeatDeviationTest::testAdaptiveThreshold()
{
    // Адаптивный порог: трек с естественным джиттером не объявляется неровным целиком
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(64, interval);
    // Добавляем небольшой естественный джиттер ко всем долям
    for (int i = 1; i < beats.size(); ++i) {
        beats[i].position += qint64((i % 3 - 1) * interval * 0.005);  // ±0.5%
    }
    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    // Фиксированный порог строже джиттера — помечает почти весь трек.
    // Порог обязан быть НИЖЕ 0.5%, иначе он не ловит ни одной доли:
    // с 0.01f проверка утверждала обратное тому, что делает поиск.
    const QVector<int> fixed = BPMAnalyzer::findUnalignedBeats(beats, 0.002f);
    QVERIFY2(fixed.size() > beats.size() / 2, "фиксированный порог слишком строг");

    // Адаптивный порог учитывает естественный разброс
    BPMAnalyzer::UnalignedOptions adaptive;
    adaptive.adaptiveThreshold = true;
    adaptive.adaptiveFloor = 0.005f;
    adaptive.adaptiveMultiplier = 2.5f;
    const QVector<int> smart = BPMAnalyzer::findUnalignedBeats(beats, 0.0f, 0.0f, adaptive);
    QVERIFY2(smart.size() < beats.size() / 4, "адаптивный порог устойчив к джиттеру");
}

void BeatDeviationTest::testRegionGrouping()
{
    // Группировка: 3 подряд неровные доли → 1 представитель
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(64, interval);
    beats[20].position += qint64(interval * 0.08);
    beats[21].position += qint64(interval * 0.12);  // наибольшее отклонение
    beats[22].position += qint64(interval * 0.06);
    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    // Без группировки — 3 неровные доли
    const QVector<int> raw = BPMAnalyzer::findUnalignedBeats(beats, 0.02f);
    QCOMPARE(raw.size(), qsizetype(3));

    // С группировкой — 1 представитель (наиболее отклонённая)
    BPMAnalyzer::UnalignedOptions grouped;
    grouped.groupRegions = true;
    grouped.regionGap = 1;
    const QVector<int> regions = BPMAnalyzer::findUnalignedBeats(beats, 0.02f, 0.0f, grouped);
    QCOMPARE(regions.size(), qsizetype(1));
    QCOMPARE(regions.first(), 21);  // доля с максимальным отклонением
}

void BeatDeviationTest::testCorrectionSelection()
{
    // Приоритетный отбор: сортировка по |deviation| × confidence × sqrt(energy)
    const int sampleRate = 44100;
    const float bpm = 120.0f;
    const double interval = (60.0 * sampleRate) / bpm;

    QVector<BPMAnalyzer::BeatInfo> beats = makeGrid(32, interval);
    beats[5].position += qint64(interval * 0.10);
    beats[5].confidence = 0.9f;
    beats[5].energy = 0.5f;  // тихая доля

    beats[15].position += qint64(interval * 0.08);
    beats[15].confidence = 1.0f;
    beats[15].energy = 1.0f;  // громкая доля

    beats[25].position += qint64(interval * 0.06);
    beats[25].confidence = 0.5f;  // низкая уверенность
    beats[25].energy = 1.0f;

    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);

    BPMAnalyzer::UnalignedOptions options;
    options.groupRegions = false;
    const BPMAnalyzer::CorrectionSelection selection =
        BPMAnalyzer::selectBeatsForCorrection(beats, 0.02f, 0.0f, options);

    QVERIFY(selection.indices.size() >= 3);
    // Первая по приоритету — beat[15]: большое отклонение, высокая уверенность, высокая энергия
    QCOMPARE(selection.indices[0], 15);
    // Вторая — beat[5]: наибольшее отклонение компенсирует низкую энергию
    QVERIFY(selection.indices.contains(5));
}

QTEST_MAIN(BeatDeviationTest)
#include "beat_deviation_test.moc"
