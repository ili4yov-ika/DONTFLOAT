#include "clap_minimal.h"
#include "../core/dontfloat_plugin_core.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>
#include <system_error>

using Dontfloat::PluginCore::AudioBufferView;
using Dontfloat::PluginCore::PitchShiftParams;
using Dontfloat::PluginCore::PitchShiftProcessor;

namespace {

constexpr const char* kPluginId = "com.dontfloat.pitch-shift";

enum ParamId : clap_id {
    kParamEnabled = 1,
    kParamPitchSemitones = 2,
    kParamGrainHz = 3,
    kParamShape = 4,
    kParamJitter = 5,
    kParamWet = 6,
    kParamPrefilter = 7,
};

struct ParamDef {
    ParamId id;
    const char* name;
    double minValue;
    double maxValue;
    double defaultValue;
};

constexpr ParamDef kParams[] = {
    {kParamEnabled, "Enabled", 0.0, 1.0, 0.0},
    {kParamPitchSemitones, "Pitch", -24.0, 24.0, 0.0},
    {kParamGrainHz, "Grain Rate", 4.0, 40.0, 8.0},
    {kParamShape, "Shape", 0.0, 1.0, 0.5},
    {kParamJitter, "Jitter", 0.0, 1.0, 0.0},
    {kParamWet, "Wet", 0.0, 1.0, 1.0},
    {kParamPrefilter, "Prefilter", 0.0, 1.0, 1.0},
};

struct ClapPitchShift {
    clap_plugin_t plugin = {};
    const clap_host_t* host = nullptr;
    PitchShiftProcessor processor;
    std::atomic<double> enabled {0.0};
    std::atomic<double> pitchSemitones {0.0};
    std::atomic<double> grainHz {8.0};
    std::atomic<double> shape {0.5};
    std::atomic<double> jitter {0.0};
    std::atomic<double> wet {1.0};
    std::atomic<double> prefilter {1.0};
};

const char* const kFeatures[] = {
    "audio-effect",
    "pitch-shifter",
    "stereo",
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION_INIT,
    kPluginId,
    "DONTFLOAT Pitch Shift",
    "DONTFLOAT",
    "https://github.com/ili4yov-ika/DONTFLOAT",
    nullptr,
    nullptr,
    "0.0.0.1",
    "Granular pitch shift powered by DONTFLOAT plugin core",
    kFeatures,
};

ClapPitchShift* self(const clap_plugin_t* plugin)
{
    return plugin ? static_cast<ClapPitchShift*>(plugin->plugin_data) : nullptr;
}

const ParamDef* findParam(clap_id id)
{
    for (const ParamDef& p : kParams) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

double clampParam(const ParamDef& def, double value)
{
    return std::clamp(value, def.minValue, def.maxValue);
}

double getAtomicParam(const ClapPitchShift& s, clap_id id)
{
    switch (id) {
    case kParamEnabled: return s.enabled.load(std::memory_order_relaxed);
    case kParamPitchSemitones: return s.pitchSemitones.load(std::memory_order_relaxed);
    case kParamGrainHz: return s.grainHz.load(std::memory_order_relaxed);
    case kParamShape: return s.shape.load(std::memory_order_relaxed);
    case kParamJitter: return s.jitter.load(std::memory_order_relaxed);
    case kParamWet: return s.wet.load(std::memory_order_relaxed);
    case kParamPrefilter: return s.prefilter.load(std::memory_order_relaxed);
    default: return 0.0;
    }
}

void setAtomicParam(ClapPitchShift& s, clap_id id, double value)
{
    const ParamDef* def = findParam(id);
    if (!def) {
        return;
    }
    value = clampParam(*def, value);
    switch (id) {
    case kParamEnabled: s.enabled.store(value, std::memory_order_relaxed); break;
    case kParamPitchSemitones: s.pitchSemitones.store(value, std::memory_order_relaxed); break;
    case kParamGrainHz: s.grainHz.store(value, std::memory_order_relaxed); break;
    case kParamShape: s.shape.store(value, std::memory_order_relaxed); break;
    case kParamJitter: s.jitter.store(value, std::memory_order_relaxed); break;
    case kParamWet: s.wet.store(value, std::memory_order_relaxed); break;
    case kParamPrefilter: s.prefilter.store(value, std::memory_order_relaxed); break;
    default: break;
    }
}

PitchShiftParams snapshotParams(const ClapPitchShift& s)
{
    PitchShiftParams p;
    p.enabled = s.enabled.load(std::memory_order_relaxed) >= 0.5;
    p.pitchSemitones = float(s.pitchSemitones.load(std::memory_order_relaxed));
    p.grainHz = float(s.grainHz.load(std::memory_order_relaxed));
    p.shape = float(s.shape.load(std::memory_order_relaxed));
    p.jitter = float(s.jitter.load(std::memory_order_relaxed));
    p.wet = float(s.wet.load(std::memory_order_relaxed));
    p.prefilter = s.prefilter.load(std::memory_order_relaxed) >= 0.5;
    return p;
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
    ClapPitchShift* s = self(plugin);
    if (!s || sampleRate <= 0.0) {
        return false;
    }
    s->processor.prepare(int(sampleRate), int(maxFrames));
    return true;
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
    if (ClapPitchShift* s = self(plugin)) {
        s->processor.reset();
    }
}

void applyInputEvents(ClapPitchShift& s, const clap_input_events_t* events)
{
    if (!events || !events->size || !events->get) {
        return;
    }
    const uint32_t count = events->size(events);
    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header_t* header = events->get(events, i);
        if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID
            || header->type != CLAP_EVENT_PARAM_VALUE
            || header->size < sizeof(clap_event_param_value_t)) {
            continue;
        }
        const auto* event = reinterpret_cast<const clap_event_param_value_t*>(header);
        setAtomicParam(s, event->param_id, event->value);
    }
}

clap_process_status pluginProcess(const clap_plugin_t* plugin, const clap_process_t* process)
{
    ClapPitchShift* s = self(plugin);
    if (!s || !process || process->frames_count == 0 || !process->audio_outputs) {
        return CLAP_PROCESS_ERROR;
    }

    applyInputEvents(*s, process->in_events);
    s->processor.setParams(snapshotParams(*s));

    const clap_audio_buffer_t& in = process->audio_inputs ? process->audio_inputs[0] : clap_audio_buffer_t{};
    clap_audio_buffer_t& out = process->audio_outputs[0];
    const float* inputPtrs[2] = {
        (in.data32 && in.channel_count > 0) ? in.data32[0] : nullptr,
        (in.data32 && in.channel_count > 1) ? in.data32[1] : nullptr,
    };
    float* outputPtrs[2] = {
        (out.data32 && out.channel_count > 0) ? out.data32[0] : nullptr,
        (out.data32 && out.channel_count > 1) ? out.data32[1] : nullptr,
    };
    s->processor.processReplacing(AudioBufferView{
        inputPtrs,
        outputPtrs,
        std::min<int>(int(in.channel_count), 2),
        std::min<int>(int(out.channel_count), 2),
        int(process->frames_count),
    });
    return CLAP_PROCESS_CONTINUE;
}

uint32_t paramsCount(const clap_plugin_t*)
{
    return uint32_t(sizeof(kParams) / sizeof(kParams[0]));
}

bool paramsGetInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info)
{
    if (!info || index >= paramsCount(nullptr)) {
        return false;
    }
    const ParamDef& def = kParams[index];
    *info = {};
    info->id = def.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    std::strncpy(info->name, def.name, sizeof(info->name) - 1);
    std::strncpy(info->module, "Pitch Shift", sizeof(info->module) - 1);
    info->min_value = def.minValue;
    info->max_value = def.maxValue;
    info->default_value = def.defaultValue;
    return true;
}

bool paramsGetValue(const clap_plugin_t* plugin, clap_id paramId, double* outValue)
{
    ClapPitchShift* s = self(plugin);
    if (!s || !outValue || !findParam(paramId)) {
        return false;
    }
    *outValue = getAtomicParam(*s, paramId);
    return true;
}

bool paramsValueToText(const clap_plugin_t*, clap_id paramId, double value,
                       char* outBuffer, uint32_t outBufferCapacity)
{
    if (!outBuffer || outBufferCapacity == 0 || !findParam(paramId)) {
        return false;
    }
    if (paramId == kParamEnabled || paramId == kParamPrefilter) {
        const char* text = value >= 0.5 ? "On" : "Off";
        std::strncpy(outBuffer, text, outBufferCapacity - 1);
        outBuffer[outBufferCapacity - 1] = '\0';
        return true;
    }
    const int written = std::snprintf(outBuffer, outBufferCapacity, "%.3f", value);
    return written > 0;
}

bool paramsTextToValue(const clap_plugin_t*, clap_id paramId, const char* text, double* outValue)
{
    const ParamDef* def = findParam(paramId);
    if (!def || !text || !outValue) {
        return false;
    }
    double value = def->defaultValue;
    const std::string_view view(text);
    const auto result = std::from_chars(view.data(), view.data() + view.size(), value);
    if (result.ec != std::errc()) {
        return false;
    }
    *outValue = clampParam(*def, value);
    return true;
}

void paramsFlush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t*)
{
    if (ClapPitchShift* s = self(plugin)) {
        applyInputEvents(*s, in);
    }
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
    std::strncpy(info->name, isInput ? "Input" : "Output", sizeof(info->name) - 1);
    return true;
}

const clap_plugin_params_t kParamsExtension = {
    paramsCount,
    paramsGetInfo,
    paramsGetValue,
    paramsValueToText,
    paramsTextToValue,
    paramsFlush,
};

const clap_plugin_audio_ports_t kAudioPortsExtension = {
    audioPortsCount,
    audioPortsGet,
};

const void* pluginGetExtension(const clap_plugin_t*, const char* id)
{
    if (!id) {
        return nullptr;
    }
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &kParamsExtension;
    }
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &kAudioPortsExtension;
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

    auto* s = new ClapPitchShift();
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
