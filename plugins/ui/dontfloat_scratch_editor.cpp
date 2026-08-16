#include "dontfloat_scratch_editor.h"

#include "../../include/audiofileservice.h"
#include "../../include/markerengine.h"
#include "../../include/timeutils.h"
#include "../../include/timestretchprocessor.h"
#include "../../include/wavwriter.h"
#include "../../include/waveformview.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>
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
    setMinimumSize(900, 480);
    setWindowTitle(productName_);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* toolbar = new QHBoxLayout();
    // Аудио приходит с дорожки DAW — собственного импорта у плагина нет
    analyzeButton_ = new QPushButton(tr("BPM analysis"), this);
    analyzeButton_->setToolTip(tr("Re-run the analysis of the track captured from the DAW"));
    alignButton_ = new QPushButton(tr("Align beats"), this);
    applyStretchButton_ = new QPushButton(tr("Apply stretch"), this);
    exportButton_ = new QPushButton(tr("Export WAV…"), this);
    toolbar->addWidget(analyzeButton_);
    toolbar->addWidget(alignButton_);
    toolbar->addWidget(applyStretchButton_);
    toolbar->addWidget(exportButton_);
    toolbar->addStretch(1);
    root->addLayout(toolbar);

    waveform_ = new WaveformView(this);
    waveform_->setMinimumHeight(320);
    waveform_->setBeatsPerBar(beatsPerBar_);
    root->addWidget(waveform_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(QStringLiteral("color:#aeb6c8;"));
    root->addWidget(statusLabel_);
    setStatus(tr("load audio or play a track in the DAW to capture the signal."));

    // Хост отдаёт аудио блоками; анализ запускаем, когда поток утих
    autoAnalysisTimer_ = new QTimer(this);
    autoAnalysisTimer_->setSingleShot(true);
    autoAnalysisTimer_->setInterval(kAutoAnalysisDelayMs);
    connect(autoAnalysisTimer_, &QTimer::timeout, this, &DontfloatScratchEditor::startAutoAnalysis);

    connect(analyzeButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onAnalyzeBpmClicked);
    connect(alignButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onAlignBeatsClicked);
    connect(applyStretchButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onApplyStretchClicked);
    connect(exportButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onExportClicked);
    connect(bpmWatcher_, &QFutureWatcher<void>::finished,
            this, &DontfloatScratchEditor::onBpmAnalysisFinished);
    connect(alignWatcher_, &QFutureWatcher<void>::finished,
            this, &DontfloatScratchEditor::onAlignFinished);
    connect(waveform_, &WaveformView::markersChanged, this, &DontfloatScratchEditor::onMarkersChanged);

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
    if (!statusLabel_) {
        return;
    }
    statusLabel_->setText(QStringLiteral("%1: %2").arg(productName_, text));
}

void DontfloatScratchEditor::updateActionButtons()
{
    const bool hasAudio = session_ && !session_->audioBuffer().empty();
    const bool hasBpm = lastAnalysis_.bpm > 0.0f;
    const bool busy = analysisRunning_ || alignRunning_;
    analyzeButton_->setEnabled(hasAudio && !busy);
    alignButton_->setEnabled(hasAudio && hasBpm && !busy);
    applyStretchButton_->setEnabled(hasAudio && waveform_ && waveform_->hasTimelineStretch() && !busy);
    exportButton_->setEnabled(hasAudio && !busy);
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
    refreshFromSession();
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

void DontfloatScratchEditor::startAutoAnalysis()
{
    if (!session_ || session_->audioBuffer().empty() || analysisRunning_) {
        return;
    }
    if (lastAnalysis_.bpm > 0.0f) {
        return;  // дорожка уже проанализирована
    }
    runBpmAnalysis();
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

void DontfloatScratchEditor::onAnalyzeBpmClicked()
{
    runBpmAnalysis();
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
    pendingAligned_ = std::make_shared<QVector<QVector<float>>>();
    alignWatcher_->setFuture(
        QtConcurrent::run([source, analysis, sampleRate, out = pendingAligned_]() {
            QVector<QVector<float>> fixed = source;
            for (int ch = 0; ch < fixed.size(); ++ch) {
                fixed[ch] = BPMAnalyzer::alignToBeatGrid(
                    source[ch], sampleRate, analysis.bpm, analysis.gridStartSample);
            }
            *out = fixed;
        }));
}

void DontfloatScratchEditor::onAlignBeatsClicked()
{
    runBeatAlign();
}

void DontfloatScratchEditor::onAlignFinished()
{
    alignRunning_ = false;
    const QVector<QVector<float>> fixed = pendingAligned_ ? *pendingAligned_
                                                          : QVector<QVector<float>>();
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
    const QVector<Marker> markers = makeAlignedBeatMarkers(alignedBeats, totalSamples, sampleRate);
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

    setStatus(tr("stretch applied · %1 samples").arg(result.audioData[0].size()));
    updateActionButtons();
}

void DontfloatScratchEditor::onExportClicked()
{
    if (!session_ || session_->audioBuffer().empty()) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("%1 — WAV export").arg(productName_), QString(),
        tr("WAV (*.wav)"));
    if (path.isEmpty()) {
        return;
    }

    const QVector<QVector<float>> channels = channelsFromBuffer(session_->audioBuffer());
    QString error;
    if (!WavWriter::writeFile(path, channels, session_->audioBuffer().sampleRate, &error)) {
        setStatus(tr("export error: %1").arg(error));
        return;
    }
    setStatus(tr("exported: %1").arg(path));
}

void DontfloatScratchEditor::onMarkersChanged()
{
    updateActionButtons();
}

} // namespace Dontfloat::Plugins::Ui
