#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>

#include "../include/waveformview.h"
#include "../include/bpmanalyzer.h"
#include "../include/audiofileservice.h"
#include "../include/wavwriter.h"
#include "../include/timeutils.h"

namespace {

QString testDataPath(const QString& filename)
{
    const QString rel = QStringLiteral("tests/source4test/%1").arg(filename);
    if (QFileInfo::exists(rel))
        return QFileInfo(rel).absoluteFilePath();

    QDir dir(QDir::current());
    QString abs = dir.absoluteFilePath(rel);
    if (QFileInfo::exists(abs))
        return abs;

    dir.cdUp();
    abs = dir.absoluteFilePath(rel);
    if (QFileInfo::exists(abs))
        return abs;

    return {};
}

bool shouldSkipIntegration()
{
    if (qEnvironmentVariableIsSet("DONTFLOAT_RUN_UI_TEST"))
        return false;
    return qEnvironmentVariableIsSet("CI") || qEnvironmentVariableIsSet("GITHUB_ACTIONS");
}

int envIntMs(const char* name, int defaultMs)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok && value > 0 ? value : defaultMs;
}

QVector<float> toMono(const QVector<QVector<float>>& channels)
{
    if (channels.isEmpty())
        return {};

    const int length = channels[0].size();
    QVector<float> mono;
    mono.reserve(length);
    for (int i = 0; i < length; ++i) {
        const float left = channels[0][i];
        const float right = (channels.size() > 1) ? channels[1][i] : left;
        mono.append((left + right) * 0.5f);
    }
    return mono;
}

// Аналог MainWindow::createDeviationMarkers (без statusBar).
int createCorrectionMarkers(WaveformView* view, float tolerancePercent, bool neutralMarkers)
{
    if (!view)
        return 0;

    QVector<BPMAnalyzer::BeatInfo> beats = view->getBeatInfo();
    if (beats.isEmpty())
        return 0;

    const float bpm = view->getBPM();
    const int sampleRate = view->getSampleRate();
    if (bpm <= 0 || sampleRate <= 0)
        return 0;

    BPMAnalyzer::calculateDeviations(beats, bpm, sampleRate);
    const float deviationThreshold = tolerancePercent / 100.0f;
    const QVector<int> unalignedIndices = BPMAnalyzer::findUnalignedBeats(beats, deviationThreshold);
    if (unalignedIndices.isEmpty())
        return 0;

    int markersCreated = 0;
    QSet<qint64> addedPositions;

    for (int idx : unalignedIndices) {
        if (idx == 0)
            continue;

        const BPMAnalyzer::BeatInfo& prevBeat = beats[idx - 1];
        const BPMAnalyzer::BeatInfo& currentBeat = beats[idx];

        if (!addedPositions.contains(prevBeat.position)) {
            Marker startMarker(prevBeat.position, sampleRate);
            startMarker.originalPosition = neutralMarkers ? prevBeat.position : prevBeat.expectedPosition;
            startMarker.originalTimeMs = TimeUtils::samplesToMs(startMarker.originalPosition, sampleRate);
            view->addMarker(startMarker);
            addedPositions.insert(prevBeat.position);
            ++markersCreated;
        }
        if (!addedPositions.contains(currentBeat.position)) {
            Marker endMarker(currentBeat.position, sampleRate);
            endMarker.originalPosition = neutralMarkers ? currentBeat.position : currentBeat.expectedPosition;
            endMarker.originalTimeMs = TimeUtils::samplesToMs(endMarker.originalPosition, sampleRate);
            view->addMarker(endMarker);
            addedPositions.insert(currentBeat.position);
            ++markersCreated;
        }
    }

    view->sortMarkers();
    return markersCreated;
}

QPoint markerCenterPixel(const WaveformView& view, qint64 samplePos)
{
    const QVector<QVector<float>>& audioData = view.getAudioData();
    const float zoom = view.getZoomLevel();
    const float hOffset = view.getHorizontalOffset();
    const int w = view.width();
    const qint64 refSize = audioData[0].size();
    const float samplesPerPixel = float(refSize) / (w * zoom);
    const int visibleSamples = int(w * samplesPerPixel);
    const int maxStartSample = qMax(0, int(refSize - visibleSamples));
    const int startSample = int(hOffset * maxStartSample);
    const float x = (samplePos - startSample) / samplesPerPixel;
    return QPoint(int(x), view.height() / 2);
}

bool waitForMediaLoaded(QMediaPlayer& player, int timeoutMs)
{
    if (player.mediaStatus() == QMediaPlayer::LoadedMedia
        || player.mediaStatus() == QMediaPlayer::BufferedMedia) {
        return true;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
            loop.quit();
    });
    timer.start(timeoutMs);
    loop.exec();
    return player.mediaStatus() == QMediaPlayer::LoadedMedia
        || player.mediaStatus() == QMediaPlayer::BufferedMedia;
}

int findDraggableMarkerIndex(const WaveformView& view)
{
    const QVector<Marker> markers = view.getMarkers();
    for (int i = 0; i < markers.size(); ++i) {
        if (!markers[i].isFixed && !markers[i].isEndMarker)
            return i;
    }
    return -1;
}

bool markersSortedUnique(const QVector<Marker>& markers)
{
    for (int i = 1; i < markers.size(); ++i) {
        if (markers[i].position <= markers[i - 1].position)
            return false;
    }
    return true;
}

struct PreparedScenario {
    QVector<QVector<float>> channels;
    BPMAnalyzer::AnalysisResult analysis;
    int sampleRate = 0;
    int markerCount = 0;
};

PreparedScenario prepareExampleV80Scenario(WaveformView* view)
{
    PreparedScenario scenario;

    const QString filePath = testDataPath(QStringLiteral("example_V80BPM.mp3"));
    if (filePath.isEmpty())
        return scenario;

    const AudioFileService::DecodeResult decoded = AudioFileService::decode(filePath);
    if (!decoded.ok || decoded.channels.isEmpty())
        return scenario;

    scenario.channels = decoded.channels;
    scenario.sampleRate = decoded.sampleRate;

    BPMAnalyzer::AnalysisOptions options;
    options.useMixxxAlgorithm = true;
    options.assumeFixedTempo = false;
    options.minBPM = 50.0f;
    options.maxBPM = 250.0f;
    options.tolerancePercent = 5.0f;

    const QVector<float> mono = toMono(scenario.channels);
    scenario.analysis = BPMAnalyzer::analyzeBPM(mono, scenario.sampleRate, options);
    if (scenario.analysis.bpm <= 0 || scenario.analysis.beats.isEmpty())
        return scenario;

    view->resize(1200, 400);
    view->show();
    QApplication::processEvents();

    view->setAudioData(scenario.channels);
    view->setBeatInfo(scenario.analysis.beats);
    view->setGridStartSample(scenario.analysis.gridStartSample);
    view->setBPM(scenario.analysis.bpm);
    view->setBeatsAligned(false);
    view->setBeatsPerBar(4);
    view->setZoomLevel(1.0f);
    view->setHorizontalOffset(0.0f);

    scenario.markerCount = createCorrectionMarkers(view, options.tolerancePercent, false);
    if (scenario.markerCount < 2) {
        // Если неровных долей мало — ставим пару меток вручную для проверки UI.
        const qint64 total = scenario.channels[0].size();
        const qint64 quarter = total / 4;
        Marker m1(quarter, view->getSampleRate());
        m1.originalPosition = quarter - (scenario.sampleRate / 20);
        m1.originalTimeMs = TimeUtils::samplesToMs(m1.originalPosition, view->getSampleRate());
        Marker m2(quarter * 2, view->getSampleRate());
        m2.originalPosition = quarter * 2 - (scenario.sampleRate / 30);
        m2.originalTimeMs = TimeUtils::samplesToMs(m2.originalPosition, view->getSampleRate());
        view->addMarker(m1);
        view->addMarker(m2);
        view->sortMarkers();
        scenario.markerCount = view->getMarkers().size();
    }

    QApplication::processEvents();
    return scenario;
}

} // namespace

class UiResponsivenessTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void testLoadAnalyzeAndCreateMarkers();
    void testMarkerDragUiResponsiveness();
    void testApplyTimeStretchAfterAlignment();
    void testProcessedPlaybackSmoothness();

private:
    WaveformView* m_view = nullptr;
};

void UiResponsivenessTest::initTestCase()
{
    qDebug() << "Инициализация UI-тестов отклика интерфейса";
}

void UiResponsivenessTest::cleanup()
{
    delete m_view;
    m_view = nullptr;
}

void UiResponsivenessTest::testLoadAnalyzeAndCreateMarkers()
{
    if (shouldSkipIntegration())
        QSKIP("UI-интеграция пропущена в CI (нужен локальный MP3 и мультимедиа). "
              "Задайте DONTFLOAT_RUN_UI_TEST=1 для принудительного запуска.");

    if (testDataPath(QStringLiteral("example_V80BPM.mp3")).isEmpty())
        QSKIP("Файл tests/source4test/example_V80BPM.mp3 не найден");

    m_view = new WaveformView;
    const PreparedScenario scenario = prepareExampleV80Scenario(m_view);

    QVERIFY2(!scenario.channels.isEmpty(), "Аудио должно загрузиться");
    QVERIFY2(scenario.sampleRate > 0, "Sample rate должен быть определён");
    QVERIFY2(scenario.analysis.bpm > 0, "BPM должен быть определён");
    QVERIFY2(scenario.markerCount >= 2, "Должно быть не менее двух меток коррекции");

    const QVector<Marker> markers = m_view->getMarkers();
    QVERIFY(markersSortedUnique(markers));
    qDebug() << "BPM:" << scenario.analysis.bpm
             << "markers:" << markers.size()
             << "samples:" << scenario.channels[0].size();
}

void UiResponsivenessTest::testMarkerDragUiResponsiveness()
{
    if (shouldSkipIntegration())
        QSKIP("UI-интеграция пропущена в CI");

    if (testDataPath(QStringLiteral("example_V80BPM.mp3")).isEmpty())
        QSKIP("Файл tests/source4test/example_V80BPM.mp3 не найден");

    m_view = new WaveformView;
    const PreparedScenario scenario = prepareExampleV80Scenario(m_view);
    QVERIFY(scenario.markerCount >= 2);

    const int markerIndex = findDraggableMarkerIndex(*m_view);
    QVERIFY2(markerIndex >= 0, "Нужна хотя бы одна перетаскиваемая метка");

    const Marker& marker = m_view->getMarkers()[markerIndex];
    const qint64 markerOriginalPos = marker.originalPosition;
    const QPoint start = markerCenterPixel(*m_view, marker.position);
    const QPoint end(start.x() + 40, start.y());

    QElapsedTimer timer;
    timer.start();

    QTest::mousePress(m_view, Qt::LeftButton, Qt::NoModifier, start, 20);
    for (int step = 1; step <= 8; ++step) {
        const QPoint pos(start.x() + (end.x() - start.x()) * step / 8, start.y());
        QTest::mouseMove(m_view, pos, 10);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    QSignalSpy dragFinishedSpy(m_view, &WaveformView::markerDragFinished);
    QTest::mouseRelease(m_view, Qt::LeftButton, Qt::NoModifier, end, 20);

    const int waitMs = envIntMs("DONTFLOAT_UI_DRAG_WAIT_MS", 5000);
    if (dragFinishedSpy.isEmpty()) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(m_view, &WaveformView::markerDragFinished, &loop, &QEventLoop::quit);
        timer.start(waitMs);
        loop.exec();
        QVERIFY2(!timer.isActive(), "markerDragFinished не получен после перетаскивания");
    }

    const qint64 elapsedMs = timer.elapsed();
    const int maxMs = envIntMs("DONTFLOAT_UI_DRAG_MAX_MS", 8000);
    qDebug() << "Перетаскивание метки заняло" << elapsedMs << "мс (лимит" << maxMs << ")";

    QVERIFY2(elapsedMs < maxMs,
             qPrintable(QString("Перетаскивание метки слишком медленное: %1 мс > %2 мс")
                            .arg(elapsedMs)
                            .arg(maxMs)));

    const QVector<Marker> after = m_view->getMarkers();
    QVERIFY(markersSortedUnique(after));

    int afterIndex = -1;
    for (int i = 0; i < after.size(); ++i) {
        if (after[i].originalPosition == markerOriginalPos) {
            afterIndex = i;
            break;
        }
    }
    QVERIFY2(afterIndex >= 0, "Метка должна остаться в списке после сортировки");
    QVERIFY2(after[afterIndex].position != marker.position,
             "Позиция метки должна измениться после перетаскивания");
}

void UiResponsivenessTest::testApplyTimeStretchAfterAlignment()
{
    if (shouldSkipIntegration())
        QSKIP("UI-интеграция пропущена в CI");

    if (testDataPath(QStringLiteral("example_V80BPM.mp3")).isEmpty())
        QSKIP("Файл tests/source4test/example_V80BPM.mp3 не найден");

    m_view = new WaveformView;
    const PreparedScenario scenario = prepareExampleV80Scenario(m_view);
    QVERIFY(scenario.markerCount >= 2);

    QElapsedTimer timer;
    timer.start();

    const TimeStretchProcessor::StretchResult stretchResult =
        m_view->applyTimeStretch(m_view->getMarkers());

    const qint64 elapsedMs = timer.elapsed();
    const int maxMs = envIntMs("DONTFLOAT_UI_STRETCH_MAX_MS", 120000);
    qDebug() << "applyTimeStretch занял" << elapsedMs << "мс";

    QVERIFY2(!stretchResult.audioData.isEmpty(), "Результат растяжения не должен быть пустым");
    QVERIFY2(!stretchResult.audioData[0].isEmpty(), "Канал после растяжения не должен быть пустым");
    QVERIFY2(elapsedMs < maxMs,
             qPrintable(QString("applyTimeStretch слишком медленный: %1 мс").arg(elapsedMs)));

    const qint64 inSamples = scenario.channels[0].size();
    const qint64 outSamples = stretchResult.audioData[0].size();
    QVERIFY2(outSamples > inSamples / 4 && outSamples < inSamples * 4,
             qPrintable(QString("Нереалистичная длина после растяжения: in=%1 out=%2")
                            .arg(inSamples)
                            .arg(outSamples)));
}

void UiResponsivenessTest::testProcessedPlaybackSmoothness()
{
    if (shouldSkipIntegration())
        QSKIP("UI-интеграция пропущена в CI");

    if (testDataPath(QStringLiteral("example_V80BPM.mp3")).isEmpty())
        QSKIP("Файл tests/source4test/example_V80BPM.mp3 не найден");

    m_view = new WaveformView;
    const PreparedScenario scenario = prepareExampleV80Scenario(m_view);
    QVERIFY(scenario.markerCount >= 2);

    const TimeStretchProcessor::StretchResult stretchResult =
        m_view->applyTimeStretch(m_view->getMarkers());
    QVERIFY(!stretchResult.audioData.isEmpty());

    QTemporaryFile tempWav(QDir::temp().filePath(QStringLiteral("dontfloat_ui_test_XXXXXX.wav")));
    tempWav.setAutoRemove(true);
    QVERIFY(tempWav.open());

    WavWriter::WriteOptions opts;
    opts.format = WavWriter::SampleFormat::Float32;
    opts.dither = false;

    QString writeError;
    QVERIFY(WavWriter::writeFile(tempWav.fileName(),
                                 stretchResult.audioData,
                                 scenario.sampleRate,
                                 &writeError,
                                 opts));

    QAudioOutput audioOutput;
    QMediaPlayer player;
    player.setAudioOutput(&audioOutput);
    player.setSource(QUrl::fromLocalFile(tempWav.fileName()));

    const int loadTimeoutMs = envIntMs("DONTFLOAT_UI_MEDIA_LOAD_MS", 10000);
    if (!waitForMediaLoaded(player, loadTimeoutMs)) {
        QSKIP("QMediaPlayer не загрузил WAV в отведённое время (нет мультимедиа-бэкенда?)");
    }

    player.play();
    QTest::qWait(200);

    if (player.playbackState() != QMediaPlayer::PlayingState) {
        QSKIP("Воспроизведение не стартовало в этой среде");
    }

    QVector<qint64> positions;
    positions.reserve(25);
    for (int i = 0; i < 25; ++i) {
        positions.append(player.position());
        QTest::qWait(100);
    }
    player.stop();

    int backwardJumps = 0;
    int stalls = 0;
    qint64 totalAdvance = 0;
    for (int i = 1; i < positions.size(); ++i) {
        const qint64 delta = positions[i] - positions[i - 1];
        if (delta < -80)
            ++backwardJumps;
        if (delta == 0)
            ++stalls;
        if (delta > 0)
            totalAdvance += delta;
    }

    qDebug() << "Playback samples:" << positions.size()
             << "totalAdvanceMs:" << totalAdvance
             << "backwardJumps:" << backwardJumps
             << "stalls:" << stalls;

    QVERIFY2(totalAdvance >= 500,
             qPrintable(QString("Позиция воспроизведения почти не продвинулась (%1 мс за ~2.5 с)")
                            .arg(totalAdvance)));
    QVERIFY2(backwardJumps <= 1,
             qPrintable(QString("Слишком много скачков назад: %1").arg(backwardJumps)));
    QVERIFY2(stalls <= 8,
             qPrintable(QString("Слишком много пауз в позиции воспроизведения: %1").arg(stalls)));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    UiResponsivenessTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ui_responsiveness_test.moc"
