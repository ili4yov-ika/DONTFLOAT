#include "clap_minimal.h"
#include "../core/dontfloat_plugin_core.h"
#include "../ui/dontfloat_qt_hosting.h"
#include "../ui/dontfloat_track_tool_editor.h"

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
using Dontfloat::Plugins::Ui::DontfloatTrackToolEditor;
using Dontfloat::Plugins::Ui::ensureQtApplication;

namespace {

constexpr const char* kPluginId = "com.dontfloat.track-tool";
constexpr uint32_t kEditorWidth = 960;
constexpr uint32_t kEditorHeight = 640;

struct ClapTrackTool {
    clap_plugin_t plugin = {};
    const clap_host_t* host = nullptr;
    TrackToolSession session;
    std::unique_ptr<DontfloatTrackToolEditor> editor;
    uint32_t editorWidth = kEditorWidth;
    uint32_t editorHeight = kEditorHeight;
};

const char* const kFeatures[] = {
    "audio-effect",
    "analyzer",
    "stereo",
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION_INIT,
    kPluginId,
    "DONTFLOAT Track Tool",
    "DONTFLOAT",
    "https://github.com/ili4yov-ika/DONTFLOAT",
    nullptr,
    nullptr,
    "0.0.0.1",
    "DONTFLOAT track analysis and BPM alignment tool MVP",
    kFeatures,
};

ClapTrackTool* self(const clap_plugin_t* plugin)
{
    return plugin ? static_cast<ClapTrackTool*>(plugin->plugin_data) : nullptr;
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

bool pluginInit(const clap_plugin_t*)
{
    return true;
}

void pluginDestroy(const clap_plugin_t* plugin)
{
    delete self(plugin);
}

bool pluginActivate(const clap_plugin_t* plugin, double sampleRate, uint32_t, uint32_t maxFrames)
{
    ClapTrackTool* s = self(plugin);
    if (!s || sampleRate <= 0.0) {
        return false;
    }
    return s->session.prepare(TrackAudioInfo{int(sampleRate), 2, std::max<uint32_t>(maxFrames, 1)}) == TrackToolStatus::Ok;
}

void pluginDeactivate(const clap_plugin_t*)
{
}

bool pluginStartProcessing(const clap_plugin_t*)
{
    return true;
}

void pluginStopProcessing(const clap_plugin_t*)
{
}

void pluginReset(const clap_plugin_t* plugin)
{
    if (ClapTrackTool* s = self(plugin)) {
        s->session.reset();
    }
}

clap_process_status pluginProcess(const clap_plugin_t* plugin, const clap_process_t* process)
{
    ClapTrackTool* s = self(plugin);
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

uint32_t audioPortsCount(const clap_plugin_t*, bool)
{
    return 1;
}

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

const clap_plugin_audio_ports_t kAudioPortsExtension = {
    audioPortsCount,
    audioPortsGet,
};

bool guiIsApiSupported(const clap_plugin_t*, const char* api, bool isFloating)
{
#if defined(_WIN32)
    return api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0 && !isFloating;
#else
    (void)api;
    (void)isFloating;
    return false;
#endif
}

bool guiGetPreferredApi(const clap_plugin_t*, const char** api, bool* isFloating)
{
#if defined(_WIN32)
    if (!api || !isFloating) {
        return false;
    }
    *api = CLAP_WINDOW_API_WIN32;
    *isFloating = false;
    return true;
#else
    (void)api;
    (void)isFloating;
    return false;
#endif
}

bool guiCreate(const clap_plugin_t* plugin, const char* api, bool isFloating)
{
    ClapTrackTool* s = self(plugin);
    if (!s || !guiIsApiSupported(plugin, api, isFloating)) {
        return false;
    }

    ensureQtApplication();
    s->editor = std::make_unique<DontfloatTrackToolEditor>();
    s->editor->bindSession(&s->session);
    s->editor->resize(int(s->editorWidth), int(s->editorHeight));
    s->editor->setAttribute(Qt::WA_NativeWindow, true);
    return true;
}

void guiDestroy(const clap_plugin_t* plugin)
{
    if (ClapTrackTool* s = self(plugin)) {
        if (s->editor) {
            s->editor->hide();
        }
        s->editor.reset();
    }
}

bool guiSetScale(const clap_plugin_t*, double)
{
    return false;
}

bool guiGetSize(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height)
{
    ClapTrackTool* s = self(plugin);
    if (!s || !width || !height) {
        return false;
    }
    *width = s->editorWidth;
    *height = s->editorHeight;
    return true;
}

bool guiCanResize(const clap_plugin_t*)
{
    return false;
}

bool guiGetResizeHints(const clap_plugin_t*, void*)
{
    return false;
}

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
    ClapTrackTool* s = self(plugin);
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
    ClapTrackTool* s = self(plugin);
    if (!s || !s->editor || !window || std::strcmp(window->api, CLAP_WINDOW_API_WIN32) != 0) {
        return false;
    }

    const HWND child = reinterpret_cast<HWND>(s->editor->winId());
    const HWND parent = reinterpret_cast<HWND>(window->win32);
    SetParent(child, parent);
    SetWindowLongPtr(child, GWL_STYLE, WS_CHILD | WS_VISIBLE);
    MoveWindow(child, 0, 0, int(s->editorWidth), int(s->editorHeight), TRUE);
    return true;
#else
    (void)plugin;
    (void)window;
    return false;
#endif
}

bool guiSetTransient(const clap_plugin_t*, const clap_window_t*)
{
    return false;
}

void guiSuggestTitle(const clap_plugin_t*, const char*)
{
}

bool guiShow(const clap_plugin_t* plugin)
{
    ClapTrackTool* s = self(plugin);
    if (!s || !s->editor) {
        return false;
    }
    s->editor->show();
    return true;
}

bool guiHide(const clap_plugin_t* plugin)
{
    ClapTrackTool* s = self(plugin);
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

const void* pluginGetExtension(const clap_plugin_t*, const char* id)
{
    if (id && std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &kAudioPortsExtension;
    }
    if (id && std::strcmp(id, CLAP_EXT_GUI) == 0) {
        return &kGuiExtension;
    }
    return nullptr;
}

void pluginOnMainThread(const clap_plugin_t*)
{
}

uint32_t factoryGetPluginCount(const clap_plugin_factory_t*)
{
    return 1;
}

const clap_plugin_descriptor_t* factoryGetPluginDescriptor(const clap_plugin_factory_t*, uint32_t index)
{
    return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t* factoryCreatePlugin(const clap_plugin_factory_t*, const clap_host_t* host, const char* pluginId)
{
    if (!pluginId || std::strcmp(pluginId, kPluginId) != 0) {
        return nullptr;
    }

    auto* s = new ClapTrackTool();
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

bool entryInit(const char*)
{
    return true;
}

void entryDeinit()
{
}

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
