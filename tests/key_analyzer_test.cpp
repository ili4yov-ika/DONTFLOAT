#include <QtTest/QTest>
#include <QtCore/QVector>
#include <cmath>

#include "../include/keyanalyzer.h"

// Тесты потактового определения тональности и модуляции (смен тональности),
// как в Melodyne. Используются синтезированные сигналы из чистых синусов:
// у чистого синуса нет гармоник, поэтому в хрому попадают только основные
// тоны — детекция становится детерминированной, не завися от qm-dsp.
class KeyAnalyzerTest : public QObject
{
    Q_OBJECT

public:
    KeyAnalyzerTest() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSamplesPerBar();
    void testChromaSingleTone();
    void testSingleKeyNoModulation();
    void testPerBarModulationDetected();
    void testMergeBarsIntoRegions();
    void testGridStartOffset();
    void testDominantModulationKey();

private:
    static constexpr int kSampleRate = 44100;

    // До-мажорная гамма (C D E F G A B), один тон на каждый класс высоты.
    static QVector<int> cMajorNotes() { return {60, 62, 64, 65, 67, 69, 71}; }
    // Фа#-мажорная гамма (F# G# A# B C# D# E#=F) — тритон от C, максимально далёкая.
    static QVector<int> fSharpMajorNotes() { return {66, 68, 70, 71, 61, 63, 65}; }

    // Дописывает в out `count` сэмплов: сумма синусов заданных MIDI-нот.
    static void appendChord(QVector<float>& out, const QVector<int>& midiNotes,
                            qint64 count)
    {
        const qint64 base = out.size();
        out.resize(base + count);
        constexpr double kTwoPi = 6.28318530717958647692;
        for (qint64 i = 0; i < count; ++i) {
            double s = 0.0;
            for (int m : midiNotes) {
                const double f = 440.0 * std::pow(2.0, (double(m) - 69.0) / 12.0);
                s += std::sin(kTwoPi * f * double(i) / double(kSampleRate));
            }
            out[base + i] = float(s / double(midiNotes.size()) * 0.8);
        }
    }
};

void KeyAnalyzerTest::initTestCase()
{
    qDebug() << "Инициализация тестов потактового анализа тональности";
}

void KeyAnalyzerTest::cleanupTestCase()
{
    qDebug() << "Завершение тестов потактового анализа тональности";
}

void KeyAnalyzerTest::testSamplesPerBar()
{
    qDebug() << "\n=== Тест: samplesPerBar ===";

    // 4/4 при 120 BPM: 4 доли * (60*44100/120) = 4 * 22050 = 88200
    KeyAnalyzer::BarGrid g44;
    g44.bpm = 120.0f;
    g44.beatsPerBar = 4;
    QCOMPARE(qint64(std::llround(KeyAnalyzer::samplesPerBar(g44, kSampleRate))),
             qint64(88200));

    // 3/4 при 120 BPM: 3 четверти * 22050 = 66150
    KeyAnalyzer::BarGrid g34;
    g34.bpm = 120.0f;
    g34.beatsPerBar = 3;
    QCOMPARE(qint64(std::llround(KeyAnalyzer::samplesPerBar(g34, kSampleRate))),
             qint64(66150));

    // 6/8 = 3 четверти на такт → как 3/4 по длине в четвертях
    KeyAnalyzer::BarGrid g68;
    g68.bpm = 120.0f;
    g68.beatsPerBar = 6;
    QCOMPARE(qint64(std::llround(KeyAnalyzer::samplesPerBar(g68, kSampleRate))),
             qint64(66150));

    // Некорректные параметры → 0
    KeyAnalyzer::BarGrid bad;
    bad.bpm = 0.0f;
    QCOMPARE(KeyAnalyzer::samplesPerBar(bad, kSampleRate), 0.0);
    qDebug() << "  ✓ Длина такта в сэмплах вычисляется корректно";
}

void KeyAnalyzerTest::testChromaSingleTone()
{
    qDebug() << "\n=== Тест: computeChromaGoertzel на одиночном тоне A4 ===";

    QVector<float> tone;
    appendChord(tone, {69}, kSampleRate); // 1 секунда чистого A4 (440 Гц)

    const QVector<float> chroma = KeyAnalyzer::computeChromaGoertzel(tone, kSampleRate);
    QCOMPARE(chroma.size(), 12);

    int argmax = 0;
    for (int i = 1; i < 12; ++i) {
        if (chroma[i] > chroma[argmax]) {
            argmax = i;
        }
    }
    // A имеет класс высоты 9 (C=0)
    QCOMPARE(argmax, 9);
    QVERIFY2(chroma[9] > 0.99f, "Пик хромы на A должен быть нормирован к ~1.0");
    qDebug() << "  ✓ Хрома выделяет верный класс высоты";
}

void KeyAnalyzerTest::testSingleKeyNoModulation()
{
    qDebug() << "\n=== Тест: один ключ, без модуляции ===";

    KeyAnalyzer::BarGrid grid;
    grid.bpm = 120.0f;
    grid.beatsPerBar = 4;
    grid.gridStartSample = 0;
    const qint64 spb = qint64(std::llround(KeyAnalyzer::samplesPerBar(grid, kSampleRate)));

    QVector<float> audio;
    appendChord(audio, cMajorNotes(), spb * 3); // 3 такта до-мажора

    const KeyAnalyzer::PerBarKeyResult res =
        KeyAnalyzer::analyzeKeyPerBar(audio, kSampleRate, grid);

    QCOMPARE(res.bars.size(), 3);
    for (const KeyAnalyzer::BarKey& b : res.bars) {
        QCOMPARE(b.key.key, KeyAnalyzer::C_MAJOR);
    }
    QVERIFY2(!res.hasModulation, "Один ключ не должен считаться модуляцией");
    QCOMPARE(res.regions.size(), 1);
    QCOMPARE(res.regions[0].startBar, 0);
    QCOMPARE(res.regions[0].endBar, 2);
    QCOMPARE(res.primaryKey.key, KeyAnalyzer::C_MAJOR);
    qDebug() << "  ✓ Для одной тональности получается один регион без модуляции";
}

void KeyAnalyzerTest::testPerBarModulationDetected()
{
    qDebug() << "\n=== Тест: модуляция C major → F# major по тактам ===";

    KeyAnalyzer::BarGrid grid;
    grid.bpm = 120.0f;
    grid.beatsPerBar = 4;
    grid.gridStartSample = 0;
    const qint64 spb = qint64(std::llround(KeyAnalyzer::samplesPerBar(grid, kSampleRate)));

    QVector<float> audio;
    appendChord(audio, cMajorNotes(), spb * 2);       // такты 0,1 — C major
    appendChord(audio, fSharpMajorNotes(), spb * 2);  // такты 2,3 — F# major

    const KeyAnalyzer::PerBarKeyResult res =
        KeyAnalyzer::analyzeKeyPerBar(audio, kSampleRate, grid);

    QCOMPARE(res.bars.size(), 4);
    QCOMPARE(res.bars[0].key.key, KeyAnalyzer::C_MAJOR);
    QCOMPARE(res.bars[1].key.key, KeyAnalyzer::C_MAJOR);
    QCOMPARE(res.bars[2].key.key, KeyAnalyzer::F_SHARP_MAJOR);
    QCOMPARE(res.bars[3].key.key, KeyAnalyzer::F_SHARP_MAJOR);

    QVERIFY2(res.hasModulation, "Должна быть обнаружена смена тональности");
    QCOMPARE(res.regions.size(), 2);

    QCOMPARE(res.regions[0].key.key, KeyAnalyzer::C_MAJOR);
    QCOMPARE(res.regions[0].startBar, 0);
    QCOMPARE(res.regions[0].endBar, 1);

    QCOMPARE(res.regions[1].key.key, KeyAnalyzer::F_SHARP_MAJOR);
    QCOMPARE(res.regions[1].startBar, 2);
    QCOMPARE(res.regions[1].endBar, 3);

    // Точка модуляции — на границе 2-го такта.
    QCOMPARE(res.regions[1].startSample, qint64(2) * spb);

    // Имена тональностей заполнены (для отображения в UI).
    QCOMPARE(res.regions[0].key.keyName, QStringLiteral("C Major"));
    QCOMPARE(res.regions[1].key.keyName, QStringLiteral("F# Major"));

    qDebug() << "  ✓ Модуляция определена, границы совпадают с тактами";
}

void KeyAnalyzerTest::testMergeBarsIntoRegions()
{
    qDebug() << "\n=== Тест: mergeBarsIntoRegions ===";

    auto mk = [](int idx, KeyAnalyzer::Key k) {
        KeyAnalyzer::BarKey b;
        b.barIndex = idx;
        b.startSample = qint64(idx) * 100;
        b.endSample = qint64(idx + 1) * 100;
        b.key.key = k;
        b.key.keyName = KeyAnalyzer::keyToString(k);
        return b;
    };

    QVector<KeyAnalyzer::BarKey> bars;
    bars << mk(0, KeyAnalyzer::C_MAJOR)
         << mk(1, KeyAnalyzer::C_MAJOR)
         << mk(2, KeyAnalyzer::G_MAJOR)
         << mk(3, KeyAnalyzer::C_MAJOR);

    const QVector<KeyAnalyzer::KeyRegion> regs = KeyAnalyzer::mergeBarsIntoRegions(bars);

    QCOMPARE(regs.size(), 3);
    QCOMPARE(regs[0].key.key, KeyAnalyzer::C_MAJOR);
    QCOMPARE(regs[0].startBar, 0);
    QCOMPARE(regs[0].endBar, 1);
    QCOMPARE(regs[0].endSample, qint64(200));
    QCOMPARE(regs[1].key.key, KeyAnalyzer::G_MAJOR);
    QCOMPARE(regs[1].startBar, 2);
    QCOMPARE(regs[2].key.key, KeyAnalyzer::C_MAJOR);
    QCOMPARE(regs[2].startBar, 3);

    QCOMPARE(KeyAnalyzer::mergeBarsIntoRegions({}).size(), 0);
    qDebug() << "  ✓ Соседние такты корректно объединяются в регионы";
}

void KeyAnalyzerTest::testGridStartOffset()
{
    qDebug() << "\n=== Тест: учёт смещения начала сетки (gridStartSample) ===";

    KeyAnalyzer::BarGrid grid;
    grid.bpm = 120.0f;
    grid.beatsPerBar = 4;
    const qint64 spb = qint64(std::llround(KeyAnalyzer::samplesPerBar(grid, kSampleRate)));
    grid.gridStartSample = spb / 2; // затакт (pickup) в половину такта

    QVector<float> audio;
    // Затакт до начала сетки — не должен учитываться как такт 0.
    appendChord(audio, fSharpMajorNotes(), spb / 2);
    // Затем два полных такта до-мажора от gridStartSample.
    appendChord(audio, cMajorNotes(), spb * 2);

    const KeyAnalyzer::PerBarKeyResult res =
        KeyAnalyzer::analyzeKeyPerBar(audio, kSampleRate, grid);

    QVERIFY(res.bars.size() >= 2);
    QCOMPARE(res.bars[0].startSample, grid.gridStartSample);
    QCOMPARE(res.bars[0].key.key, KeyAnalyzer::C_MAJOR);
    QCOMPARE(res.bars[1].key.key, KeyAnalyzer::C_MAJOR);
    qDebug() << "  ✓ Такт 0 начинается с gridStartSample";
}

void KeyAnalyzerTest::testDominantModulationKey()
{
    qDebug() << "\n=== Тест: dominantModulationKey (тональность для поля модуляции) ===";

    auto mk = [](KeyAnalyzer::Key k) {
        KeyAnalyzer::BarKey b;
        b.key.key = k;
        b.key.keyName = KeyAnalyzer::keyToString(k);
        b.key.strength = 1.0f;
        b.key.confidence = 1.0f;
        return b;
    };

    KeyAnalyzer::PerBarKeyResult perBar;
    perBar.bars << mk(KeyAnalyzer::C_MAJOR) << mk(KeyAnalyzer::C_MAJOR)
                << mk(KeyAnalyzer::C_MAJOR) << mk(KeyAnalyzer::F_SHARP_MAJOR)
                << mk(KeyAnalyzer::F_SHARP_MAJOR) << mk(KeyAnalyzer::G_MAJOR);
    perBar.primaryKey.key = KeyAnalyzer::C_MAJOR;

    // Самая частая тональность, отличная от основной, — F# Major (2 такта).
    const KeyAnalyzer::KeyInfo mod =
        KeyAnalyzer::dominantModulationKey(perBar, KeyAnalyzer::C_MAJOR);
    QCOMPARE(mod.key, KeyAnalyzer::F_SHARP_MAJOR);
    QCOMPARE(mod.keyName, QStringLiteral("F# Major"));

    // Исключается ровно переданная тональность: если исключить не основную (F#),
    // самой частой из оставшихся остаётся C Major (3 такта).
    const KeyAnalyzer::KeyInfo mod2 =
        KeyAnalyzer::dominantModulationKey(perBar, KeyAnalyzer::F_SHARP_MAJOR);
    QCOMPARE(mod2.key, KeyAnalyzer::C_MAJOR);

    // Без иной тональности (все такты — один ключ) → UNKNOWN.
    KeyAnalyzer::PerBarKeyResult single;
    single.bars << mk(KeyAnalyzer::C_MAJOR) << mk(KeyAnalyzer::C_MAJOR);
    single.primaryKey.key = KeyAnalyzer::C_MAJOR;
    const KeyAnalyzer::KeyInfo none =
        KeyAnalyzer::dominantModulationKey(single, KeyAnalyzer::C_MAJOR);
    QCOMPARE(none.key, KeyAnalyzer::UNKNOWN_KEY);

    qDebug() << "  ✓ Тональность модуляции для поля выбирается корректно";
}

QTEST_MAIN(KeyAnalyzerTest)
#include "key_analyzer_test.moc"
