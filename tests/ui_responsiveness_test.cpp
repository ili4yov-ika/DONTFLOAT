#include <algorithm>

#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QApplication>
#include <QtGui/QMouseEvent>
#include <QtCore/QRectF>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtCore/QRandomGenerator>
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

/** Индекс метки под точкой — копия WaveformView::getMarkerIndexAt для тестов. */
int markerIndexAtPixel(const WaveformView& view, const QPoint& pos)
{
    const QVector<QVector<float>>& audioData = view.getAudioData();
    if (audioData.isEmpty())
        return -1;

    const qint64 referenceSize = audioData[0].size();
    const float samplesPerPixel = float(referenceSize) / (view.rect().width() * view.getZoomLevel());
    const int visibleSamples = int(view.rect().width() * samplesPerPixel);
    const int maxStartSample = qMax(0, int(referenceSize - visibleSamples));
    const int startSample = int(view.getHorizontalOffset() * maxStartSample);

    const float diamondSize = 10.0f;
    const float centerY = view.rect().height() / 2.0f;
    const QVector<Marker> markers = view.getMarkers();
    for (int i = 0; i < markers.size(); ++i) {
        const float x = (markers[i].position - startSample) / samplesPerPixel;
        const QRectF diamondRect(x - diamondSize / 2, centerY - diamondSize / 2, diamondSize, diamondSize);
        const QRectF expandedRect = diamondRect.adjusted(-5, -5, 5, 5);
        if (expandedRect.contains(pos))
            return i;
    }
    return -1;
}

/** Пиксель метки — та же формула, что в WaveformView::getMarkerIndexAt. */
QPoint markerPixelAt(const WaveformView& view, qint64 samplePos)
{
    const QRect r = view.rect();
    const int w = r.width();
    const int h = r.height();
    const QVector<QVector<float>>& audioData = view.getAudioData();
    if (w <= 0 || h <= 0 || audioData.isEmpty())
        return QPoint(-1, -1);

    const qint64 refSize = audioData[0].size();
    const float zoom = view.getZoomLevel();
    const float hOffset = view.getHorizontalOffset();
    const float samplesPerPixel = float(refSize) / (w * zoom);
    const int visibleSamples = int(w * samplesPerPixel);
    const int maxStartSample = qMax(0, int(refSize - visibleSamples));
    const int startSample = int(hOffset * maxStartSample);
    const qreal x = qreal(samplePos - startSample) / qreal(samplesPerPixel);
    const int xi = qBound(0, int(qBound(qreal(0), x, qreal(w - 1))), w - 1);
    return QPoint(xi, h / 2);
}

void ensureViewGeometry(WaveformView* view, int width = 1200, int height = 400)
{
    if (!view)
        return;
    view->resize(width, height);
    view->setGeometry(0, 0, width, height);
    view->show();
    for (int i = 0; i < 8; ++i)
        QApplication::processEvents();
}

void scrollViewToSample(WaveformView* view, qint64 samplePos)
{
    const QVector<QVector<float>>& audioData = view->getAudioData();
    if (!view || audioData.isEmpty())
        return;

    const QRect r = view->rect();
    const int w = r.width();
    if (w <= 0)
        return;

    const qint64 refSize = audioData[0].size();
    const float zoom = view->getZoomLevel();
    const float samplesPerPixel = float(refSize) / (w * zoom);
    const int visibleSamples = int(w * samplesPerPixel);
    const int maxStartSample = qMax(0, int(refSize - visibleSamples));
    if (maxStartSample <= 0) {
        view->setHorizontalOffset(0.0f);
        QApplication::processEvents();
        return;
    }

    const int idealStart = int(samplePos - visibleSamples / 2);
    const int clampedStart = qBound(0, idealStart, maxStartSample);
    view->setHorizontalOffset(float(clampedStart) / float(maxStartSample));
    QApplication::processEvents();
}

int addRandomMarkers(WaveformView* view, int count, int sampleRate)
{
    if (!view || count <= 0)
        return 0;

    const QVector<QVector<float>>& audioData = view->getAudioData();
    if (audioData.isEmpty())
        return 0;

    const qint64 total = audioData[0].size();
    const qint64 minPos = total / 10;
    const qint64 maxPos = total * 9 / 10;
    if (maxPos <= minPos)
        return 0;

    QSet<qint64> used;
    for (const Marker& m : view->getMarkers())
        used.insert(m.position);

    int added = 0;
    QRandomGenerator* rng = QRandomGenerator::global();
    for (int attempt = 0; attempt < count * 20 && added < count; ++attempt) {
        const qint64 pos = minPos + rng->bounded(maxPos - minPos);
        if (used.contains(pos))
            continue;

        Marker m(pos, sampleRate);
        const qint64 offset = rng->bounded(sampleRate / 50, qMax(sampleRate / 49, sampleRate / 10));
        m.originalPosition = pos - offset;
        m.originalTimeMs = TimeUtils::samplesToMs(m.originalPosition, sampleRate);
        view->addMarker(m);
        used.insert(pos);
        ++added;
    }

    view->sortMarkers();
    return added;
}

QVector<int> pickRandomDraggableIndices(const WaveformView& view, int wantCount)
{
    QVector<int> pool;
    const QVector<Marker> markers = view.getMarkers();
    for (int i = 0; i < markers.size(); ++i) {
        if (!markers[i].isFixed && !markers[i].isEndMarker)
            pool.append(i);
    }

    if (pool.isEmpty())
        return {};

    QRandomGenerator* rng = QRandomGenerator::global();
    for (int i = pool.size() - 1; i > 0; --i)
        std::swap(pool[i], pool[rng->bounded(i + 1)]);

    if (pool.size() > wantCount)
        pool.resize(wantCount);
    return pool;
}

bool waitMarkerDragFinished(WaveformView* view, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(view, &WaveformView::markerDragFinished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    return !timer.isActive();
}

struct DragSimulationResult {
    bool ok = false;
    qint64 elapsedMs = 0;
    QString error;
    qint64 positionBefore = 0;
    qint64 positionAfter = 0;
};

DragSimulationResult simulateMarkerDrag(WaveformView* view, int markerIndex, int deltaPixels)
{
    DragSimulationResult result;
    if (!view || markerIndex < 0 || markerIndex >= view->getMarkers().size()) {
        result.error = QStringLiteral("invalid marker index");
        return result;
    }

    const Marker& marker = view->getMarkers()[markerIndex];
    if (marker.isFixed || marker.isEndMarker) {
        result.error = QStringLiteral("marker not draggable");
        return result;
    }

    ensureViewGeometry(view);
    scrollViewToSample(view, marker.position);
    QPoint start = markerPixelAt(*view, marker.position);
    for (int attempt = 0; attempt < 4 && markerIndexAtPixel(*view, start) != markerIndex; ++attempt) {
        scrollViewToSample(view, marker.position);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        start = markerPixelAt(*view, marker.position);
    }
    if (view->rect().width() <= 0 || start.x() < 0) {
        result.error = QStringLiteral("view not laid out or marker outside view");
        return result;
    }
    if (markerIndexAtPixel(*view, start) != markerIndex) {
        result.error = QStringLiteral("marker not under simulated cursor");
        return result;
    }

    const QPoint end(qBound(0, start.x() + deltaPixels, view->rect().width() - 1), start.y());
    result.positionBefore = marker.position;

    auto postMouse = [&](QEvent::Type type, const QPoint& pos, Qt::MouseButtons buttonsHeld, bool pumpTimers) {
        const Qt::MouseButton button = (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease)
            ? Qt::LeftButton
            : Qt::NoButton;
        QMouseEvent ev(type, pos, view->mapToGlobal(pos), button, buttonsHeld, Qt::NoModifier);
        QApplication::sendEvent(view, &ev);
        if (pumpTimers)
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        else
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };

    QElapsedTimer timer;
    timer.start();

    postMouse(QEvent::MouseButtonPress, start, Qt::LeftButton, false);
    for (int step = 1; step <= 3; ++step) {
        const QPoint pos(start.x() + (end.x() - start.x()) * step / 3, start.y());
        postMouse(QEvent::MouseMove, pos, Qt::LeftButton, false);
    }
    bool dragFinished = false;
    QMetaObject::Connection finishedConn = QObject::connect(
        view, &WaveformView::markerDragFinished, view, [&]() { dragFinished = true; });

    postMouse(QEvent::MouseButtonRelease, end, Qt::NoButton, true);

    const int waitMs = envIntMs("DONTFLOAT_UI_DRAG_WAIT_MS", 300);
    const QDeadlineTimer deadline(waitMs);
    while (!dragFinished && !deadline.hasExpired()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        QTest::qWait(10);
    }
    QObject::disconnect(finishedConn);

    result.elapsedMs = timer.elapsed();

    const QVector<Marker> after = view->getMarkers();
    for (const Marker& m : after) {
        if (m.originalPosition == marker.originalPosition) {
            result.positionAfter = m.position;
            break;
        }
    }

    if (result.positionAfter == result.positionBefore) {
        result.error = QStringLiteral("position unchanged");
        return result;
    }

    if (!dragFinished) {
        qDebug() << "markerDragFinished не получен, но позиция метки изменилась"
                 << result.positionBefore << "->" << result.positionAfter;
    }

    result.ok = true;
    return result;
}

int findMarkerIndexByOriginalPosition(const WaveformView& view, qint64 originalPosition)
{
    const QVector<Marker> markers = view.getMarkers();
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].originalPosition == originalPosition && !markers[i].isFixed)
            return i;
    }
    return -1;
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
    const QVector<QVector<float>>& audio = view.getAudioData();
    const qint64 total = audio.isEmpty() ? 0 : audio[0].size();
    const qint64 lo = total / 4;
    const qint64 hi = total * 3 / 4;

    int fallback = -1;
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].isFixed || markers[i].isEndMarker)
            continue;
        if (total > 0 && markers[i].position >= lo && markers[i].position <= hi)
            return i;
        if (fallback < 0)
            fallback = i;
    }
    return fallback;
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

    ensureViewGeometry(view);

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
    void testMarkerDragWorkflowThreeRandom();
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

    const DragSimulationResult drag = simulateMarkerDrag(m_view, markerIndex, 18);
    QVERIFY2(drag.ok, qPrintable(drag.error));

    const int maxMs = envIntMs("DONTFLOAT_UI_DRAG_MAX_MS", 45000);
    qDebug() << "Перетаскивание метки заняло" << drag.elapsedMs << "мс (лимит" << maxMs << ")";
    QVERIFY2(drag.elapsedMs < maxMs,
             qPrintable(QString("Перетаскивание метки слишком медленное: %1 мс > %2 мс")
                            .arg(drag.elapsedMs)
                            .arg(maxMs)));
    QVERIFY(markersSortedUnique(m_view->getMarkers()));
}

void UiResponsivenessTest::testMarkerDragWorkflowThreeRandom()
{
    if (shouldSkipIntegration())
        QSKIP("UI-интеграция пропущена в CI");

    if (testDataPath(QStringLiteral("example_V80BPM.mp3")).isEmpty())
        QSKIP("Файл tests/source4test/example_V80BPM.mp3 не найден");

    m_view = new WaveformView;
    const PreparedScenario scenario = prepareExampleV80Scenario(m_view);

    QVERIFY2(!scenario.channels.isEmpty(), "Аудио должно загрузиться");
    QVERIFY2(scenario.sampleRate > 0, "Sample rate должен быть определён");
    QVERIFY2(scenario.analysis.bpm > 0, "BPM должен быть определён");
    QVERIFY2(!scenario.analysis.beats.isEmpty(), "Доли должны быть найдены анализатором");
    QVERIFY2(scenario.markerCount >= 2, "После анализа должно быть не менее двух меток");

    const int randomAdded = addRandomMarkers(m_view, 2, scenario.sampleRate);
    QVERIFY2(randomAdded >= 2, "Не удалось добавить случайные метки");

    const QVector<int> picked = pickRandomDraggableIndices(*m_view, 3);
    QVERIFY2(picked.size() >= 3, "Нужно не менее трёх перетаскиваемых меток");

    QVector<qint64> dragTargets;
    dragTargets.reserve(picked.size());
    for (int idx : picked)
        dragTargets.append(m_view->getMarkers()[idx].originalPosition);

    const int perDragMaxMs = envIntMs("DONTFLOAT_UI_DRAG_MAX_MS", 45000);
    const int totalMaxMs = envIntMs("DONTFLOAT_UI_DRAG_TOTAL_MAX_MS", 135000);

    QElapsedTimer totalTimer;
    totalTimer.start();

    for (int round = 0; round < dragTargets.size(); ++round) {
        const int markerIndex = findMarkerIndexByOriginalPosition(*m_view, dragTargets[round]);
        QVERIFY2(markerIndex >= 0,
                 qPrintable(QString("Метка #%1 не найдена перед перетаскиванием").arg(round)));

        const int deltaPx = (round % 2 == 0 ? 1 : -1) * (12 + round * 6);
        const DragSimulationResult drag = simulateMarkerDrag(m_view, markerIndex, deltaPx);
        QVERIFY2(drag.ok, qPrintable(QString("Раунд %1: %2").arg(round).arg(drag.error)));
        QVERIFY2(drag.elapsedMs < perDragMaxMs,
                 qPrintable(QString("Раунд %1 слишком медленный: %2 мс > %3 мс")
                                .arg(round)
                                .arg(drag.elapsedMs)
                                .arg(perDragMaxMs)));

        qDebug() << "Drag round" << (round + 1)
                 << "ms:" << drag.elapsedMs
                 << "pos:" << drag.positionBefore << "->" << drag.positionAfter;

        QVERIFY(markersSortedUnique(m_view->getMarkers()));
    }

    const qint64 totalMs = totalTimer.elapsed();
    qDebug() << "Три перетаскивания заняли" << totalMs << "мс (лимит" << totalMaxMs << ")";
    QVERIFY2(totalMs < totalMaxMs,
             qPrintable(QString("Суммарное время трёх перетаскиваний слишком велико: %1 мс")
                            .arg(totalMs)));
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
