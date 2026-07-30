#include "dontfloat_full_editor.h"
#include "dontfloat_pitch_editor.h"
#include "dontfloat_scratch_editor.h"

#include "../core/plugin_product.h"

#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

namespace Dontfloat::Plugins::Ui {

DontfloatFullEditor::DontfloatFullEditor(QWidget* parent)
    : QWidget(parent)
{
    const QString productName = QString::fromUtf8(
        Dontfloat::PluginCore::productDesc(Dontfloat::PluginCore::PluginProduct::Full).clapName);

    setObjectName(QStringLiteral("dontfloatFullEditor"));
    setMinimumSize(960, 720);
    setWindowTitle(productName);

    scratch_ = new DontfloatScratchEditor(this, productName);
    pitch_ = new DontfloatPitchEditor(this, productName);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* rhythmLabel = new QLabel(tr("Rhythm / BPM / stretch"), this);
    rhythmLabel->setStyleSheet(QStringLiteral("color:#cfd6e4; font-weight:600;"));
    auto* pitchLabel = new QLabel(tr("Pitch / notes / key"), this);
    pitchLabel->setStyleSheet(QStringLiteral("color:#cfd6e4; font-weight:600;"));

    auto* topPane = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topPane);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(4);
    topLayout->addWidget(rhythmLabel);
    topLayout->addWidget(scratch_, 1);

    auto* bottomPane = new QWidget(this);
    auto* bottomLayout = new QVBoxLayout(bottomPane);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);
    bottomLayout->addWidget(pitchLabel);
    bottomLayout->addWidget(pitch_, 1);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(topPane);
    splitter->addWidget(bottomPane);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    root->addWidget(splitter, 1);
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

} // namespace Dontfloat::Plugins::Ui
