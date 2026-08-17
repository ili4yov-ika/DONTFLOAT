#ifndef DONTFLOAT_FULL_EDITOR_H
#define DONTFLOAT_FULL_EDITOR_H

#include "../core/dontfloat_plugin_core.h"

#include <QWidget>

namespace Dontfloat::Plugins::Ui {

class DontfloatScratchEditor;
class DontfloatPitchEditor;

class DontfloatFullEditor final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatFullEditor(QWidget* parent = nullptr);

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void notifyHostAudioAppended();
    /** Каретка DAW (сэмплы дорожки) — в обе секции. */
    void setHostPlayhead(qint64 samplePosition);

private:
    DontfloatScratchEditor* scratch_ = nullptr;
    DontfloatPitchEditor* pitch_ = nullptr;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_FULL_EDITOR_H
