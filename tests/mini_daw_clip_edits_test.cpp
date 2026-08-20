// Правки клипов в DAW глазами плагина: рез, обрезка краёв, сжатие и растяжение.
//
// Мини-DAW умеет то же, что настоящая DAW: резать клип, тянуть его края и
// менять длительность. Плагин ничего этого не знает — он видит только поток
// блоков в process() с позициями на таймлайне. Тест проходит весь путь: правит
// клипы **тем же кодом, что и окно мини-DAW** (MiniDaw::splitClipAt и соседи),
// собирает дорожку, прогоняет её через сессию плагина и смотрит, что у плагина
// в захвате.
//
// Перенос клипа проверяет plugin_content_shift_test; здесь — остальные правки.

#include <QtTest/QTest>

#include "../plugins/core/dontfloat_plugin_core.h"
#include "../tools/mini_daw/mini_daw_clip_model.h"

#include <algorithm>
#include <cmath>
#include <vector>

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;

namespace {

constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 512;
constexpr qint64 kClipFrames = 24000;  // 0.5 с исходного материала

/**
 * Исходник-«линейка»: значение равно номеру кадра, поделённому на длину.
 * По любому куску сразу видно, откуда он взят, — это и проверяем после реза
 * и обрезки краёв.
 */
QVector<float> makeRuler(qint64 frames)
{
    QVector<float> out(static_cast<int>(frames));
    for (qint64 i = 0; i < frames; ++i) {
        out[int(i)] = float(double(i) / double(frames));
    }
    return out;
}

/** Обратно: по значению — номер кадра в исходнике. */
qint64 sourceFrameOf(float value, qint64 frames)
{
    return qint64(std::llround(double(value) * double(frames)));
}

/** Отдаёт дорожку плагину блоками, как это делает DAW, и разбирает захват. */
void streamToPlugin(TrackToolSession& session, const QVector<float>& left,
                    const QVector<float>& right)
{
    const int frames = int(std::min(left.size(), right.size()));
    for (int pos = 0; pos < frames; pos += kBlockSize) {
        const int n = std::min(kBlockSize, frames - pos);
        const float* channels[2] = { left.constData() + pos, right.constData() + pos };
        session.writeHostFrames(channels, 2, n, pos);
        session.drainHostCapture();
    }
}

TrackToolSession& prepare(TrackToolSession& session)
{
    TrackAudioInfo info;
    info.sampleRate = kSampleRate;
    info.channelCount = 2;
    info.frameCount = kBlockSize;
    session.prepare(info);
    return session;
}

/** Один клип на весь материал, стоящий в начале дорожки. */
QVector<MiniDaw::Clip> wholeClip(qint64 frames)
{
    MiniDaw::Clip clip;
    clip.timelineStart = 0;
    clip.sourceStart = 0;
    clip.sourceLength = frames;
    return { clip };
}

} // namespace

class MiniDawClipEditsTest : public QObject
{
    Q_OBJECT

private slots:
    void testAddedClipAppearsOnTimeline();
    void testAddedClipKeepsExistingMaterial();
    void testSplitKeepsMaterialContiguous();
    void testSplitTooCloseToEdgeIsRejected();
    void testSplitHalvesCanBeMovedApart();
    void testTrimStartDropsHeadAndShiftsClip();
    void testTrimEndDropsTail();
    void testTrimStopsAtSourceBounds();
    void testStretchMakesClipLongerAndKeepsMaterial();
    void testCompressMakesClipShorter();
    void testStretchIsClampedToLimits();
    void testPluginSeesEditedTimeline();
};

// На дорожку добавили второй клип — он появляется на своём месте
void MiniDawClipEditsTest::testAddedClipAppearsOnTimeline()
{
    const QVector<float> source = makeRuler(kClipFrames);
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);

    // Новый клип из того же материала, но дальше по дорожке и с разрывом
    const qint64 gap = 8000;
    MiniDaw::Clip added;
    added.timelineStart = kClipFrames + gap;
    added.sourceStart = 0;
    added.sourceLength = kClipFrames;
    clips.append(added);

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);
    QCOMPARE(qint64(left.size()), kClipFrames * 2 + gap);

    // Между клипами тишина, а новый начинается с начала материала
    QCOMPARE(left[int(kClipFrames + gap / 2)], 0.0f);
    QCOMPARE(left[int(kClipFrames + gap)], source[0]);

    // Клип под новой позицией действительно находится
    QCOMPARE(MiniDaw::clipAt(clips, kClipFrames + gap + 100), 1);
    QCOMPARE(MiniDaw::clipAt(clips, kClipFrames + gap / 2), -1);  // в разрыве пусто

    // И плагин видит обе части
    TrackToolSession session;
    prepare(session);
    streamToPlugin(session, left, right);
    const auto& captured = session.audioBuffer();
    QCOMPARE(qint64(captured.left.size()), qint64(left.size()));
    QCOMPARE(captured.left[int(kClipFrames + gap / 2)], 0.0f);
    QVERIFY(std::fabs(captured.left[int(kClipFrames + gap)] - source[0]) < 1e-5f);
}

// Добавление клипа не трогает то, что уже лежало на дорожке
void MiniDawClipEditsTest::testAddedClipKeepsExistingMaterial()
{
    const QVector<float> source = makeRuler(kClipFrames);

    QVector<MiniDaw::Clip> before = wholeClip(kClipFrames);
    QVector<float> leftBefore, rightBefore;
    MiniDaw::renderTimeline(before, source, source, leftBefore, rightBefore);

    QVector<MiniDaw::Clip> after = before;
    MiniDaw::Clip added;
    added.timelineStart = kClipFrames * 2;
    added.sourceStart = 0;
    added.sourceLength = kClipFrames;
    after.append(added);

    QVector<float> leftAfter, rightAfter;
    MiniDaw::renderTimeline(after, source, source, leftAfter, rightAfter);

    // Первый клип остался сэмпл в сэмпл там же, где был
    QVERIFY(leftAfter.size() > leftBefore.size());
    for (int i = 0; i < leftBefore.size(); i += 53) {
        QVERIFY2(std::fabs(leftAfter[i] - leftBefore[i]) < 1e-6f,
                 qPrintable(QStringLiteral("кадр %1 уехал после добавления клипа").arg(i)));
    }
}

// Рез не теряет и не дублирует материал: половинки стыкуются встык
void MiniDawClipEditsTest::testSplitKeepsMaterialContiguous()
{
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);
    const qint64 cut = 10000;

    const int tail = MiniDaw::splitClipAt(clips, cut);
    QCOMPARE(tail, 1);
    QCOMPARE(clips.size(), 2);

    // Левая половина заканчивается там, где начинается правая
    QCOMPARE(clips[0].timelineStart, qint64(0));
    QCOMPARE(clips[0].sourceLength, cut);
    QCOMPARE(clips[1].timelineStart, cut);
    QCOMPARE(clips[1].sourceStart, cut);
    QCOMPARE(clips[0].sourceLength + clips[1].sourceLength, kClipFrames);
    QCOMPARE(clips[0].timelineEnd(), clips[1].timelineStart);
}

// Рез у самого края дал бы огрызок — такую правку отклоняем
void MiniDawClipEditsTest::testSplitTooCloseToEdgeIsRejected()
{
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);

    QCOMPARE(MiniDaw::splitClipAt(clips, MiniDaw::kMinClipFrames / 2), -1);
    QCOMPARE(MiniDaw::splitClipAt(clips, kClipFrames - MiniDaw::kMinClipFrames / 2), -1);
    QCOMPARE(clips.size(), 1);  // клип не тронут

    QCOMPARE(MiniDaw::splitClipAt(clips, kClipFrames + 1000), -1);  // мимо клипа
    QCOMPARE(clips.size(), 1);
}

// Половинки после реза — независимые клипы: одну можно отодвинуть
void MiniDawClipEditsTest::testSplitHalvesCanBeMovedApart()
{
    const QVector<float> source = makeRuler(kClipFrames);
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);
    const qint64 cut = 12000;
    const int tail = MiniDaw::splitClipAt(clips, cut);
    QVERIFY(tail > 0);

    const qint64 gap = 5000;
    QVERIFY(MiniDaw::moveClip(clips, tail, gap));

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);
    QCOMPARE(qint64(left.size()), kClipFrames + gap);

    // В разрыве между половинками — тишина
    QCOMPARE(left[int(cut + gap / 2)], 0.0f);
    // А правая половина начинается тем же материалом, что и до переноса
    const qint64 atNewStart = sourceFrameOf(left[int(cut + gap)], kClipFrames);
    QVERIFY2(std::llabs(atNewStart - cut) <= 1,
             qPrintable(QStringLiteral("после переноса материал начался с %1, ждали %2")
                            .arg(atNewStart).arg(cut)));
}

// Обрезка слева выбрасывает начало и двигает клип по дорожке
void MiniDawClipEditsTest::testTrimStartDropsHeadAndShiftsClip()
{
    const QVector<float> source = makeRuler(kClipFrames);
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);
    const qint64 cutOff = 4000;

    QVERIFY(MiniDaw::trimClip(clips, 0, /*startEdge=*/true, cutOff, kClipFrames));
    QCOMPARE(clips[0].sourceStart, cutOff);
    QCOMPARE(clips[0].sourceLength, kClipFrames - cutOff);
    QCOMPARE(clips[0].timelineStart, cutOff);

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);

    // До клипа тишина, а с его начала идёт материал уже не с нуля
    QCOMPARE(left[int(cutOff / 2)], 0.0f);
    const qint64 first = sourceFrameOf(left[int(cutOff)], kClipFrames);
    QVERIFY2(std::llabs(first - cutOff) <= 1,
             qPrintable(QStringLiteral("клип начался с кадра %1, ждали %2").arg(first).arg(cutOff)));
}

// Обрезка справа укорачивает клип, не трогая его начало
void MiniDawClipEditsTest::testTrimEndDropsTail()
{
    const QVector<float> source = makeRuler(kClipFrames);
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);
    const qint64 cutOff = -6000;

    QVERIFY(MiniDaw::trimClip(clips, 0, /*startEdge=*/false, cutOff, kClipFrames));
    QCOMPARE(clips[0].timelineStart, qint64(0));
    QCOMPARE(clips[0].sourceStart, qint64(0));
    QCOMPARE(clips[0].sourceLength, kClipFrames + cutOff);

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);
    QCOMPARE(qint64(left.size()), kClipFrames + cutOff);
    QCOMPARE(left[0], source[0]);
}

// Края не уезжают за пределы исходника и не схлопывают клип в точку
void MiniDawClipEditsTest::testTrimStopsAtSourceBounds()
{
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);

    // Слева дальше материала не уйти
    QVERIFY(!MiniDaw::trimClip(clips, 0, true, -1000, kClipFrames));
    QCOMPARE(clips[0].sourceStart, qint64(0));

    // Справа дальше конца исходника тоже
    QVERIFY(!MiniDaw::trimClip(clips, 0, false, 1000, kClipFrames));
    QCOMPARE(clips[0].sourceLength, kClipFrames);

    // Слишком большая обрезка не отклоняется, а упирается в минимальную длину:
    // край едет ровно настолько, насколько можно
    QVERIFY(MiniDaw::trimClip(clips, 0, true, kClipFrames, kClipFrames));
    QCOMPARE(clips[0].sourceLength, MiniDaw::kMinClipFrames);
    // А дальше двигать уже некуда
    QVERIFY(!MiniDaw::trimClip(clips, 0, true, kClipFrames, kClipFrames));
}

// Растяжение удлиняет клип на дорожке, материал остаётся тем же
void MiniDawClipEditsTest::testStretchMakesClipLongerAndKeepsMaterial()
{
    const QVector<float> source = makeRuler(kClipFrames);
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);

    QVERIFY(MiniDaw::stretchClip(clips, 0, 2.0));
    QCOMPARE(clips[0].sourceLength, kClipFrames);       // материала столько же
    QCOMPARE(clips[0].timelineLength(), kClipFrames * 2);  // а времени вдвое больше

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);
    QCOMPARE(qint64(left.size()), kClipFrames * 2);

    // Середина растянутого клипа — середина исходного материала
    const qint64 mid = sourceFrameOf(left[int(kClipFrames)], kClipFrames);
    QVERIFY2(std::llabs(mid - kClipFrames / 2) <= 2,
             qPrintable(QStringLiteral("в середине оказался кадр %1, ждали %2")
                            .arg(mid).arg(kClipFrames / 2)));
}

// Сжатие — то же в обратную сторону
void MiniDawClipEditsTest::testCompressMakesClipShorter()
{
    const QVector<float> source = makeRuler(kClipFrames);
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);

    QVERIFY(MiniDaw::stretchClip(clips, 0, 0.5));
    QCOMPARE(clips[0].timelineLength(), kClipFrames / 2);

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);
    QCOMPARE(qint64(left.size()), kClipFrames / 2);

    // Конец сжатого клипа — конец исходного материала
    const qint64 last = sourceFrameOf(left[int(kClipFrames / 2) - 1], kClipFrames);
    QVERIFY2(last > kClipFrames - 10,
             qPrintable(QStringLiteral("в конце оказался кадр %1").arg(last)));
}

// Коэффициент зажат: клип нельзя растянуть или сжать до неузнаваемости
void MiniDawClipEditsTest::testStretchIsClampedToLimits()
{
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);

    for (int i = 0; i < 20; ++i) {
        MiniDaw::stretchClip(clips, 0, 2.0);
    }
    QCOMPARE(clips[0].stretch, MiniDaw::kMaxStretch);
    QVERIFY(!MiniDaw::stretchClip(clips, 0, 2.0));  // дальше некуда

    for (int i = 0; i < 40; ++i) {
        MiniDaw::stretchClip(clips, 0, 0.5);
    }
    QCOMPARE(clips[0].stretch, MiniDaw::kMinStretch);
    QVERIFY(!MiniDaw::stretchClip(clips, 0, 0.5));
}

// Главное: после правок плагин получает именно ту дорожку, что собрала DAW
void MiniDawClipEditsTest::testPluginSeesEditedTimeline()
{
    const QVector<float> source = makeRuler(kClipFrames);

    // Режем пополам, правую половину отодвигаем и растягиваем — типичный набор
    QVector<MiniDaw::Clip> clips = wholeClip(kClipFrames);
    const qint64 cut = 12000;
    const int tail = MiniDaw::splitClipAt(clips, cut);
    QVERIFY(tail > 0);
    QVERIFY(MiniDaw::moveClip(clips, tail, 3000));
    QVERIFY(MiniDaw::stretchClip(clips, tail, 2.0));

    QVector<float> left, right;
    MiniDaw::renderTimeline(clips, source, source, left, right);
    QVERIFY(!left.isEmpty());

    TrackToolSession session;
    prepare(session);
    streamToPlugin(session, left, right);

    const auto& captured = session.audioBuffer();
    QCOMPARE(captured.sampleRate, kSampleRate);
    QCOMPARE(qint64(captured.left.size()), qint64(left.size()));

    // Плагин видит ровно ту дорожку, что собрала DAW, — сэмпл в сэмпл
    for (int i = 0; i < left.size(); i += 97) {
        QVERIFY2(std::fabs(captured.left[i] - left[i]) < 1e-5f,
                 qPrintable(QStringLiteral("кадр %1: у плагина %2, на дорожке %3")
                                .arg(i).arg(captured.left[i]).arg(left[i])));
    }

    // И тишину в разрыве между половинками тоже
    QCOMPARE(captured.left[int(cut + 1500)], 0.0f);
}

QTEST_MAIN(MiniDawClipEditsTest)
#include "mini_daw_clip_edits_test.moc"
