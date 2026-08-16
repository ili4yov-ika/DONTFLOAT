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

#include <QLabel>
#include <QVBoxLayout>

namespace Dontfloat::Plugins::Ui {

DontfloatPluginEditorShell::DontfloatPluginEditorShell(
    Dontfloat::PluginCore::PluginProduct product, QWidget* parent)
    : QWidget(parent)
    , product_(product)
{
    const Dontfloat::PluginCore::PluginProductDesc& info =
        Dontfloat::PluginCore::productDesc(product_);
    const QString productName = QString::fromUtf8(info.clapName);
    const QString productDescription = QString::fromUtf8(info.clapDescription);

    setObjectName(QStringLiteral("dontfloatPluginEditorShell"));
    setWindowTitle(productName);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("dontfloatPluginHeader"));
    header->setStyleSheet(QStringLiteral(
        "#dontfloatPluginHeader {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #1e2430, stop:1 #2a3344);"
        "  border-bottom: 1px solid #3a4558;"
        "}"
        "#dontfloatPluginHeader QLabel#productTitle {"
        "  color: #f2f5fa; font-size: 16px; font-weight: 700;"
        "}"
        "#dontfloatPluginHeader QLabel#productSubtitle {"
        "  color: #9aa6bb; font-size: 11px;"
        "}"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(2);

    auto* title = new QLabel(productName, header);
    title->setObjectName(QStringLiteral("productTitle"));
    auto* subtitle = new QLabel(productDescription, header);
    subtitle->setObjectName(QStringLiteral("productSubtitle"));
    subtitle->setWordWrap(true);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    root->addWidget(header);

#if DONTFLOAT_PLUGIN_PRODUCT_INDEX == 0
    content_ = new DontfloatFullEditor(this);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 1
    content_ = new DontfloatScratchEditor(this, productName);
#elif DONTFLOAT_PLUGIN_PRODUCT_INDEX == 2
    content_ = new DontfloatPitchEditor(this, productName);
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
