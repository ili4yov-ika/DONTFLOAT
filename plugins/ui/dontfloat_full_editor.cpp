#include "dontfloat_full_editor.h"
#include "dontfloat_pitch_editor.h"
#include "dontfloat_scratch_editor.h"

#include "../core/plugin_product.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace Dontfloat::Plugins::Ui {

DontfloatFullEditor::DontfloatFullEditor(QWidget* parent)
    : QWidget(parent)
{
    const QString productName = QString::fromUtf8(
        Dontfloat::PluginCore::productDesc(Dontfloat::PluginCore::PluginProduct::Full).clapName);

    setObjectName(QStringLiteral("dontfloatFullEditor"));
    setMinimumSize(640, 480);
    setWindowTitle(productName);

    scratch_ = new DontfloatScratchEditor(this, productName);
    pitch_ = new DontfloatPitchEditor(this, productName);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Волна и пианоролл разделены так же, как mainSplitter в главном окне
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName(QStringLiteral("dontfloatFullSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(scratch_);
    splitter->addWidget(pitch_);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    // Пропорции как на макете: волна примерно на треть, пианоролл — остальное
    splitter->setSizes({ 240, 360 });
    root->addWidget(splitter, 1);

    connect(scratch_, &DontfloatScratchEditor::statusMessage,
            this, &DontfloatFullEditor::statusMessage);
    connect(pitch_, &DontfloatPitchEditor::statusMessage,
            this, &DontfloatFullEditor::statusMessage);
    connect(scratch_, &DontfloatScratchEditor::seekRequested,
            this, &DontfloatFullEditor::seekRequested);
    connect(pitch_, &DontfloatPitchEditor::seekRequested,
            this, &DontfloatFullEditor::seekRequested);
    connect(scratch_, &DontfloatScratchEditor::renderedOutputChanged,
            this, &DontfloatFullEditor::renderedOutputChanged);
    connect(pitch_, &DontfloatPitchEditor::renderedOutputChanged,
            this, &DontfloatFullEditor::renderedOutputChanged);
}

void DontfloatFullEditor::bindSession(Dontfloat::PluginCore::TrackToolSession* session)
{
    if (scratch_) {
        scratch_->bindSession(session);
    }
    if (pitch_) {
        pitch_->bindSession(session);
    }
}

void DontfloatFullEditor::notifyHostAudioAppended()
{
    if (scratch_) {
        scratch_->notifyHostAudioAppended();
    }
    if (pitch_) {
        pitch_->notifyHostAudioAppended();
    }
}

void DontfloatFullEditor::setHostPlayhead(qint64 samplePosition)
{
    if (scratch_) {
        scratch_->setHostPlayhead(samplePosition);
    }
    if (pitch_) {
        pitch_->setHostPlayhead(samplePosition);
    }
}

void DontfloatFullEditor::setHostBeatGrid(double bpm, int beatsPerBar, qint64 barStartSample)
{
    if (scratch_) {
        scratch_->setHostBeatGrid(bpm, beatsPerBar, barStartSample);
    }
    if (pitch_) {
        pitch_->setHostBeatGrid(bpm, beatsPerBar, barStartSample);
    }
}

void DontfloatFullEditor::shiftBeatGrid(int beats)
{
    if (scratch_) {
        scratch_->shiftBeatGrid(beats);
    }
}

void DontfloatFullEditor::snapMarkersToGrid()
{
    if (scratch_) {
        scratch_->snapMarkersToGrid();
    }
}

void DontfloatFullEditor::detectOnsetMarkers()
{
    if (scratch_) {
        scratch_->detectOnsetMarkers();
    }
}

void DontfloatFullEditor::setLoopBoundAtPlayhead(bool start)
{
    if (scratch_) {
        scratch_->setLoopBoundAtPlayhead(start);
    }
}

void DontfloatFullEditor::setLoopEnabled(bool enabled)
{
    if (scratch_) {
        scratch_->setLoopEnabled(enabled);
    }
}

bool DontfloatFullEditor::loopRegionMs(qint64* startMs, qint64* endMs) const
{
    return scratch_ && scratch_->loopRegionMs(startMs, endMs);
}

} // namespace Dontfloat::Plugins::Ui
