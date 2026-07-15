#include "dontfloat_plugin_editor_shell.h"

#ifndef DONTFLOAT_PLUGIN_PRODUCT_INDEX
#error "DONTFLOAT_PLUGIN_PRODUCT_INDEX must be defined for plugin UI targets"
#endif

#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
#include "dontfloat_full_editor.h"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
#include "dontfloat_scratch_editor.h"
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
#include "dontfloat_pitch_editor.h"
#else
#error "Invalid DONTFLOAT_PLUGIN_PRODUCT_INDEX"
#endif

#include <QVBoxLayout>

namespace Dontfloat::Plugins::Ui {

DontfloatPluginEditorShell::DontfloatPluginEditorShell(
    Dontfloat::PluginCore::PluginProduct product, QWidget* parent)
    : QWidget(parent)
    , product_(product)
{
    setObjectName(QStringLiteral("dontfloatPluginEditorShell"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
    content_ = new DontfloatFullEditor(this);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
    content_ = new DontfloatScratchEditor(this);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
    content_ = new DontfloatPitchEditor(this);
#endif
    root->addWidget(content_, 1);
}

void DontfloatPluginEditorShell::bindSession(Dontfloat::PluginCore::TrackToolSession* session)
{
    session_ = session;
    if (!content_) {
        return;
    }
#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
    static_cast<DontfloatFullEditor*>(content_)->bindSession(session);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
    static_cast<DontfloatScratchEditor*>(content_)->bindSession(session);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
    static_cast<DontfloatPitchEditor*>(content_)->bindSession(session);
#endif
}

void DontfloatPluginEditorShell::notifyHostAudioAppended()
{
    if (!content_) {
        return;
    }
#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
    static_cast<DontfloatFullEditor*>(content_)->notifyHostAudioAppended();
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
    static_cast<DontfloatScratchEditor*>(content_)->notifyHostAudioAppended();
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
    static_cast<DontfloatPitchEditor*>(content_)->notifyHostAudioAppended();
#endif
}

} // namespace Dontfloat::Plugins::Ui
