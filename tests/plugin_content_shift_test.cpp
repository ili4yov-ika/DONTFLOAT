// Захват дорожки из DAW: запись по позиции таймлайна и распознавание
// перемещения клипа (по нему плагин двигает метки растяжения и ноты).

#include <QtTest/QTest>

#include "../plugins/core/dontfloat_plugin_core.h"

#include <cmath>
#include <vector>

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::computeContentFingerprint;
using Dontfloat::PluginCore::detectContentShift;

namespace {

constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 512;

/** Короткий «клип»: затухающая синусоида, чтобы отпечаток был содержательным. */
std::vector<float> makeClip(int frames)
{
    std::vector<float> clip(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const float t = float(i) / float(kSampleRate);
        clip[static_cast<std::size_t>(i)] =
            0.6f * std::sin(6.2831853f * 220.0f * t) * std::exp(-3.0f * t);
    }
    return clip;
}

/** Прогон клипа через сессию блоками, начиная с позиции \a timelineStart. */
void feedClip(TrackToolSession& session, const std::vector<float>& clip, qint64 timelineStart)
{
    for (std::size_t pos = 0; pos < clip.size(); pos += kBlockSize) {
        const int n = int(std::min<std::size_t>(kBlockSize, clip.size() - pos));
        const float* channels[2] = { clip.data() + pos, clip.data() + pos };
        session.writeHostFrames(channels, 2, n, timelineStart + qint64(pos));
    }
}

TrackToolSession makePreparedSession()
{
    TrackToolSession session;
    TrackAudioInfo info;
    info.sampleRate = kSampleRate;
    info.channelCount = 2;
    info.frameCount = kBlockSize;
    session.prepare(info);
    return session;
}

} // namespace

class PluginContentShiftTest : public QObject
{
    Q_OBJECT

private slots:
    void testWriteAtTimelinePosition();
    void testMovedClipIsDetectedAsShift();
    void testMovedClipInSameSession();
    void testDifferentContentIsNotAShift();
    void testRenderedOutputReplacesInput();
    void testRenderedOutputOnlyOnItsRange();
};

// Обработанный звук уходит в выход плагина: DAW слышит правки, а не исходник
void PluginContentShiftTest::testRenderedOutputReplacesInput()
{
    TrackToolSession session = makePreparedSession();
    const std::vector<float> clip = makeClip(kSampleRate / 4);
    feedClip(session, clip, 0);

    // Результат: тот же материал, но вдвое тише — так видно подмену
    Dontfloat::PluginCore::TrackAudioBuffer rendered;
    rendered.sampleRate = kSampleRate;
    rendered.channelCount = 1;
    rendered.mono = clip;
    for (float& sample : rendered.mono) {
        sample *= 0.5f;
    }
    session.setRenderedOutput(rendered, 0);
    QVERIFY(session.hasRenderedOutput());

    // Блок в середине результата: выход должен стать «тише вдвое»
    const int frames = 128;
    const qint64 position = kSampleRate / 8;
    std::vector<float> left(frames, 1.0f);
    std::vector<float> right(frames, 1.0f);
    float* outputs[2] = { left.data(), right.data() };
    QVERIFY(session.readRenderedOutput(outputs, 2, frames, position));

    for (int i = 0; i < frames; ++i) {
        const float expected = clip[static_cast<std::size_t>(position + i)] * 0.5f;
        QVERIFY(std::fabs(left[static_cast<std::size_t>(i)] - expected) < 1.0e-6f);
        // Моно-результат уходит в оба канала
        QCOMPARE(right[static_cast<std::size_t>(i)], left[static_cast<std::size_t>(i)]);
    }
}

// Вне диапазона результата выход не трогаем — играет то, что дала DAW
void PluginContentShiftTest::testRenderedOutputOnlyOnItsRange()
{
    TrackToolSession session = makePreparedSession();
    Dontfloat::PluginCore::TrackAudioBuffer rendered;
    rendered.sampleRate = kSampleRate;
    rendered.channelCount = 1;
    rendered.mono = makeClip(kSampleRate / 4);
    // Результат лежит со второй секунды дорожки
    session.setRenderedOutput(rendered, 2 * kSampleRate);

    const int frames = 64;
    std::vector<float> left(frames, 1.0f);
    float* outputs[1] = { left.data() };

    QVERIFY(!session.readRenderedOutput(outputs, 1, frames, 0));
    QCOMPARE(left[0], 1.0f);  // выход остался нетронутым

    QVERIFY(session.readRenderedOutput(outputs, 1, frames, 2 * kSampleRate));
    QVERIFY(std::fabs(left[0] - rendered.mono[0]) < 1.0e-6f);

    session.clearRenderedOutput();
    QVERIFY(!session.hasRenderedOutput());
    QVERIFY(!session.readRenderedOutput(outputs, 1, frames, 2 * kSampleRate));
}

// Как в жизни: та же сессия, второй проход DAW с клипом на новой позиции
void PluginContentShiftTest::testMovedClipInSameSession()
{
    TrackToolSession session = makePreparedSession();
    const std::vector<float> clip = makeClip(kSampleRate / 2);

    feedClip(session, clip, kSampleRate);
    const auto before = computeContentFingerprint(session.audioBuffer());

    // Клип переехал — DAW прогоняет дорожку заново с новой позиции
    feedClip(session, clip, 3 * kSampleRate);
    const auto after = computeContentFingerprint(session.audioBuffer());

    QCOMPARE(after.lengthFrames, before.lengthFrames);
    QCOMPARE(after.hash, before.hash);

    qint64 delta = 0;
    QVERIFY(detectContentShift(before, after, &delta));
    QVERIFY(std::llabs(delta - 2 * kSampleRate) < kBlockSize);
}

// Блоки ложатся по позиции таймлайна: до клипа — тишина
void PluginContentShiftTest::testWriteAtTimelinePosition()
{
    TrackToolSession session = makePreparedSession();
    const std::vector<float> clip = makeClip(kSampleRate / 2);  // 0.5 с
    const qint64 offset = kSampleRate;                          // клип начинается с 1 с

    feedClip(session, clip, offset);

    const auto& buffer = session.audioBuffer();
    QCOMPARE(qint64(buffer.mono.size()), offset + qint64(clip.size()));
    QCOMPARE(buffer.mono[0], 0.0f);
    QCOMPARE(buffer.mono[static_cast<std::size_t>(offset) - 1], 0.0f);

    const auto print = computeContentFingerprint(buffer);
    QVERIFY(!print.empty());
    QVERIFY(std::llabs(print.startFrame - offset) < kBlockSize);
}

// Тот же материал на новой позиции — это перемещение клипа, а не новый трек
void PluginContentShiftTest::testMovedClipIsDetectedAsShift()
{
    const std::vector<float> clip = makeClip(kSampleRate / 2);

    TrackToolSession first = makePreparedSession();
    feedClip(first, clip, kSampleRate);
    const auto before = computeContentFingerprint(first.audioBuffer());

    // Новый проход DAW с другой позицией клипа: захват начинается заново
    TrackToolSession second = makePreparedSession();
    feedClip(second, clip, 3 * kSampleRate);
    const auto after = computeContentFingerprint(second.audioBuffer());

    QCOMPARE(before.hash, after.hash);
    QCOMPARE(before.lengthFrames, after.lengthFrames);

    qint64 delta = 0;
    QVERIFY(detectContentShift(before, after, &delta));
    QVERIFY(std::llabs(delta - 2 * kSampleRate) < kBlockSize);

    // Сдвига нет — тот же захват не считается перемещением
    QVERIFY(!detectContentShift(before, before, &delta));
}

// Другой материал сдвигом не считается: нужен полный анализ
void PluginContentShiftTest::testDifferentContentIsNotAShift()
{
    TrackToolSession first = makePreparedSession();
    feedClip(first, makeClip(kSampleRate / 2), kSampleRate);
    const auto before = computeContentFingerprint(first.audioBuffer());

    TrackToolSession second = makePreparedSession();
    std::vector<float> other = makeClip(kSampleRate / 2);
    for (float& sample : other) {
        sample = -sample * 0.5f;  // тот же размер, другое содержимое
    }
    feedClip(second, other, kSampleRate);
    const auto after = computeContentFingerprint(second.audioBuffer());

    qint64 delta = 0;
    QVERIFY(!detectContentShift(before, after, &delta));
}

QTEST_MAIN(PluginContentShiftTest)
#include "plugin_content_shift_test.moc"
