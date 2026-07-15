#include "lv2_minimal.h"
#include "../core/dontfloat_plugin_core.h"

#include <algorithm>
#include <cstdint>

using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::sharedTrackToolSession;

namespace {

constexpr const char* kPluginUri = "https://github.com/ili4yov-ika/DONTFLOAT/plugins/track-tool";

enum PortIndex : uint32_t {
    kPortAudioInL = 0,
    kPortAudioInR,
    kPortAudioOutL,
    kPortAudioOutR,
};

struct Lv2TrackTool {
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

LV2_Handle instantiate(const LV2_Descriptor*, double sampleRate, const char*, const LV2_Feature* const*)
{
    auto* self = new Lv2TrackTool();
    self->sampleRate = std::max(1, int(sampleRate));
    self->session.prepare(TrackAudioInfo{self->sampleRate, 2, 0});
    return self;
}

void connectPort(LV2_Handle instance, uint32_t port, void* data)
{
    auto* self = static_cast<Lv2TrackTool*>(instance);
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
    if (auto* self = static_cast<Lv2TrackTool*>(instance)) {
        self->session.prepare(TrackAudioInfo{self->sampleRate, 2, 0});
    }
}

void run(LV2_Handle instance, uint32_t sampleCount)
{
    auto* self = static_cast<Lv2TrackTool*>(instance);
    if (!self || sampleCount == 0) {
        return;
    }

    copyChannel(self->inL, self->outL, sampleCount);
    copyChannel(self->inR ? self->inR : self->inL, self->outR, sampleCount);

    const float* inputs[2] = { self->inL, self->inR ? self->inR : self->inL };
    sharedTrackToolSession().appendHostFrames(inputs, 2, int(sampleCount));
}

void deactivate(LV2_Handle)
{
}

void cleanup(LV2_Handle instance)
{
    delete static_cast<Lv2TrackTool*>(instance);
}

const void* extensionData(const char*)
{
    return nullptr;
}

const LV2_Descriptor kDescriptor = {
    kPluginUri,
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
