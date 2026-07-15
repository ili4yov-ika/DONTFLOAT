#include "dontfloat_full_editor.h"
#include "dontfloat_pitch_editor.h"
#include "dontfloat_scratch_editor.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace Dontfloat::Plugins::Ui {

DontfloatFullEditor::DontfloatFullEditor(QWidget* parent)
    : QWidget(parent)
    , scratch_(new DontfloatScratchEditor(this))
    , pitch_(new DontfloatPitchEditor(this))
{
    setObjectName(QStringLiteral("dontfloatFullEditor"));
    setMinimumSize(960, 720);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(scratch_);
    splitter->addWidget(pitch_);
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
