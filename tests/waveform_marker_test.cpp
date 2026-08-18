// Метки растяжения на волне: тот же путь, которым их ставит плагин
// (клавиша M → WaveformView::addMarker по позиции каретки).

#include <QtTest/QTest>

#include "../include/waveformview.h"
#include "../include/markerengine.h"

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

QTEST_MAIN(WaveformMarkerTest)
#include "waveform_marker_test.moc"
