// Метки растяжения на волне: тот же путь, которым их ставит плагин
// (клавиша M → WaveformView::addMarker по позиции каретки).

#include <QtTest/QTest>

#include "../include/waveformview.h"
#include "../include/markerengine.h"
#include "../include/timestretchprocessor.h"
#include "../include/wavwriter.h"

#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <QtCore/QUrl>

#include <cmath>

namespace {

constexpr int kSampleRate = 48000;

QVector<QVector<float>> makeTone(int frames)
{
    QVector<float> mono(frames);
    for (int i = 0; i < frames; ++i) {
        mono[i] = 0.5f * std::sin(6.2831853f * 220.0f * float(i) / float(kSampleRate));
    }
    return { mono, mono };
}

} // namespace

class WaveformMarkerTest : public QObject
{
    Q_OBJECT

private slots:
    void testAddMarkerAtPlayhead();
    void testMarkerTooCloseRejected();
    void testNoMarkersWithoutAudio();
    void testMarkerDragChangesRenderedAudio();
    void testPreviewWavIsPlayable();
    void testTwoMarkersOnOneGridLineKillStretch();
    void testSegmentCacheMatchesFullRender();
    void testMarkersLandOnTheirTargets();
};

// Метка ставится по позиции каретки — так работает клавиша M в приложении и в плагине
void WaveformMarkerTest::testAddMarkerAtPlayhead()
{
    WaveformView view;
    view.setAudioData(makeTone(4 * kSampleRate));
    view.setSampleRate(kSampleRate);

    const qint64 positionMs = 1500;
    view.setPlaybackPosition(positionMs);
    const qint64 sample = (positionMs * kSampleRate) / 1000;
    view.addMarker(sample);

    const QVector<Marker> markers = view.getMarkers();
    QVERIFY(!markers.isEmpty());

    // Среди меток есть поставленная (нулевая и конечная создаются автоматически)
    bool found = false;
    for (const Marker& marker : markers) {
        if (std::llabs(marker.position - sample) <= kSampleRate / 100) {
            found = true;
            break;
        }
    }
    QVERIFY2(found, "метка по позиции каретки не найдена");
}

// Ближе 50 мс к существующей метке новая не создаётся — об этом и пишет статус плагина
void WaveformMarkerTest::testMarkerTooCloseRejected()
{
    WaveformView view;
    view.setAudioData(makeTone(4 * kSampleRate));
    view.setSampleRate(kSampleRate);

    const qint64 sample = 2 * kSampleRate;
    view.addMarker(sample);
    const int afterFirst = view.getMarkers().size();

    view.addMarker(sample + kSampleRate / 100);  // 10 мс — слишком близко
    QCOMPARE(view.getMarkers().size(), afterFirst);

    view.addMarker(sample + kSampleRate / 2);    // полсекунды — нормально
    QVERIFY(view.getMarkers().size() > afterFirst);
}

// Без аудио метки не появляются: в плагине это состояние «дорожка ещё не пришла»
void WaveformMarkerTest::testNoMarkersWithoutAudio()
{
    WaveformView view;
    view.setSampleRate(kSampleRate);
    view.addMarker(1000);
    QVERIFY(view.getMarkers().isEmpty());
}

// Сценарий приложения: «Выровнять» ставит метки коррекции (originalPosition —
// реальная доля, position — линия сетки), затем пользователь тащит метку.
// Звук обязан меняться на каждом шаге, иначе растяжения не слышно.
void WaveformMarkerTest::testMarkerDragChangesRenderedAudio()
{
    const int totalSamples = 8 * kSampleRate;
    const QVector<QVector<float>> source = makeTone(totalSamples);

    QVector<MarkerData> markers;
    const auto addMarker = [&markers](qint64 original, qint64 target) {
        MarkerData marker;
        marker.originalPosition = original;
        marker.position = target;
        markers.append(marker);
    };
    addMarker(0, 0);
    addMarker(2 * kSampleRate + 3000, 2 * kSampleRate);
    addMarker(4 * kSampleRate - 2500, 4 * kSampleRate);
    addMarker(totalSamples - 1, totalSamples - 1);

    QString error;
    QVERIFY2(TimeStretchProcessor::validateMarkers(markers, totalSamples, &error),
             qPrintable(error));

    const TimeStretchProcessor::StretchResult first =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, true);
    QVERIFY(!first.audioData.isEmpty() && !first.audioData[0].isEmpty());
    QVERIFY2(first.audioData[0] != source[0], "растяжение по меткам обязано менять звук");

    // Пользователь тащит вторую метку — рендер обязан выйти другим
    markers[1].position += kSampleRate / 4;
    QVERIFY2(TimeStretchProcessor::validateMarkers(markers, totalSamples, &error),
             qPrintable(error));

    const TimeStretchProcessor::StretchResult second =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, true);
    QVERIFY(!second.audioData.isEmpty() && !second.audioData[0].isEmpty());
    QVERIFY2(second.audioData[0] != first.audioData[0],
             "сдвиг метки обязан менять результат рендера");
}

// Превью растяжения уходит в плеер временным WAV. Формат пишется Float32,
// и если проигрыватель платформы его не принимает, пользователь видит новую
// волну, но слышит старый звук — сломанного звена при этом никто не замечает.
void WaveformMarkerTest::testPreviewWavIsPlayable()
{
    const QVector<QVector<float>> audio = makeTone(kSampleRate);
    const QString path = WavWriter::writeTempProcessedFile(audio, kSampleRate);
    QVERIFY2(!path.isEmpty(), "временный WAV превью не записался");

    QMediaPlayer player;
    QAudioOutput output;
    output.setVolume(0.0f);
    player.setAudioOutput(&output);
    player.setSource(QUrl::fromLocalFile(path));

    QTRY_VERIFY_WITH_TIMEOUT(player.mediaStatus() == QMediaPlayer::LoadedMedia
                                 || player.mediaStatus() == QMediaPlayer::BufferedMedia
                                 || player.mediaStatus() == QMediaPlayer::InvalidMedia,
                             15000);

    QVERIFY2(player.mediaStatus() != QMediaPlayer::InvalidMedia,
             qPrintable(QStringLiteral("плеер не принял превью-WAV: %1").arg(player.errorString())));
    QVERIFY2(player.duration() > 0, "плеер не увидел длительность превью-WAV");
}

// Две доли садятся на одну линию сетки штатно (duplicateCount у DeviationStats).
// Если обе попадут в метки, целевой сегмент между ними нулевой — и растяжение
// не применяется НИ ПО ОДНОЙ метке: набор целиком отвергается валидацией, а
// applyMarkerStretch молча отдаёт исходный звук. На волне при этом всё
// выглядит правильно, потому что предпросмотр рисуется прямо по меткам.
// Отсюда требование к createDeviationMarkers: таких пар не создавать.
void WaveformMarkerTest::testTwoMarkersOnOneGridLineKillStretch()
{
    const int totalSamples = 8 * kSampleRate;
    const QVector<QVector<float>> source = makeTone(totalSamples);

    QVector<MarkerData> markers;
    const auto addMarker = [&markers](qint64 original, qint64 target) {
        MarkerData marker;
        marker.originalPosition = original;
        marker.position = target;
        markers.append(marker);
    };
    addMarker(0, 0);
    addMarker(2 * kSampleRate + 3000, 2 * kSampleRate);
    // Следующая доля села на ту же линию сетки: цель совпала с предыдущей
    addMarker(2 * kSampleRate + 9000, 2 * kSampleRate);
    addMarker(totalSamples - 1, totalSamples - 1);

    QString error;
    QVERIFY2(!TimeStretchProcessor::validateMarkers(markers, totalSamples, &error),
             "нулевой сегмент обязан отвергаться валидацией");

    const TimeStretchProcessor::StretchResult result =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, true);
    QVERIFY(!result.audioData.isEmpty());
    // Вот он, режим отказа: звук возвращается нетронутым, без единого признака
    QCOMPARE(result.audioData[0], source[0]);
}

namespace {

/** Дорожка 120 BPM с метками коррекции на каждой доле. */
QVector<MarkerData> makeBeatMarkers(qint64 totalSamples, int sampleRate)
{
    QVector<MarkerData> markers;
    const qint64 beat = sampleRate / 2;
    for (qint64 pos = 0; pos + beat < totalSamples; pos += beat) {
        MarkerData m;
        m.originalPosition = pos;
        m.position = pos + (pos / beat % 2 == 0 ? 700 : -700);
        if (m.position < 0) {
            m.position = 0;
        }
        markers.append(m);
    }
    if (!markers.isEmpty()) {
        markers.first().position = 0;
        markers.first().originalPosition = 0;
    }
    return markers;
}

} // namespace

// Кэш сегментов обязан давать ровно тот же звук, что и счёт с нуля, и при
// сдвиге одной метки пересчитывать только её окрестность, а не всю дорожку.
void WaveformMarkerTest::testSegmentCacheMatchesFullRender()
{
    const int totalSamples = 8 * kSampleRate;
    const QVector<QVector<float>> source = makeTone(totalSamples);
    QVector<MarkerData> markers = makeBeatMarkers(totalSamples, kSampleRate);
    QVERIFY(markers.size() >= 4);

    TimeStretchProcessor::SegmentCache cache;

    const TimeStretchProcessor::StretchResult plain =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, false);
    const TimeStretchProcessor::StretchResult cachedCold =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, false, &cache);

    QVERIFY(!plain.audioData.isEmpty() && !cachedCold.audioData.isEmpty());
    QCOMPARE(cachedCold.audioData[0], plain.audioData[0]);
    QCOMPARE(cache.hitCount(), 0);
    const int coldMisses = cache.missCount();
    QVERIFY2(coldMisses > 0, "холодный проход обязан что-то посчитать");

    // Тот же набор меток — всё берётся готовым
    const TimeStretchProcessor::StretchResult cachedWarm =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, false, &cache);
    QCOMPARE(cachedWarm.audioData[0], plain.audioData[0]);
    QCOMPARE(cache.missCount(), coldMisses);
    QVERIFY2(cache.hitCount() > 0, "повторный проход обязан попадать в кэш");

    // Двигаем одну метку: меняются её сегменты, остальные обязаны остаться
    const int moved = markers.size() / 2;
    markers[moved].position += kSampleRate / 8;

    const TimeStretchProcessor::StretchResult plainMoved =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, false);
    const int missesBefore = cache.missCount();
    const TimeStretchProcessor::StretchResult cachedMoved =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, false, &cache);

    QCOMPARE(cachedMoved.audioData[0], plainMoved.audioData[0]);

    const int recomputed = cache.missCount() - missesBefore;
    QVERIFY2(recomputed <= 3,
             qPrintable(QStringLiteral("пересчитано %1 сегментов из %2 — сдвиг одной метки "
                                       "не должен трогать всю дорожку")
                            .arg(recomputed).arg(markers.size())));
    QVERIFY2(recomputed > 0, "сдвинутая метка обязана вызвать пересчёт");
}

// Метки обязаны приходить ровно туда, куда их поставили.
//
// Ради этого коэффициент каждого сегмента подгоняется под фактически собранную
// длину: иначе кроссфейд съедает свои ~10 мс на каждом стыке, ошибка копится, и
// к концу дорожки метки уезжают. Сегменты считаются заранее и параллельно, по
// предсказанной длине, — эта проверка сторожит, что предсказание не разъезжается
// с фактом.
void WaveformMarkerTest::testMarkersLandOnTheirTargets()
{
    const int totalSamples = 20 * kSampleRate;
    const QVector<QVector<float>> source = makeTone(totalSamples);

    // Полсекунды на долю, каждая вторая уехала — как метки коррекции
    QVector<MarkerData> markers;
    const qint64 beat = kSampleRate / 2;
    for (qint64 pos = 0; pos + beat < totalSamples; pos += beat) {
        MarkerData m;
        m.originalPosition = pos;
        m.position = pos + (pos / beat % 2 == 0 ? 900 : -900);
        if (m.position < 0) {
            m.position = 0;
        }
        markers.append(m);
    }
    markers.first().position = 0;
    markers.first().originalPosition = 0;
    QVERIFY(markers.size() > 8);

    const TimeStretchProcessor::StretchResult result =
        TimeStretchProcessor::applyMarkerStretch(source, markers, kSampleRate, true);
    QVERIFY(!result.audioData.isEmpty() && !result.audioData[0].isEmpty());
    QCOMPARE(result.newMarkers.size(), markers.size());

    // Допуск в две миллисекунды: кроссфейд и округления Rubber Band дают свои
    // единицы сэмплов, а вот накопление ошибки к концу дорожки — уже дефект
    const qint64 tolerance = (2 * kSampleRate) / 1000;
    for (int i = 1; i < result.newMarkers.size(); ++i) {
        const qint64 landed = result.newMarkers[i].position;
        const qint64 wanted = markers[i].position;
        QVERIFY2(qAbs(landed - wanted) <= tolerance,
                 qPrintable(QStringLiteral("метка %1 из %2 встала на %3 вместо %4 "
                                           "(промах %5 сэмплов)")
                                .arg(i).arg(result.newMarkers.size())
                                .arg(landed).arg(wanted).arg(landed - wanted)));
    }
}

QTEST_MAIN(WaveformMarkerTest)
#include "waveform_marker_test.moc"
