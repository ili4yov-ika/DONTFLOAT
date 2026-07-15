#ifndef DONTFLOAT_PLUGIN_EDITOR_SHELL_H
#define DONTFLOAT_PLUGIN_EDITOR_SHELL_H

#include "../core/plugin_product.h"

#include <QWidget>

namespace Dontfloat::Plugins::Ui {

class DontfloatPluginEditorShell final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatPluginEditorShell(Dontfloat::PluginCore::PluginProduct product,
                                        QWidget* parent = nullptr);

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void notifyHostAudioAppended();

private:
    Dontfloat::PluginCore::PluginProduct product_;
    Dontfloat::PluginCore::TrackToolSession* session_ = nullptr;
    QWidget* content_ = nullptr;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PLUGIN_EDITOR_SHELL_H
