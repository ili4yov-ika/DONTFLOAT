#include "dontfloat_scratch_editor.h"

#include "../../include/audiofileservice.h"
#include "../../include/bpmanalyzer.h"
#include "../../include/waveformview.h"

#include <QFileDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>
#include <algorithm>
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

} // namespace

DontfloatScratchEditor::DontfloatScratchEditor(QWidget* parent)
    : QWidget(parent)
    , bpmWatcher_(new QFutureWatcher<float>(this))
{
    setObjectName(QStringLiteral("dontfloatScratchEditor"));
    setMinimumSize(900, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* toolbar = new QHBoxLayout();
    importButton_ = new QPushButton(tr("Импорт WAV…"), this);
    analyzeButton_ = new QPushButton(tr("Анализ BPM"), this);
    toolbar->addWidget(importButton_);
    toolbar->addWidget(analyzeButton_);
    toolbar->addStretch(1);
    root->addLayout(toolbar);

    waveform_ = new WaveformView(this);
    waveform_->setMinimumHeight(320);
    root->addWidget(waveform_, 1);

    statusLabel_ = new QLabel(
        tr("DONTFLOAT Scratch: загрузите аудио или воспроизведите трек в DAW для захвата сигнала."),
        this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(QStringLiteral("color:#aeb6c8;"));
    root->addWidget(statusLabel_);

    connect(importButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onImportAudioClicked);
    connect(analyzeButton_, &QPushButton::clicked, this, &DontfloatScratchEditor::onAnalyzeBpmClicked);
    connect(bpmWatcher_, &QFutureWatcher<float>::finished, this, &DontfloatScratchEditor::onBpmAnalysisFinished);
}

void DontfloatScratchEditor::bindSession(TrackToolSession* session)
{
    session_ = session;
    refreshFromSession();
}

void DontfloatScratchEditor::refreshFromSession()
{
    refreshWaveform();
}

void DontfloatScratchEditor::notifyHostAudioAppended()
{
    refreshFromSession();
}

void DontfloatScratchEditor::refreshWaveform()
{
    if (!session_ || !waveform_) {
        return;
    }
    const TrackAudioBuffer& buffer = session_->audioBuffer();
    if (buffer.empty()) {
        statusLabel_->setText(
            tr("DONTFLOAT Scratch: загрузите аудио или воспроизведите трек в DAW для захвата сигнала."));
        return;
    }

    const QVector<QVector<float>> channels = channelsFromBuffer(buffer);
    waveform_->setAudioData(channels);
    waveform_->setSampleRate(buffer.sampleRate);
    statusLabel_->setText(tr("Аудио: %1 сэмплов, %2 Гц")
                              .arg(buffer.frameCount())
                              .arg(buffer.sampleRate));
}

void DontfloatScratchEditor::onImportAudioClicked()
{
    if (!session_) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Импорт аудио"), QString(),
        tr("Аудиофайлы (*.wav *.mp3 *.flac);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }

    const AudioFileService::DecodeResult decoded = AudioFileService::decode(path);
    if (!decoded.ok) {
        statusLabel_->setText(tr("Ошибка импорта: %1").arg(decoded.error));
        return;
    }

    TrackAudioBuffer buffer;
    buffer.sampleRate = decoded.sampleRate;
    buffer.channelCount = decoded.channels.size();
    if (!decoded.channels.isEmpty()) {
        buffer.left = toStdVector(decoded.channels[0]);
    }
    if (decoded.channels.size() > 1) {
        buffer.right = toStdVector(decoded.channels[1]);
    }
    buffer.mono = toStdVector(AudioFileService::toMono(decoded.channels));

    session_->setAudioBuffer(buffer);
    refreshFromSession();
}

void DontfloatScratchEditor::runBpmAnalysis()
{
    if (!session_ || session_->audioBuffer().empty() || analysisRunning_) {
        return;
    }
    const QVector<float> mono = toQVector(session_->audioBuffer().mono);
    const int sampleRate = session_->audioBuffer().sampleRate;
    if (mono.isEmpty() || sampleRate <= 0) {
        statusLabel_->setText(tr("Нет аудиоданных для анализа BPM"));
        return;
    }

    analysisRunning_ = true;
    analyzeButton_->setEnabled(false);
    statusLabel_->setText(tr("Анализ BPM…"));

    bpmWatcher_->setFuture(QtConcurrent::run([mono, sampleRate]() {
        const BPMAnalyzer::AnalysisResult result = BPMAnalyzer::analyzeBPM(mono, sampleRate);
        return result.bpm;
    }));
}

void DontfloatScratchEditor::onAnalyzeBpmClicked()
{
    runBpmAnalysis();
}

void DontfloatScratchEditor::onBpmAnalysisFinished()
{
    analysisRunning_ = false;
    analyzeButton_->setEnabled(true);
    const float bpm = bpmWatcher_->result();
    if (bpm > 0.0f) {
        statusLabel_->setText(tr("BPM: %1").arg(double(bpm), 0, 'f', 2));
    } else {
        statusLabel_->setText(tr("Не удалось определить BPM"));
    }
}

} // namespace Dontfloat::Plugins::Ui
