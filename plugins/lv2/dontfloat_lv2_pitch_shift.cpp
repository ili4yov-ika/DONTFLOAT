#include "lv2_minimal.h"
#include "../core/dontfloat_plugin_core.h"

#include <algorithm>
#include <cstdint>

using Dontfloat::PluginCore::AudioBufferView;
using Dontfloat::PluginCore::PitchShiftParams;
using Dontfloat::PluginCore::PitchShiftProcessor;

namespace {

constexpr const char* kPluginUri = "https://github.com/ili4yov-ika/DONTFLOAT/plugins/pitch-shift";

enum PortIndex : uint32_t {
    kPortAudioInL = 0,
    kPortAudioInR,
    kPortAudioOutL,
    kPortAudioOutR,
    kPortEnabled,
    kPortPitchSemitones,
    kPortGrainHz,
    kPortShape,
    kPortJitter,
    kPortWet,
    kPortPrefilter,
};

struct Lv2PitchShift {
    PitchShiftProcessor processor;

    const float* inL = nullptr;
    const float* inR = nullptr;
    float* outL = nullptr;
    float* outR = nullptr;

    const float* enabled = nullptr;
    const float* pitchSemitones = nullptr;
    const float* grainHz = nullptr;
    const float* shape = nullptr;
    const float* jitter = nullptr;
    const float* wet = nullptr;
    const float* prefilter = nullptr;

    int sampleRate = 44100;
};

float readControl(const float* value, float fallback)
{
    return value ? *value : fallback;
}

PitchShiftParams readParams(const Lv2PitchShift& self)
{
    PitchShiftParams params;
    params.enabled = readControl(self.enabled, 0.0f) >= 0.5f;
    params.pitchSemitones = readControl(self.pitchSemitones, 0.0f);
    params.grainHz = readControl(self.grainHz, 8.0f);
    params.shape = readControl(self.shape, 0.5f);
    params.jitter = readControl(self.jitter, 0.0f);
    params.wet = readControl(self.wet, 1.0f);
    params.prefilter = readControl(self.prefilter, 1.0f) >= 0.5f;
    return params;
}

LV2_Handle instantiate(const LV2_Descriptor*, double sampleRate, const char*, const LV2_Feature* const*)
{
    auto* self = new Lv2PitchShift();
    self->sampleRate = std::max(1, int(sampleRate));
    self->processor.prepare(self->sampleRate, 4096);
    return self;
}

void connectPort(LV2_Handle instance, uint32_t port, void* data)
{
    auto* self = static_cast<Lv2PitchShift*>(instance);
    if (!self) {
        return;
    }

    switch (port) {
    case kPortAudioInL: self->inL = static_cast<const float*>(data); break;
    case kPortAudioInR: self->inR = static_cast<const float*>(data); break;
    case kPortAudioOutL: self->outL = static_cast<float*>(data); break;
    case kPortAudioOutR: self->outR = static_cast<float*>(data); break;
    case kPortEnabled: self->enabled = static_cast<const float*>(data); break;
    case kPortPitchSemitones: self->pitchSemitones = static_cast<const float*>(data); break;
    case kPortGrainHz: self->grainHz = static_cast<const float*>(data); break;
    case kPortShape: self->shape = static_cast<const float*>(data); break;
    case kPortJitter: self->jitter = static_cast<const float*>(data); break;
    case kPortWet: self->wet = static_cast<const float*>(data); break;
    case kPortPrefilter: self->prefilter = static_cast<const float*>(data); break;
    default: break;
    }
}

void activate(LV2_Handle instance)
{
    if (auto* self = static_cast<Lv2PitchShift*>(instance)) {
        self->processor.reset();
    }
}

void run(LV2_Handle instance, uint32_t sampleCount)
{
    auto* self = static_cast<Lv2PitchShift*>(instance);
    if (!self || sampleCount == 0) {
        return;
    }

    self->processor.setParams(readParams(*self));

    const float* inputs[] = {self->inL, self->inR ? self->inR : self->inL};
    float* outputs[] = {self->outL, self->outR};
    self->processor.processReplacing(AudioBufferView{
        inputs,
        outputs,
        2,
        self->outR ? 2 : 1,
        int(sampleCount),
    });
}

void deactivate(LV2_Handle)
{
}

void cleanup(LV2_Handle instance)
{
    delete static_cast<Lv2PitchShift*>(instance);
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
