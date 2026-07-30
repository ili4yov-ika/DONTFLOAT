#include "lv2_minimal.h"
#include "../core/dontfloat_plugin_core.h"
#include "../core/plugin_host_config.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::TrackToolStatus;
using Dontfloat::PluginHost::desc;
using Dontfloat::PluginHost::product;
using Dontfloat::PluginCore::sharedSession;

namespace {

enum PortIndex : uint32_t {
    kPortAudioInL = 0,
    kPortAudioInR,
    kPortAudioOutL,
    kPortAudioOutR,
};

struct Lv2PluginInstance {
    TrackToolSession session;

    const float* inL = nullptr;
    const float* inR = nullptr;
    float* outL = nullptr;
    float* outR = nullptr;

    int sampleRate = 44100;
};

void copyChannel(const float* input, float* output, uint32_t sampleCount)
{
    if (!output) {
        return;
    }
    if (input) {
        if (input != output) {
            std::copy(input, input + sampleCount, output);
        }
    } else {
        std::fill(output, output + sampleCount, 0.0f);
    }
}

LV2_Handle instantiate(const LV2_Descriptor* descriptor, double sampleRate,
                       const char* /*bundlePath*/, const LV2_Feature* const*)
{
    // Third argument is bundle_path (filesystem), NOT the plugin URI.
    // Hosts like Reaper pass e.g. "...\dontfloat.lv2\" here; comparing it to
    // the URI made instantiate() return nullptr → "plugin is damaged".
    if (!descriptor || !descriptor->URI
        || std::strcmp(descriptor->URI, desc().lv2Uri) != 0) {
        return nullptr;
    }

    auto* self = new Lv2PluginInstance();
    self->sampleRate = std::max(1, int(sampleRate));
    self->session.prepare(TrackAudioInfo{self->sampleRate, 2, 0});
    return self;
}

void connectPort(LV2_Handle instance, uint32_t port, void* data)
{
    auto* self = static_cast<Lv2PluginInstance*>(instance);
    if (!self) {
        return;
    }

    switch (port) {
    case kPortAudioInL: self->inL = static_cast<const float*>(data); break;
    case kPortAudioInR: self->inR = static_cast<const float*>(data); break;
    case kPortAudioOutL: self->outL = static_cast<float*>(data); break;
    case kPortAudioOutR: self->outR = static_cast<float*>(data); break;
    default: break;
    }
}

void activate(LV2_Handle instance)
{
    if (auto* self = static_cast<Lv2PluginInstance*>(instance)) {
        self->session.prepare(TrackAudioInfo{self->sampleRate, 2, 0});
    }
}

void run(LV2_Handle instance, uint32_t sampleCount)
{
    auto* self = static_cast<Lv2PluginInstance*>(instance);
    if (!self || sampleCount == 0) {
        return;
    }

    copyChannel(self->inL, self->outL, sampleCount);
    copyChannel(self->inR ? self->inR : self->inL, self->outR, sampleCount);

    const float* inputs[2] = { self->inL, self->inR ? self->inR : self->inL };
    self->session.appendHostFrames(inputs, 2, int(sampleCount));
    sharedSession(product()) = self->session;
}

void deactivate(LV2_Handle) {}

void cleanup(LV2_Handle instance) { delete static_cast<Lv2PluginInstance*>(instance); }

const void* extensionData(const char*) { return nullptr; }

const LV2_Descriptor kDescriptor = {
    desc().lv2Uri,
    instantiate,
    connectPort,
    activate,
    run,
    deactivate,
    cleanup,
    extensionData,
};

} // namespace

extern "C" {
LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &kDescriptor : nullptr;
}
}
