#include "dontfloat_track_tool_editor.h"
#include "dontfloat_plugin_editor_shell.h"

#include "../core/plugin_product.h"

#include <QVBoxLayout>

namespace Dontfloat::Plugins::Ui {

DontfloatTrackToolEditor::DontfloatTrackToolEditor(QWidget* parent)
    : QWidget(parent)
    , shell_(new DontfloatPluginEditorShell(Dontfloat::PluginCore::PluginProduct::Full, this))
{
    setObjectName(QStringLiteral("dontfloatTrackToolEditor"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(shell_, 1);
}

void DontfloatTrackToolEditor::bindSession(Dontfloat::PluginCore::TrackToolSession* session)
{
    if (shell_) {
        shell_->bindSession(session);
    }
}

void DontfloatTrackToolEditor::setSessionSnapshot(const Dontfloat::PluginCore::TrackToolSession&)
{
}

void DontfloatTrackToolEditor::notifyHostAudioAppended()
{
    if (shell_) {
        shell_->notifyHostAudioAppended();
    }
}

DontfloatPitchEditor* DontfloatTrackToolEditor::pitchEditor() const
{
    return nullptr;
}

} // namespace Dontfloat::Plugins::Ui
