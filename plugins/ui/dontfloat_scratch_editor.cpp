#include "dontfloat_scratch_editor.h"

#include "../../include/audiofileservice.h"
#if defined(DONTFLOAT_WITH_ARA)
#include "../ara/dontfloat_ara_document_controller.h"
#include "../core/dontfloat_diagnostics.h"
#endif
#include "../../include/markerengine.h"
#include "../../include/timeutils.h"
#include "../../include/timestretchprocessor.h"
#include "../../include/uiconstants.h"
#include "../../include/waveformview.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Dontfloat::Plugins::Ui {
namespace {

using Dontfloat::PluginCore::TrackAudioBuffer;
using Dontfloat::PluginCore::TrackToolSession;

QVector<float> toQVector(const std::vector<float>& samples)
{
    QVector<float> out(static_cast<int>(samples.size()));
    if (!samples.empty()) {
        std::copy(samples.begin(), samples.end(), out.begin());
    }
    return out;
}

std::vector<float> toStdVector(const QVector<float>& samples)
{
    return std::vector<float>(samples.begin(), samples.end());
}

QVector<QVector<float>> channelsFromBuffer(const TrackAudioBuffer& buffer)
{
    QVector<QVector<float>> channels;
    if (!buffer.left.empty()) {
        channels.append(toQVector(buffer.left));
        if (!buffer.right.empty()) {
            channels.append(toQVector(buffer.right));
        }
    } else if (!buffer.mono.empty()) {
        channels.append(toQVector(buffer.mono));
    }
    return channels;
}

QVector<BPMAnalyzer::BeatInfo> createAlignedBeatGrid(float bpm,
                                                     qint64 gridStartSample,
                                                     qint64 totalSamples,
                                                     int sampleRate,
                                                     const QVector<QVector<float>>& audioData)
{
    QVector<BPMAnalyzer::BeatInfo> alignedBeats;
    if (bpm <= 0.0f || totalSamples <= 0 || sampleRate <= 0) {
        return alignedBeats;
    }

    const float beatInterval = (60.0f * float(sampleRate)) / bpm;
    qint64 pos = gridStartSample;
    while (pos < totalSamples) {
        BPMAnalyzer::BeatInfo beat;
        beat.position = pos;
        beat.expectedPosition = pos;
        beat.confidence = 1.0f;
        beat.deviation = 0.0f;
        beat.energy = 0.0f;
        if (pos >= 0 && !audioData.isEmpty() && pos < audioData[0].size()) {
            beat.energy = std::abs(audioData[0][static_cast<int>(pos)]);
        }
        alignedBeats.append(beat);
        pos += static_cast<qint64>(beatInterval);
    }
    return alignedBeats;
}

} // namespace

DontfloatScratchEditor::DontfloatScratchEditor(QWidget* parent, const QString& productName)
    : QWidget(parent)
    , productName_(productName)
    , bpmWatcher_(new QFutureWatcher<void>(this))
    , alignWatcher_(new QFutureWatcher<void>(this))
{
    setObjectName(QStringLiteral("dontfloatScratchEditor"));
    setMinimumSize(480, 180);
    setWindowTitle(productName_);

    // Разметка как в главном окне: волна во всю ширину, под ней скроллбар,
    // действия — узкой строкой (макет MARKDOWN/example_plugin_dontfloat.svg)
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(UiConstants::kTimelineHorizontalMarginPx, 4,
                             UiConstants::kTimelineHorizontalMarginPx, 0);
    root->setSpacing(0);

    waveform_ = new WaveformView(this);
    waveform_->setMinimumHeight(90);
    waveform_->setBeatsPerBar(beatsPerBar_);
    root->addWidget(waveform_, 1);

    horizontalScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    horizontalScrollBar_->setMinimum(0);
    horizontalScrollBar_->setMaximum(0);
    horizontalScrollBar_->setSingleStep(10);
    horizontalScrollBar_->setPageStep(100);
    horizontalScrollBar_->setFixedHeight(UiConstants::kHorizontalScrollBarHeightPx);
    root->addWidget(horizontalScrollBar_);
    connect(horizontalScrollBar_, &QScrollBar::valueChanged, this, [this](int value) {
        if (waveform_ && horizontalScrollBar_->maximum() > 0) {
            const float offset =
                qBound(0.0f, float(value) / float(horizontalScrollBar_->maximum()), 1.0f);
            waveform_->setHorizontalOffset(offset);
        }
    });

    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 4, 0, 4);
    toolbar->setSpacing(6);
    // Аудио приходит с дорожки DAW, анализ идёт сам при каждом изменении
    // содержимого — кнопок «Анализировать» и «Экспорт WAV» больше нет
    alignButton_ = new QPushButton(tr("Align beats"), this);
    applyStretchButton_ = new QPushButton(tr("Apply stretch"), this);
    for (QPushButton* button : { alignButton_, applyStretchButton_ }) {
        button->setProperty("dontfloatSlim", true);
    }
    toolbar->addStretch(1);
    toolbar->addWidget(alignButton_);
    toolbar->addWidget(applyStretchButton_);
    root->addLayout(toolbar);

    setStatus(tr("load audio or play a track in the DAW to capture the signal."));

    // Хост отдаёт аудио блоками; анализ запускаем, когда поток утих
    araPollTimer_ = new QTimer(this);
    araPollTimer_->setInterval(kAraPollIntervalMs);
    connect(araPollTimer_, &QTimer::timeout, this, [this]() { pullAudioFromAra(); });

    autoAnalysisTimer_ = new QTimer(this);
    autoAnalysisTimer_->setSingleShot(true);
    autoAnalysisTimer_->setInterval(kAutoAnalysisDelayMs);
    connect(autoAnalysisTimer_, &QTimer::timeout, this, &DontfloatScratchEditor::startAutoAnalysis);

    connect(alignButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onAlignBeatsClicked);
    connect(applyStretchButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onApplyStretchClicked);
    connect(bpmWatcher_, &QFutureWatcher<void>::finished,
            this, &DontfloatScratchEditor::onBpmAnalysisFinished);
    connect(alignWatcher_, &QFutureWatcher<void>::finished,
            this, &DontfloatScratchEditor::onAlignFinished);
    connect(waveform_, &WaveformView::markersChanged, this, &DontfloatScratchEditor::onMarkersChanged);

    // Метка растяжения по каретке — та же клавиша, что в главном окне.
    // Контекст виджета: в DAW клавиша M не должна перехватываться у хоста
    auto* markerShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    markerShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(markerShortcut, &QShortcut::activated, this, &DontfloatScratchEditor::addMarkerAtPlayhead);

    // Каретку двигают в плагине — просим DAW встать туда же
    connect(waveform_, &WaveformView::positionChanged, this, [this](qint64 positionMs) {
        if (applyingHostPlayhead_ || !session_) {
            return;
        }
        const int sampleRate = session_->audioBuffer().sampleRate;
        if (sampleRate > 0) {
            const qint64 sourceSample = (positionMs * sampleRate) / 1000;
            // Под ARA каретку ведёт DAW: просим её встать туда же, куда
            // кликнули в волне, иначе каретки разъедутся
            requestHostSeek(sourceSample);
            emit seekRequested(sourceSample);
        }
    });

    updateActionButtons();
}

void DontfloatScratchEditor::setProductName(const QString& productName)
{
    productName_ = productName;
    setWindowTitle(productName_);
    refreshFromSession();
}

void DontfloatScratchEditor::setStatus(const QString& text)
{
    // Статусбар живёт в оболочке плагина — как в главном окне, один на весь редактор
    emit statusMessage(QStringLiteral("%1: %2").arg(productName_, text));
}

void DontfloatScratchEditor::updateActionButtons()
{
    const bool hasAudio = session_ && !session_->audioBuffer().empty();
    const bool hasBpm = lastAnalysis_.bpm > 0.0f;
    const bool busy = analysisRunning_ || alignRunning_;
    alignButton_->setEnabled(hasAudio && hasBpm && !busy);
    applyStretchButton_->setEnabled(hasAudio && waveform_ && waveform_->hasTimelineStretch() && !busy);
}

void DontfloatScratchEditor::bindSession(TrackToolSession* session)
{
    session_ = session;
    refreshFromSession();
}

void DontfloatScratchEditor::refreshFromSession()
{
    refreshWaveform();
    updateActionButtons();
}

void DontfloatScratchEditor::notifyHostAudioAppended()
{
    // Хост зовёт это на каждый блок: перерисовку волны тормозим, иначе поток
    // блоков забивает UI (анализ всё равно ждёт паузы в потоке)
    if (!hostRefreshClock_.isValid()
        || hostRefreshClock_.elapsed() >= kHostRefreshIntervalMs) {
        hostRefreshClock_.restart();
        refreshFromSession();
    }
    // Анализ дорожки стартует сам, как только DAW перестала слать блоки
    if (!analysisRunning_ && autoAnalysisTimer_ && session_ && !session_->audioBuffer().empty()) {
        autoAnalysisTimer_->start();
    }
}

void DontfloatScratchEditor::refreshWaveform()
{
    if (!session_ || !waveform_) {
        return;
    }
    const TrackAudioBuffer& buffer = session_->audioBuffer();
    if (buffer.empty()) {
        Dontfloat::PluginCore::Diagnostics::log("wave.cleared");
        waveform_->setAudioData({});
        waveform_->clearMarkers();
        lastAnalysis_ = {};
        setStatus(tr("load audio or play a track in the DAW to capture the signal."));
        return;
    }

    const QVector<QVector<float>> channels = channelsFromBuffer(buffer);
    waveform_->setAudioData(channels);
    waveform_->setSampleRate(buffer.sampleRate);

    if (lastAnalysis_.bpm > 0.0f) {
        waveform_->setBeatInfo(lastAnalysis_.beats);
        waveform_->setGridStartSample(lastAnalysis_.gridStartSample);
        waveform_->setBPM(lastAnalysis_.bpm);
        waveform_->setBeatsPerBar(beatsPerBar_);
        setStatus(tr("audio %1 samples @ %2 Hz · BPM %3")
                      .arg(buffer.frameCount())
                      .arg(buffer.sampleRate)
                      .arg(double(lastAnalysis_.bpm), 0, 'f', 2));
    } else {
        setStatus(tr("audio: %1 samples, %2 Hz — click “BPM analysis”")
                      .arg(buffer.frameCount())
                      .arg(buffer.sampleRate));
    }
}

void DontfloatScratchEditor::publishRenderedOutput(const QVector<QVector<float>>& channels,
                                                   int sampleRate)
{
    if (!session_ || channels.isEmpty()) {
        return;
    }
    Dontfloat::PluginCore::TrackAudioBuffer rendered;
    rendered.sampleRate = sampleRate;
    rendered.channelCount = channels.size();
    rendered.left = toStdVector(channels[0]);
    if (channels.size() > 1) {
        rendered.right = toStdVector(channels[1]);
    }
    rendered.mono = toStdVector(AudioFileService::toMono(channels));
    // Результат покрывает ту же дорожку, что и захват, — с её начала
    session_->setRenderedOutput(rendered, 0);
    emit renderedOutputChanged();
}

void DontfloatScratchEditor::setAraBinding(const void* extension)
{
#if defined(DONTFLOAT_WITH_ARA)
    araBinding_ = extension;
    araAudioApplied_ = false;
    if (!araBinding_) {
        if (araPollTimer_) {
            araPollTimer_->stop();
        }
        return;
    }
    // В момент привязки документ обычно ещё пуст: хост заводит источник и
    // разрешает доступ к сэмплам позже, поэтому дальше опрашиваем
    if (!pullAudioFromAra() && araPollTimer_) {
        araPollTimer_->start();
    }
#else
    Q_UNUSED(extension);
#endif
}

bool DontfloatScratchEditor::pullAudioFromAra()
{
#if defined(DONTFLOAT_WITH_ARA)
    if (!araBinding_ || araAudioApplied_ || !session_) {
        return false;
    }

    const auto* extension = static_cast<const ARA::PlugIn::PlugInExtension*>(araBinding_);
    auto* controller = extension->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    if (!controller) {
        return false;
    }

    Dontfloat::Ara::AraAudioSource* source =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*extension);
    if (!source) {
        // Роли хост мог назначить позже привязки — на дорожке источник один
        source = controller->onlyAudioSource();
    }
    if (!source || source->monoSamples().empty()) {
        return false;
    }
    if (source->analysisRunning()) {
        // Пока хост разбирает источник, не лезем: перенос дорожки в сессию
        // и перерисовка волны занимают UI-поток на сотни миллисекунд, и
        // хост успевал не дождаться завершения разбора
        return false;
    }

    TrackAudioBuffer buffer;
    buffer.sampleRate = int(source->getSampleRate() > 0.0 ? source->getSampleRate() : 44100.0);
    buffer.channelCount = 1;
    buffer.mono = source->monoSamples();
    session_->setAudioBuffer(buffer);
    araAudioApplied_ = true;
    if (araPollTimer_) {
        araPollTimer_->stop();
    }

    QElapsedTimer applyClock;
    applyClock.start();
    refreshFromSession();
    const qint64 refreshMs = applyClock.elapsed();
    pullBeatGridFromAra();

    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        char line[200];
        std::snprintf(line, sizeof(line),
                      "ara.audio.applied frames=%zu rate=%d refreshMs=%lld",
                      buffer.mono.size(), buffer.sampleRate,
                      static_cast<long long>(refreshMs));
        Dontfloat::PluginCore::Diagnostics::log(line);
    }
    return true;
#else
    return false;
#endif
}

void DontfloatScratchEditor::pullBeatGridFromAra()
{
#if defined(DONTFLOAT_WITH_ARA)
    if (!araBinding_ || !waveform_ || !session_) {
        return;
    }
    const auto* extension = static_cast<const ARA::PlugIn::PlugInExtension*>(araBinding_);
    auto* controller = extension->getDocumentController<Dontfloat::Ara::AraDocumentController>();
    if (!controller) {
        return;
    }

    Dontfloat::Ara::AraAudioSource* source =
        Dontfloat::Ara::AraDocumentController::audioSourceForInstance(*extension);
    if (!source) {
        source = controller->onlyAudioSource();
    }
    if (!source) {
        return;
    }

    // Где клип стоит на таймлайне и насколько растянут — по первому клипу
    // этого источника: каретка и сетка обязаны считаться в одних координатах
    const std::vector<Dontfloat::Ara::AraClipPlacement> clips =
        controller->clipsForAudioSource(source);
    if (!clips.empty()) {
        const Dontfloat::Ara::AraClipPlacement& clip = clips.front();
        araClipStartPlaybackSec_ = clip.startInPlaybackSeconds;
        araClipStartSourceSec_ = clip.startInSourceSeconds;
        araClipStretch_ = clip.stretchFactor() > 0.0 ? clip.stretchFactor() : 1.0;
        araClipValid_ = true;
    }

    const Dontfloat::Ara::AraBeatGrid grid = controller->hostBeatGrid();
    if (!grid.valid || grid.tempoBpm <= 0.0) {
        return;
    }

    const int sampleRate = session_->audioBuffer().sampleRate > 0
        ? session_->audioBuffer().sampleRate
        : 44100;
    // Начало такта 1 хост даёт во времени проекта — переносим внутрь файла
    double gridStartInSource = grid.gridStartSeconds;
    if (araClipValid_) {
        gridStartInSource = araClipStartSourceSec_
            + (grid.gridStartSeconds - araClipStartPlaybackSec_) / araClipStretch_;
    }
    const qint64 gridStartSample =
        qint64(std::llround(std::max(0.0, gridStartInSource) * double(sampleRate)));

    beatsPerBar_ = grid.beatsPerBar > 0 ? grid.beatsPerBar : beatsPerBar_;
    waveform_->setBPM(float(grid.tempoBpm));
    waveform_->setBeatsPerBar(beatsPerBar_);
    waveform_->setGridStartSample(gridStartSample);
    waveform_->update();

    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        char line[200];
        std::snprintf(line, sizeof(line),
                      "ara.grid.applied bpm=%.3f beats=%d gridStart=%lld stretch=%.4f",
                      grid.tempoBpm, beatsPerBar_,
                      static_cast<long long>(gridStartSample), araClipStretch_);
        Dontfloat::PluginCore::Diagnostics::log(line);
    }
#endif
}

#if defined(DONTFLOAT_WITH_ARA)
Dontfloat::Ara::AraDocumentController* DontfloatScratchEditor::araController() const
{
    if (!araBinding_) {
        return nullptr;
    }
    const auto* extension = static_cast<const ARA::PlugIn::PlugInExtension*>(araBinding_);
    return extension->getDocumentController<Dontfloat::Ara::AraDocumentController>();
}
#endif

bool DontfloatScratchEditor::requestHostTransport(bool start)
{
#if defined(DONTFLOAT_WITH_ARA)
    if (auto* controller = araController()) {
        return controller->requestHostPlayback(start);
    }
#else
    Q_UNUSED(start);
#endif
    return false;
}

double DontfloatScratchEditor::sourceSampleToProjectSeconds(qint64 sourceSample,
                                                            int sampleRate) const
{
    if (sampleRate <= 0) {
        return 0.0;
    }
    const double sourceSeconds = double(sourceSample) / double(sampleRate);
#if defined(DONTFLOAT_WITH_ARA)
    if (araClipValid_) {
        // Обратное к projectSampleToSource: клип может стоять не в начале
        // проекта и быть растянут
        return araClipStartPlaybackSec_
            + (sourceSeconds - araClipStartSourceSec_) * araClipStretch_;
    }
#endif
    return sourceSeconds;
}

bool DontfloatScratchEditor::requestHostSeek(qint64 sourceSample)
{
#if defined(DONTFLOAT_WITH_ARA)
    auto* controller = araController();
    if (!controller || !session_) {
        return false;
    }
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (sampleRate <= 0) {
        return false;
    }
    return controller->requestHostPlaybackPosition(
        sourceSampleToProjectSeconds(sourceSample, sampleRate));
#else
    Q_UNUSED(sourceSample);
    return false;
#endif
}

qint64 DontfloatScratchEditor::projectSampleToSource(qint64 projectSample, int sampleRate) const
{
#if defined(DONTFLOAT_WITH_ARA)
    if (araClipValid_ && sampleRate > 0) {
        const double projectSeconds = double(projectSample) / double(sampleRate);
        const double sourceSeconds = araClipStartSourceSec_
            + (projectSeconds - araClipStartPlaybackSec_) / araClipStretch_;
        return qint64(std::llround(sourceSeconds * double(sampleRate)));
    }
#endif
    Q_UNUSED(sampleRate);
    return projectSample;
}

void DontfloatScratchEditor::writeChannelsToSession(const QVector<QVector<float>>& channels, int sampleRate)
{
    if (!session_ || channels.isEmpty()) {
        return;
    }

    TrackAudioBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.channelCount = channels.size();
    if (!channels.isEmpty()) {
        buffer.left = toStdVector(channels[0]);
    }
    if (channels.size() > 1) {
        buffer.right = toStdVector(channels[1]);
    }
    buffer.mono = toStdVector(AudioFileService::toMono(channels));
    session_->setAudioBuffer(buffer);
}

void DontfloatScratchEditor::setHostPlayhead(qint64 samplePosition)
{
    if (!waveform_ || !session_) {
        return;
    }
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (sampleRate <= 0) {
        return;
    }
    // Каретка приходит во времени проекта, а волна показывает источник: под
    // ARA клип может стоять не в начале и быть растянут, поэтому позицию
    // переносим внутрь файла — иначе каретка стояла бы не там, где звучит
    const qint64 inSource = projectSampleToSource(samplePosition, sampleRate);

    // Волна принимает позицию в миллисекундах — там же, где каретка DAW.
    // Флаг гасит обратную отправку в DAW — иначе позиция ходила бы по кругу.
    const qint64 clamped = std::clamp<qint64>(
        inSource, 0, qint64(session_->audioBuffer().frameCount()));
    applyingHostPlayhead_ = true;
    waveform_->setPlaybackPosition((clamped * 1000) / sampleRate);
    applyingHostPlayhead_ = false;
}

void DontfloatScratchEditor::addMarkerAtPlayhead()
{
    if (!waveform_ || waveform_->getAudioData().isEmpty()) {
        setStatus(tr("no audio captured from the DAW yet"));
        return;
    }
    const int sampleRate = waveform_->getSampleRate();
    if (sampleRate <= 0) {
        return;
    }

    const qint64 sample = (waveform_->getPlaybackPosition() * sampleRate) / 1000;
    const int before = waveform_->getMarkers().size();
    waveform_->addMarker(sample);
    const int after = waveform_->getMarkers().size();
    if (after > before) {
        setStatus(tr("stretch marker added at %1")
                      .arg(TimeUtils::formatTime(waveform_->getPlaybackPosition())));
    } else {
        setStatus(tr("could not add a marker here (too close to an existing one)"));
    }
    updateActionButtons();
}

void DontfloatScratchEditor::setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample)
{
    if (!waveform_ || bpm <= 0.0) {
        return;
    }
    // Сетка хоста главнее собственного анализа: в DAW доли считает она
    const bool changed = std::fabs(double(waveform_->getBPM()) - bpm) > 0.01
        || beatsPerBar_ != std::max(1, beatsPerBar)
        || waveform_->getGridStartSample() != barStartSample;
    if (!changed) {
        return;
    }

    beatsPerBar_ = std::max(1, beatsPerBar);
    waveform_->setBPM(float(bpm));
    waveform_->setBeatsPerBar(beatsPerBar_);
    waveform_->setGridStartSample(std::max<qint64>(0, barStartSample));
    waveform_->update();
}

void DontfloatScratchEditor::shiftBeatGrid(int beats)
{
    if (!waveform_) {
        return;
    }
    const QVector<QVector<float>>& data = waveform_->getAudioData();
    if (data.isEmpty() || data[0].isEmpty()) {
        setStatus(tr("no audio captured from the DAW yet"));
        return;
    }
    const float bpm = waveform_->getBPM();
    const int sampleRate = waveform_->getSampleRate();
    if (bpm <= 0.0f || sampleRate <= 0) {
        setStatus(tr("no beat grid to shift (BPM not detected)"));
        return;
    }

    const qint64 beatSamples = qMax<qint64>(1, qRound((60.0f * sampleRate) / bpm));
    // Shift, как в главном окне, двигает и метки
    const bool moveMarkers = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    const qint64 maxGridStart = qMax<qint64>(0, data[0].size() - 1);
    const qint64 oldGridStart = waveform_->getGridStartSample();
    const qint64 newGridStart =
        qBound<qint64>(0, oldGridStart + qint64(beats) * beatSamples, maxGridStart);
    if (newGridStart == oldGridStart) {
        setStatus(tr("beat grid is already at the file boundary"));
        return;
    }

    waveform_->shiftGridBySamples(newGridStart - oldGridStart, moveMarkers);
    setStatus(beats < 0 ? tr("beat grid shifted one beat back")
                        : tr("beat grid shifted one beat forward"));
}

void DontfloatScratchEditor::snapMarkersToGrid()
{
    if (!waveform_) {
        return;
    }
    if (waveform_->getBPM() <= 0.0f || waveform_->getBeatInfo().isEmpty()) {
        setStatus(tr("no beat grid to snap to (BPM or beats not detected)"));
        return;
    }
    const QVector<Marker> markers = waveform_->getMarkers();
    if (markers.size() < 2) {
        setStatus(tr("no markers to snap to the grid"));
        return;
    }

    const QVector<Marker> snapped = waveform_->snapMarkersToGrid(markers);
    if (snapped.size() != markers.size()) {
        setStatus(tr("could not snap markers to the grid"));
        return;
    }
    int movedCount = 0;
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i].position != snapped[i].position) {
            ++movedCount;
        }
    }
    waveform_->setMarkers(snapped);
    setStatus(movedCount > 0 ? tr("markers snapped to the grid (%1)").arg(movedCount)
                             : tr("all markers are already on the grid"));
}

void DontfloatScratchEditor::detectOnsetMarkers()
{
    if (!waveform_) {
        return;
    }
    const QVector<QVector<float>>& data = waveform_->getAudioData();
    const int sampleRate = waveform_->getSampleRate();
    // Алгоритм общий с главным окном
    const QVector<qint64> onsets = MarkerUtils::detectOnsetSamples(data, sampleRate);
    if (onsets.isEmpty()) {
        setStatus(tr("no transients found"));
        return;
    }

    waveform_->clearMarkers();
    for (qint64 sample : onsets) {
        waveform_->addMarker(Marker(sample, sampleRate));
    }
    waveform_->sortMarkers();
    waveform_->update();
    setStatus(tr("created %1 transient markers").arg(onsets.size()));
}

void DontfloatScratchEditor::setLoopBoundAtPlayhead(bool start)
{
    if (!waveform_) {
        return;
    }
    const qint64 positionMs = waveform_->getPlaybackPosition();
    if (start) {
        loopStartMs_ = positionMs;
        waveform_->setLoopStart(positionMs);
        setStatus(tr("loop start A: %1").arg(TimeUtils::formatTime(positionMs)));
    } else {
        loopEndMs_ = positionMs;
        waveform_->setLoopEnd(positionMs);
        setStatus(tr("loop end B: %1").arg(TimeUtils::formatTime(positionMs)));
    }
}

void DontfloatScratchEditor::setLoopEnabled(bool enabled)
{
    loopEnabled_ = enabled;
    if (!enabled) {
        setStatus(tr("loop off"));
        return;
    }
    if (loopStartMs_ < 0 || loopEndMs_ <= loopStartMs_) {
        setStatus(tr("set loop points A and B first"));
        return;
    }
    setStatus(tr("loop on: %1 — %2")
                  .arg(TimeUtils::formatTime(loopStartMs_), TimeUtils::formatTime(loopEndMs_)));
}

bool DontfloatScratchEditor::loopRegionMs(qint64* startMs, qint64* endMs) const
{
    if (!loopEnabled_ || loopStartMs_ < 0 || loopEndMs_ <= loopStartMs_) {
        return false;
    }
    if (startMs) {
        *startMs = loopStartMs_;
    }
    if (endMs) {
        *endMs = loopEndMs_;
    }
    return true;
}

void DontfloatScratchEditor::startAutoAnalysis()
{
    if (!session_ || session_->audioBuffer().empty() || analysisRunning_) {
        return;
    }
    // Поток аудио утих — показываем дорожку целиком
    refreshFromSession();

    const auto print = Dontfloat::PluginCore::computeContentFingerprint(session_->audioBuffer());
    if (print.empty()) {
        return;
    }
    if (print.hash == analyzedContent_.hash && print.startFrame == analyzedContent_.startFrame
        && print.lengthFrames == analyzedContent_.lengthFrames) {
        return;  // содержимое не менялось
    }

    // Тот же материал на новой позиции — клип переехал в DAW: метки едут с ним.
    // Тип именно std::int64_t: ядро плагина живёт без Qt, а на Linux (LP64)
    // int64_t — это long, qint64 — long long, и qint64* туда не приводится
    std::int64_t shift = 0;
    if (Dontfloat::PluginCore::detectContentShift(analyzedContent_, print, &shift)) {
        shiftAnnotations(shift);
        analyzedContent_ = print;
        setStatus(tr("clip moved by %1 — markers followed")
                      .arg(TimeUtils::formatTime(samplesToMs(shift))));
        return;
    }

    // Содержимое дорожки изменилось — считаем заново
    analyzedContent_ = print;
    runBpmAnalysis();
}

qint64 DontfloatScratchEditor::samplesToMs(qint64 samples) const
{
    const int sampleRate = session_ ? session_->audioBuffer().sampleRate : 0;
    return sampleRate > 0 ? (samples * 1000) / sampleRate : 0;
}

void DontfloatScratchEditor::shiftAnnotations(qint64 deltaSamples)
{
    if (!waveform_ || deltaSamples == 0) {
        return;
    }

    // Метки растяжения едут вместе с клипом
    QVector<Marker> markers = waveform_->getMarkers();
    for (Marker& marker : markers) {
        marker.position = std::max<qint64>(0, marker.position + deltaSamples);
        marker.originalPosition = std::max<qint64>(0, marker.originalPosition + deltaSamples);
        marker.updateTimeFromSamples(waveform_->getSampleRate());
    }
    waveform_->setMarkers(markers);

    // Доли анализа едут вместе с материалом, иначе сетка осталась бы на месте
    for (BPMAnalyzer::BeatInfo& beat : lastAnalysis_.beats) {
        beat.position = std::max<qint64>(0, beat.position + deltaSamples);
        beat.expectedPosition = std::max<qint64>(0, beat.expectedPosition + deltaSamples);
    }
    lastAnalysis_.gridStartSample = std::max<qint64>(0, lastAnalysis_.gridStartSample + deltaSamples);
    waveform_->setBeatInfo(lastAnalysis_.beats);

    // Тактовая сетка и точки цикла — тоже часть разметки клипа
    waveform_->setGridStartSample(std::max<qint64>(0, waveform_->getGridStartSample() + deltaSamples));
    const qint64 deltaMs = samplesToMs(deltaSamples);
    if (loopStartMs_ >= 0) {
        loopStartMs_ = std::max<qint64>(0, loopStartMs_ + deltaMs);
        waveform_->setLoopStart(loopStartMs_);
    }
    if (loopEndMs_ >= 0) {
        loopEndMs_ = std::max<qint64>(0, loopEndMs_ + deltaMs);
        waveform_->setLoopEnd(loopEndMs_);
    }
    waveform_->update();
}

void DontfloatScratchEditor::runBpmAnalysis()
{
    if (!session_ || session_->audioBuffer().empty() || analysisRunning_) {
        return;
    }
    const QVector<float> mono = toQVector(session_->audioBuffer().mono);
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (mono.isEmpty() || sampleRate <= 0) {
        setStatus(tr("no audio data for BPM analysis"));
        return;
    }

    analysisRunning_ = true;
    updateActionButtons();
    setStatus(tr("analyzing BPM…"));

    // Результат мимо QFuture::result(): после длинного анализа он падает в
    // QResultStore/QList::at (MSVC Debug) — как и в MainWindow / питч-редакторе
    pendingBpm_ = std::make_shared<BPMAnalyzer::AnalysisResult>();
    bpmWatcher_->setFuture(QtConcurrent::run([mono, sampleRate, result = pendingBpm_]() {
        *result = BPMAnalyzer::analyzeBPM(mono, sampleRate);
    }));
}

void DontfloatScratchEditor::onBpmAnalysisFinished()
{
    analysisRunning_ = false;
    if (!pendingBpm_) {
        updateActionButtons();
        return;
    }
    lastAnalysis_ = *pendingBpm_;
    pendingBpm_.reset();

    if (lastAnalysis_.bpm <= 0.0f) {
        setStatus(tr("could not detect BPM"));
        updateActionButtons();
        return;
    }

    // Отклонения считаем по той же сетке, которую показывает волна.
    BPMAnalyzer::DeviationOptions deviationOptions;
    deviationOptions.gridStartSample = lastAnalysis_.gridStartSample;
    const BPMAnalyzer::DeviationStats deviationStats = BPMAnalyzer::calculateDeviations(
        lastAnalysis_.beats, lastAnalysis_.bpm,
        session_ ? session_->audioBuffer().sampleRate : 44100, deviationOptions);

    if (waveform_) {
        waveform_->setBeatInfo(lastAnalysis_.beats);
        waveform_->setGridStartSample(lastAnalysis_.gridStartSample);
        waveform_->setBPM(lastAnalysis_.bpm);
        waveform_->setBeatsPerBar(beatsPerBar_);
        waveform_->setBeatsAligned(false);
        waveform_->setShowBeatWaveform(true);
        waveform_->update();
    }

    const int unaligned = int(BPMAnalyzer::findUnalignedBeats(lastAnalysis_.beats).size());
    setStatus(tr("BPM %1 · beats %2 · deviations %3 · avg %4%")
                  .arg(double(lastAnalysis_.bpm), 0, 'f', 2)
                  .arg(lastAnalysis_.beats.size())
                  .arg(unaligned)
                  .arg(double(deviationStats.meanAbsDeviation) * 100.0, 0, 'f', 1));
    updateActionButtons();
}

QVector<Marker> DontfloatScratchEditor::makeAlignedBeatMarkers(
    const QVector<BPMAnalyzer::BeatInfo>& beats, qint64 totalSamples, int sampleRate) const
{
    QVector<Marker> markers;
    if (beats.isEmpty() || totalSamples <= 0 || sampleRate <= 0) {
        return markers;
    }

    const qint64 minSegmentSamples = (sampleRate * 50) / 1000;
    markers.append(Marker(0, true, sampleRate));
    qint64 lastPos = 0;

    for (const BPMAnalyzer::BeatInfo& beat : beats) {
        const qint64 pos = beat.position;
        if (pos <= 0) {
            continue;
        }
        if (pos >= totalSamples) {
            break;
        }
        if (pos - lastPos >= minSegmentSamples) {
            Marker m(pos, sampleRate);
            m.originalPosition = pos;
            m.originalTimeMs = TimeUtils::samplesToMs(pos, sampleRate);
            m.updateTimeFromSamples(sampleRate);
            markers.append(m);
            lastPos = pos;
        }
    }

    const qint64 endPos = totalSamples - 1;
    if (endPos > lastPos) {
        Marker end(endPos, true, true, sampleRate);
        end.originalPosition = endPos;
        end.originalTimeMs = TimeUtils::samplesToMs(endPos, sampleRate);
        end.updateTimeFromSamples(sampleRate);
        markers.append(end);
    }
    return markers;
}

void DontfloatScratchEditor::runBeatAlign()
{
    if (!session_ || !waveform_ || lastAnalysis_.bpm <= 0.0f || alignRunning_) {
        return;
    }

    const QVector<QVector<float>> source = channelsFromBuffer(session_->audioBuffer());
    if (source.isEmpty()) {
        return;
    }

    alignRunning_ = true;
    updateActionButtons();
    setStatus(tr("aligning beats…"));

    const BPMAnalyzer::AnalysisResult analysis = lastAnalysis_;
    const int sampleRate = session_->audioBuffer().sampleRate;
    // Выравнивание — это растяжение участков между долями, а не правка отдельных
    // сэмплов: каждая найденная доля едет на ближайшую линию сетки, звук между
    // ними тянется с сохранением высоты. Все каналы обрабатываются вместе, иначе
    // стерео разъедется
    QVector<qint64> beatPositions;
    beatPositions.reserve(analysis.beats.size());
    for (const BPMAnalyzer::BeatInfo& beat : analysis.beats) {
        beatPositions.append(beat.position);
    }

    pendingAligned_ = std::make_shared<AlignedTake>();
    alignWatcher_->setFuture(
        QtConcurrent::run([source, analysis, beatPositions, sampleRate, out = pendingAligned_]() {
            const TimeStretchProcessor::StretchResult aligned =
                TimeStretchProcessor::alignBeatsToGrid(source, beatPositions, analysis.bpm,
                                                       sampleRate, analysis.gridStartSample, true, nullptr);
            // Метки забираем из результата: они уже стоят на фактических стыках
            // сегментов растянутого звука, с учётом съеденных кроссфейдом
            // миллисекунд. Пересобранные по сетке от этих стыков уезжают
            out->audio = aligned.audioData.isEmpty() ? source : aligned.audioData;
            out->markers = aligned.newMarkers;
        }));
}

void DontfloatScratchEditor::onAlignBeatsClicked()
{
    runBeatAlign();
}

void DontfloatScratchEditor::onAlignFinished()
{
    alignRunning_ = false;
    const AlignedTake take = pendingAligned_ ? *pendingAligned_ : AlignedTake();
    const QVector<QVector<float>>& fixed = take.audio;
    pendingAligned_.reset();
    if (fixed.isEmpty() || !waveform_ || !session_) {
        setStatus(tr("beat alignment error"));
        updateActionButtons();
        return;
    }

    const int sampleRate = session_->audioBuffer().sampleRate;
    writeChannelsToSession(fixed, sampleRate);

    const qint64 totalSamples = fixed.isEmpty() ? 0 : fixed[0].size();
    const QVector<BPMAnalyzer::BeatInfo> alignedBeats = createAlignedBeatGrid(
        lastAnalysis_.bpm, lastAnalysis_.gridStartSample, totalSamples, sampleRate, fixed);
    lastAnalysis_.beats = alignedBeats;

    waveform_->setAudioData(fixed);
    waveform_->setSampleRate(sampleRate);
    waveform_->setBeatInfo(alignedBeats);
    waveform_->setGridStartSample(lastAnalysis_.gridStartSample);
    waveform_->setBPM(lastAnalysis_.bpm);
    waveform_->setBeatsAligned(true);
    waveform_->setBeatsPerBar(beatsPerBar_);

    waveform_->clearMarkers();
    // Метки растяжения из самого результата; если тянуть было нечего и их нет,
    // ставим нейтральные по сетке — дорожку всё равно должно быть чем двигать
    QVector<Marker> markers = MarkerUtils::toMarkers(take.markers);
    if (markers.size() < 2) {
        markers = makeAlignedBeatMarkers(alignedBeats, totalSamples, sampleRate);
    }
    if (markers.size() >= 2) {
        waveform_->setMarkers(markers);
    }
    waveform_->update();

    setStatus(tr("beats aligned (BPM %1). Drag stretch markers if needed.")
                  .arg(double(lastAnalysis_.bpm), 0, 'f', 2));
    updateActionButtons();
}

void DontfloatScratchEditor::onApplyStretchClicked()
{
    if (!session_ || !waveform_ || !waveform_->hasTimelineStretch()) {
        return;
    }

    const TimeStretchProcessor::StretchResult result =
        waveform_->applyTimeStretch(waveform_->getMarkers());
    if (result.audioData.isEmpty()) {
        setStatus(tr("failed to apply stretch"));
        return;
    }

    const int sampleRate = waveform_->getSampleRate();
    writeChannelsToSession(result.audioData, sampleRate);
    waveform_->setAudioData(result.audioData);
    waveform_->updateOriginalData(result.audioData);
    if (!result.newMarkers.isEmpty()) {
        waveform_->setMarkers(MarkerUtils::toMarkers(result.newMarkers));
    }
    waveform_->setBeatsAligned(true);
    waveform_->update();

    // Результат уходит в выход плагина: DAW должна услышать растяжение
    publishRenderedOutput(result.audioData, sampleRate);
    setStatus(tr("stretch applied · %1 samples").arg(result.audioData[0].size()));
    updateActionButtons();
}

void DontfloatScratchEditor::onMarkersChanged()
{
    updateActionButtons();
}

} // namespace Dontfloat::Plugins::Ui
