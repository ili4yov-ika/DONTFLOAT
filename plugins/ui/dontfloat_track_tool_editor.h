#ifndef DONTFLOAT_TRACK_TOOL_EDITOR_H
#define DONTFLOAT_TRACK_TOOL_EDITOR_H

#include "../core/dontfloat_plugin_core.h"

#include <QWidget>

namespace Dontfloat::Plugins::Ui {

class DontfloatPitchEditor;
class DontfloatPluginEditorShell;

/** @deprecated Используйте DontfloatPluginEditorShell. Оставлен для совместимости. */
class DontfloatTrackToolEditor final : public QWidget {
    Q_OBJECT

public:
    explicit DontfloatTrackToolEditor(QWidget* parent = nullptr);

    void bindSession(Dontfloat::PluginCore::TrackToolSession* session);
    void setSessionSnapshot(const Dontfloat::PluginCore::TrackToolSession& session);
    void notifyHostAudioAppended();

    DontfloatPitchEditor* pitchEditor() const;

private:
    DontfloatPluginEditorShell* shell_ = nullptr;
};

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_TRACK_TOOL_EDITOR_H
