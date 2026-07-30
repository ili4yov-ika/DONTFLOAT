#include "lv2_minimal.h"
#include "../core/plugin_host_config.h"
#include "../core/dontfloat_plugin_core.h"
#include "../ui/dontfloat_plugin_editor_shell.h"
#include "../ui/dontfloat_qt_hosting.h"

#include <QApplication>
#include <QEventLoop>
#include <QString>

#include <cstring>
#include <memory>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

const void* featureData(const LV2_Feature* const* features, const char* uri)
{
    if (!features || !uri) {
        return nullptr;
    }
    for (int i = 0; features[i]; ++i) {
        if (features[i]->URI && std::strcmp(features[i]->URI, uri) == 0) {
            return features[i]->data;
        }
    }
    return nullptr;
}

int idleUi(LV2UI_Handle handle)
{
    auto* ui = static_cast<Lv2PluginUi*>(handle);
    if (!ui || !ui->editor) {
        return 0;
    }
    if (QApplication* app = qApp) {
        app->processEvents(QEventLoop::AllEvents, 16);
    }
    return 0;
}

const LV2UI_Idle_Interface kIdleInterface = {idleUi};

LV2UI_Handle instantiateUi(const LV2UI_Descriptor*,
                           const char* pluginUri,
                           const char*,
                           LV2UI_Write_Function,
                           LV2UI_Controller,
                           LV2UI_Widget* widget,
                           const LV2_Feature* const* features)
{
    if (!pluginUri || std::strcmp(pluginUri, desc().lv2Uri) != 0 || !widget) {
        return nullptr;
    }

    ensureQtApplication(desc().clapName);

    auto* ui = new Lv2PluginUi();
    ui->editor = std::make_unique<DontfloatPluginEditorShell>(product());
    ui->editor->bindSession(&sharedSession(product()));
    ui->editor->setWindowTitle(QString::fromUtf8(desc().clapName));
    ui->editor->setAttribute(Qt::WA_NativeWindow, true);
    ui->editor->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    ui->editor->resize(kEditorWidth, kEditorHeight);

#if defined(_WIN32)
    // Force native HWND before parenting into the host.
    (void)ui->editor->winId();
    const HWND child = reinterpret_cast<HWND>(ui->editor->winId());
    if (const void* parentData = featureData(features, LV2_UI__parent)) {
        const HWND parent = *static_cast<const HWND*>(parentData);
        if (parent) {
            SetParent(child, parent);
            SetWindowLongPtr(child, GWL_STYLE, WS_CHILD | WS_VISIBLE);
            MoveWindow(child, 0, 0, kEditorWidth, kEditorHeight, TRUE);
        }
    }
#endif

    ui->editor->show();
    *widget = reinterpret_cast<LV2UI_Widget>(ui->editor->winId());

    if (const auto* resize = static_cast<const LV2UI_Resize*>(featureData(features, LV2_UI__resize))) {
        if (resize->ui_resize) {
            resize->ui_resize(resize->handle, kEditorWidth, kEditorHeight);
        }
    }

    return ui;
}

void cleanupUi(LV2UI_Handle handle)
{
    auto* ui = static_cast<Lv2PluginUi*>(handle);
    if (ui && ui->editor) {
#if defined(_WIN32)
        SetParent(reinterpret_cast<HWND>(ui->editor->winId()), nullptr);
#endif
        ui->editor->hide();
    }
    delete ui;
}

void portEvent(LV2UI_Handle, uint32_t, uint32_t, uint32_t, const void*) {}

const void* extensionDataUi(const char* uri)
{
    if (uri && std::strcmp(uri, LV2_UI__idleInterface) == 0) {
        return &kIdleInterface;
    }
    return nullptr;
}

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
