#include "lv2_minimal.h"
#include "../core/dontfloat_plugin_core.h"
#include "../ui/dontfloat_qt_hosting.h"
#include "../ui/dontfloat_track_tool_editor.h"

#include <cstring>
#include <memory>

using Dontfloat::PluginCore::sharedTrackToolSession;
using Dontfloat::Plugins::Ui::DontfloatTrackToolEditor;
using Dontfloat::Plugins::Ui::ensureQtApplication;

namespace {

constexpr const char* kPluginUri = "https://github.com/ili4yov-ika/DONTFLOAT/plugins/track-tool";
constexpr const char* kUiUri = "https://github.com/ili4yov-ika/DONTFLOAT/plugins/track-tool#ui";
constexpr int kEditorWidth = 960;
constexpr int kEditorHeight = 640;

struct Lv2TrackToolUi {
    std::unique_ptr<DontfloatTrackToolEditor> editor;
};

LV2UI_Handle instantiateUi(const LV2UI_Descriptor*,
                           const char* pluginUri,
                           const char*,
                           LV2UI_Write_Function,
                           LV2UI_Controller,
                           LV2UI_Widget* widget,
                           const LV2_Feature* const*)
{
    if (!pluginUri || std::strcmp(pluginUri, kPluginUri) != 0 || !widget) {
        return nullptr;
    }

    ensureQtApplication();

    auto* ui = new Lv2TrackToolUi();
    ui->editor = std::make_unique<DontfloatTrackToolEditor>();
    ui->editor->bindSession(&sharedTrackToolSession());
    ui->editor->setAttribute(Qt::WA_NativeWindow, true);
    ui->editor->resize(kEditorWidth, kEditorHeight);
    ui->editor->show();

    *widget = reinterpret_cast<LV2UI_Widget>(ui->editor->winId());
    return ui;
}

void cleanupUi(LV2UI_Handle handle)
{
    auto* ui = static_cast<Lv2TrackToolUi*>(handle);
    if (ui && ui->editor) {
        ui->editor->hide();
    }
    delete ui;
}

void portEvent(LV2UI_Handle, uint32_t, uint32_t, uint32_t, const void*)
{
}

const void* extensionData(const char*)
{
    return nullptr;
}

const LV2UI_Descriptor kUiDescriptor = {
    kUiUri,
    instantiateUi,
    cleanupUi,
    portEvent,
    extensionData,
};

} // namespace

extern "C" {
LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
    return index == 0 ? &kUiDescriptor : nullptr;
}
}
