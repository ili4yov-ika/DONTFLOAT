#include "clap_minimal.h"
#include "../core/dontfloat_plugin_core.h"
#include "../core/plugin_host_config.h"
#include "../ui/dontfloat_plugin_editor_shell.h"
#include "../ui/dontfloat_qt_hosting.h"

#include <QApplication>
#include <QEventLoop>
#include <QString>
#include <QWindow>

#include <algorithm>
#include <cstring>
#include <memory>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::TrackToolStatus;
using Dontfloat::PluginHost::desc;
using Dontfloat::PluginHost::product;
using Dontfloat::Plugins::Ui::DontfloatPluginEditorShell;
using Dontfloat::Plugins::Ui::ensureQtApplication;

namespace {

constexpr uint32_t kEditorWidth = 960;
constexpr uint32_t kEditorHeight = 640;

struct ClapPluginInstance {
    clap_plugin_t plugin = {};
    const clap_host_t* host = nullptr;
    TrackToolSession session;
    std::unique_ptr<DontfloatPluginEditorShell> editor;
    std::unique_ptr<QWindow> foreignParent; // host window wrapper (X11 embedding)
    uint32_t editorWidth = kEditorWidth;
    uint32_t editorHeight = kEditorHeight;
    clap_id guiTimerId = 0;
    bool guiTimerRegistered = false;
};

// Pump the Qt event loop from the host timer so the editor stays responsive
// inside non-Qt DAW hosts (Reaper, Bitwig, Ardour, …).
void pumpQtEvents()
{
    if (QApplication* app = qApp) {
        app->sendPostedEvents();
        app->processEvents(QEventLoop::AllEvents, 8);
        app->sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

const clap_host_timer_support_t* hostTimerSupport(ClapPluginInstance* s)
{
    if (!s || !s->host || !s->host->get_extension) {
        return nullptr;
    }
    return static_cast<const clap_host_timer_support_t*>(
        s->host->get_extension(s->host, CLAP_EXT_TIMER_SUPPORT));
}

const char* const kFeatures[] = {
    "audio-effect",
    "analyzer",
    "stereo",
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION_INIT,
    desc().clapId,
    desc().clapName,
    "DONTFLOAT",
    "https://github.com/ili4yov-ika/DONTFLOAT",
    nullptr,
    nullptr,
    "0.0.0.1",
    desc().clapDescription,
    kFeatures,
};

ClapPluginInstance* self(const clap_plugin_t* plugin)
{
    return plugin ? static_cast<ClapPluginInstance*>(plugin->plugin_data) : nullptr;
}

void copyOrClear(const clap_audio_buffer_t& in, clap_audio_buffer_t& out, uint32_t frameCount)
{
    const uint32_t channels = out.channel_count;
    for (uint32_t ch = 0; ch < channels; ++ch) {
        float* dst = out.data32 ? out.data32[ch] : nullptr;
        if (!dst) {
            continue;
        }
        const float* src = (in.data32 && ch < in.channel_count) ? in.data32[ch] : nullptr;
        if (src) {
            if (src != dst) {
                std::copy(src, src + frameCount, dst);
            }
        } else {
            std::fill(dst, dst + frameCount, 0.0f);
        }
    }
}

bool pluginInit(const clap_plugin_t*) { return true; }

void pluginDestroy(const clap_plugin_t* plugin) { delete self(plugin); }

bool pluginActivate(const clap_plugin_t* plugin, double sampleRate, uint32_t, uint32_t maxFrames)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || sampleRate <= 0.0) {
        return false;
    }
    return s->session.prepare(TrackAudioInfo{int(sampleRate), 2, std::max<uint32_t>(maxFrames, 1)})
        == TrackToolStatus::Ok;
}

void pluginDeactivate(const clap_plugin_t*) {}

bool pluginStartProcessing(const clap_plugin_t*) { return true; }

void pluginStopProcessing(const clap_plugin_t*) {}

void pluginReset(const clap_plugin_t* plugin)
{
    if (ClapPluginInstance* s = self(plugin)) {
        s->session.reset();
    }
}

clap_process_status pluginProcess(const clap_plugin_t* plugin, const clap_process_t* process)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || !process || !process->audio_outputs) {
        return CLAP_PROCESS_ERROR;
    }

    const clap_audio_buffer_t& in = process->audio_inputs ? process->audio_inputs[0] : clap_audio_buffer_t{};
    clap_audio_buffer_t& out = process->audio_outputs[0];
    copyOrClear(in, out, process->frames_count);

    if (process->audio_inputs && process->frames_count > 0) {
        s->session.appendHostFrames(in.data32, int(in.channel_count), int(process->frames_count));
        if (s->editor) {
            s->editor->notifyHostAudioAppended();
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

uint32_t audioPortsCount(const clap_plugin_t*, bool) { return 1; }

bool audioPortsGet(const clap_plugin_t*, uint32_t index, bool isInput, clap_audio_port_info_t* info)
{
    if (!info || index > 0) {
        return false;
    }
    *info = {};
    info->id = isInput ? 0 : 1;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = isInput ? 1 : 0;
    std::strncpy(info->name, isInput ? "Track Input" : "Track Output", sizeof(info->name) - 1);
    return true;
}

const clap_plugin_audio_ports_t kAudioPortsExtension = { audioPortsCount, audioPortsGet };

bool guiIsApiSupported(const clap_plugin_t*, const char* api, bool isFloating)
{
#if defined(_WIN32)
    return api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0 && !isFloating;
#elif defined(__APPLE__)
    (void)api;
    (void)isFloating;
    return false; // Cocoa embedding not implemented yet
#else
    // Linux: embed into the host's X11 window.
    return api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0 && !isFloating;
#endif
}

bool guiGetPreferredApi(const clap_plugin_t*, const char** api, bool* isFloating)
{
    if (!api || !isFloating) {
        return false;
    }
#if defined(_WIN32)
    *api = CLAP_WINDOW_API_WIN32;
    *isFloating = false;
    return true;
#elif defined(__APPLE__)
    return false;
#else
    *api = CLAP_WINDOW_API_X11;
    *isFloating = false;
    return true;
#endif
}

bool guiCreate(const clap_plugin_t* plugin, const char* api, bool isFloating)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || !guiIsApiSupported(plugin, api, isFloating)) {
        return false;
    }

    ensureQtApplication(desc().clapName);
    s->editor = std::make_unique<DontfloatPluginEditorShell>(product());
    s->editor->bindSession(&s->session);
    s->editor->setWindowTitle(QString::fromUtf8(desc().clapName));
    s->editor->resize(int(s->editorWidth), int(s->editorHeight));
    s->editor->setAttribute(Qt::WA_NativeWindow, true);

    // Ask the host to tick us so the Qt event loop keeps running in non-Qt DAWs.
    if (const clap_host_timer_support_t* ts = hostTimerSupport(s)) {
        if (ts->register_timer && ts->register_timer(s->host, 16, &s->guiTimerId)) {
            s->guiTimerRegistered = true;
        }
    }
    return true;
}

void guiDestroy(const clap_plugin_t* plugin)
{
    if (ClapPluginInstance* s = self(plugin)) {
        if (s->guiTimerRegistered) {
            if (const clap_host_timer_support_t* ts = hostTimerSupport(s)) {
                if (ts->unregister_timer) {
                    ts->unregister_timer(s->host, s->guiTimerId);
                }
            }
            s->guiTimerRegistered = false;
        }
        if (s->editor) {
            s->editor->hide();
        }
        s->editor.reset();
        s->foreignParent.reset();
    }
}

bool guiSetScale(const clap_plugin_t*, double) { return false; }

bool guiGetSize(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || !width || !height) {
        return false;
    }
    *width = s->editorWidth;
    *height = s->editorHeight;
    return true;
}

bool guiCanResize(const clap_plugin_t*) { return false; }

bool guiGetResizeHints(const clap_plugin_t*, void*) { return false; }

bool guiAdjustSize(const clap_plugin_t*, uint32_t* width, uint32_t* height)
{
    if (!width || !height) {
        return false;
    }
    *width = kEditorWidth;
    *height = kEditorHeight;
    return true;
}

bool guiSetSize(const clap_plugin_t* plugin, uint32_t width, uint32_t height)
{
    ClapPluginInstance* s = self(plugin);
    if (!s) {
        return false;
    }
    s->editorWidth = width;
    s->editorHeight = height;
    if (s->editor) {
        s->editor->resize(int(width), int(height));
    }
    return true;
}

bool guiSetParent(const clap_plugin_t* plugin, const clap_window_t* window)
{
#if defined(_WIN32)
    ClapPluginInstance* s = self(plugin);
    if (!s || !s->editor || !window || std::strcmp(window->api, CLAP_WINDOW_API_WIN32) != 0) {
        return false;
    }

    const HWND child = reinterpret_cast<HWND>(s->editor->winId());
    const HWND parent = reinterpret_cast<HWND>(window->win32);
    SetParent(child, parent);
    SetWindowLongPtr(child, GWL_STYLE, WS_CHILD | WS_VISIBLE);
    MoveWindow(child, 0, 0, int(s->editorWidth), int(s->editorHeight), TRUE);
    return true;
#elif defined(__APPLE__)
    (void)plugin;
    (void)window;
    return false;
#else
    // Linux/X11: reparent the editor's native window under the host window.
    ClapPluginInstance* s = self(plugin);
    if (!s || !s->editor || !window) {
        return false;
    }
    if (window->api && std::strcmp(window->api, CLAP_WINDOW_API_X11) != 0) {
        return false;
    }

    (void)s->editor->winId(); // force creation of the native X11 window
    QWindow* childWindow = s->editor->windowHandle();
    if (!childWindow) {
        return false;
    }
    QWindow* parentWindow = QWindow::fromWinId(static_cast<WId>(window->x11));
    if (!parentWindow) {
        return false;
    }
    s->foreignParent.reset(parentWindow);
    childWindow->setParent(parentWindow);
    s->editor->move(0, 0);
    s->editor->resize(int(s->editorWidth), int(s->editorHeight));
    return true;
#endif
}

bool guiSetTransient(const clap_plugin_t*, const clap_window_t*) { return false; }

void guiSuggestTitle(const clap_plugin_t*, const char* title)
{
    (void)title;
}

bool guiShow(const clap_plugin_t* plugin)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || !s->editor) {
        return false;
    }
    s->editor->show();
    return true;
}

bool guiHide(const clap_plugin_t* plugin)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || !s->editor) {
        return false;
    }
    s->editor->hide();
    return true;
}

const clap_plugin_gui_t kGuiExtension = {
    guiIsApiSupported,
    guiGetPreferredApi,
    guiCreate,
    guiDestroy,
    guiSetScale,
    guiGetSize,
    guiCanResize,
    guiGetResizeHints,
    guiAdjustSize,
    guiSetSize,
    guiSetParent,
    guiSetTransient,
    guiSuggestTitle,
    guiShow,
    guiHide,
};

void pluginOnTimer(const clap_plugin_t* plugin, clap_id timerId)
{
    ClapPluginInstance* s = self(plugin);
    if (!s || !s->guiTimerRegistered || timerId != s->guiTimerId) {
        return;
    }
    pumpQtEvents();
}

const clap_plugin_timer_support_t kTimerSupportExtension = { pluginOnTimer };

const void* pluginGetExtension(const clap_plugin_t*, const char* id)
{
    if (id && std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &kAudioPortsExtension;
    }
    if (id && std::strcmp(id, CLAP_EXT_GUI) == 0) {
        return &kGuiExtension;
    }
    if (id && std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) {
        return &kTimerSupportExtension;
    }
    return nullptr;
}

void pluginOnMainThread(const clap_plugin_t*) {}

uint32_t factoryGetPluginCount(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* factoryGetPluginDescriptor(const clap_plugin_factory_t*, uint32_t index)
{
    return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t* factoryCreatePlugin(const clap_plugin_factory_t*, const clap_host_t* host, const char* pluginId)
{
    if (!pluginId || std::strcmp(pluginId, desc().clapId) != 0) {
        return nullptr;
    }

    auto* s = new ClapPluginInstance();
    s->host = host;
    s->plugin.desc = &kDescriptor;
    s->plugin.plugin_data = s;
    s->plugin.init = pluginInit;
    s->plugin.destroy = pluginDestroy;
    s->plugin.activate = pluginActivate;
    s->plugin.deactivate = pluginDeactivate;
    s->plugin.start_processing = pluginStartProcessing;
    s->plugin.stop_processing = pluginStopProcessing;
    s->plugin.reset = pluginReset;
    s->plugin.process = pluginProcess;
    s->plugin.get_extension = pluginGetExtension;
    s->plugin.on_main_thread = pluginOnMainThread;
    return &s->plugin;
}

const clap_plugin_factory_t kFactory = {
    factoryGetPluginCount,
    factoryGetPluginDescriptor,
    factoryCreatePlugin,
};

bool entryInit(const char*) { return true; }

void entryDeinit() {}

const void* entryGetFactory(const char* factoryId)
{
    if (factoryId && std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &kFactory;
    }
    return nullptr;
}

} // namespace

extern "C" {
CLAP_EXPORT extern const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    entryInit,
    entryDeinit,
    entryGetFactory,
};
}
