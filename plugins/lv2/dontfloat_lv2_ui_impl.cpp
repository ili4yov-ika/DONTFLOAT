#include "lv2_minimal.h"
#include "../core/plugin_host_config.h"
#include "../core/dontfloat_plugin_core.h"
#include "../ui/dontfloat_plugin_editor_shell.h"
#include "../ui/dontfloat_qt_hosting.h"

#include <cstring>
#include <memory>

using Dontfloat::PluginCore::sharedSession;
using Dontfloat::PluginHost::desc;
using Dontfloat::PluginHost::product;
using Dontfloat::Plugins::Ui::DontfloatPluginEditorShell;
using Dontfloat::Plugins::Ui::ensureQtApplication;

namespace {

constexpr int kEditorWidth = 960;
constexpr int kEditorHeight = 640;

struct Lv2PluginUi {
    std::unique_ptr<DontfloatPluginEditorShell> editor;
};

LV2UI_Handle instantiateUi(const LV2UI_Descriptor*,
                           const char* pluginUri,
                           const char*,
                           LV2UI_Write_Function,
                           LV2UI_Controller,
                           LV2UI_Widget* widget,
                           const LV2_Feature* const*)
{
    if (!pluginUri || std::strcmp(pluginUri, desc().lv2Uri) != 0 || !widget) {
        return nullptr;
    }

    ensureQtApplication();

    auto* ui = new Lv2PluginUi();
    ui->editor = std::make_unique<DontfloatPluginEditorShell>(product());
    ui->editor->bindSession(&sharedSession(product()));
    ui->editor->setAttribute(Qt::WA_NativeWindow, true);
    ui->editor->resize(kEditorWidth, kEditorHeight);
    ui->editor->show();

    *widget = reinterpret_cast<LV2UI_Widget>(ui->editor->winId());
    return ui;
}

void cleanupUi(LV2UI_Handle handle)
{
    auto* ui = static_cast<Lv2PluginUi*>(handle);
    if (ui && ui->editor) {
        ui->editor->hide();
    }
    delete ui;
}

void portEvent(LV2UI_Handle, uint32_t, uint32_t, uint32_t, const void*) {}

const void* extensionDataUi(const char*) { return nullptr; }

const LV2UI_Descriptor kUiDescriptor = {
    desc().lv2UiUri,
    instantiateUi,
    cleanupUi,
    portEvent,
    extensionDataUi,
};

} // namespace

extern "C" {
LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
    return index == 0 ? &kUiDescriptor : nullptr;
}
}
