// VST3 starter implementation.
//
// This file intentionally depends on the official Steinberg VST3 SDK and is
// compiled only when DONTFLOAT_BUILD_VST3=ON and DONTFLOAT_VST3_SDK_ROOT is set.
// The CLAP/LV2 targets are self-contained; VST3 is SDK-gated because the public
// ABI is not small enough to mirror locally without risking host compatibility.

#include "../core/dontfloat_plugin_core.h"

#include <algorithm>

#if defined(DONTFLOAT_HAS_VST3_SDK)
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace Dontfloat::Vst3 {

using Dontfloat::PluginCore::AudioBufferView;
using Dontfloat::PluginCore::PitchShiftParams;
using Dontfloat::PluginCore::PitchShiftProcessor;

enum ParamId : Steinberg::Vst::ParamID {
    kParamEnabled = 1,
    kParamPitchSemitones = 2,
    kParamGrainHz = 3,
    kParamShape = 4,
    kParamJitter = 5,
    kParamWet = 6,
    kParamPrefilter = 7,
};

class PitchShiftProcessorVst3 final : public Steinberg::Vst::AudioEffect {
public:
    static Steinberg::FUnknown* createInstance(void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new PitchShiftProcessorVst3());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override
    {
        const Steinberg::tresult result = AudioEffect::initialize(context);
        if (result != Steinberg::kResultOk) {
            return result;
        }

        addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override
    {
        if (state) {
            core_.reset();
        }
        return AudioEffect::setActive(state);
    }

    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override
    {
        core_.prepare(int(setup.sampleRate), int(setup.maxSamplesPerBlock));
        return AudioEffect::setupProcessing(setup);
    }

    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override
    {
        applyParameterChanges(data.inputParameterChanges);

        if (data.numSamples <= 0 || data.numOutputs <= 0 || data.outputs[0].numChannels <= 0) {
            return Steinberg::kResultOk;
        }

        core_.setParams(params_);
        const float* inputs[2] = {
            data.numInputs > 0 && data.inputs[0].numChannels > 0 ? data.inputs[0].channelBuffers32[0] : nullptr,
            data.numInputs > 0 && data.inputs[0].numChannels > 1 ? data.inputs[0].channelBuffers32[1] : nullptr,
        };
        float* outputs[2] = {
            data.outputs[0].numChannels > 0 ? data.outputs[0].channelBuffers32[0] : nullptr,
            data.outputs[0].numChannels > 1 ? data.outputs[0].channelBuffers32[1] : nullptr,
        };

        core_.processReplacing(AudioBufferView{
            inputs,
            outputs,
            data.numInputs > 0 ? std::min<int>(data.inputs[0].numChannels, 2) : 0,
            std::min<int>(data.outputs[0].numChannels, 2),
            data.numSamples,
        });
        return Steinberg::kResultOk;
    }

private:
    void applyParameterChanges(Steinberg::Vst::IParameterChanges* changes)
    {
        if (!changes) {
            return;
        }

        const Steinberg::int32 queueCount = changes->getParameterCount();
        for (Steinberg::int32 i = 0; i < queueCount; ++i) {
            Steinberg::Vst::IParamValueQueue* queue = changes->getParameterData(i);
            if (!queue || queue->getPointCount() <= 0) {
                continue;
            }

            Steinberg::int32 sampleOffset = 0;
            Steinberg::Vst::ParamValue value = 0.0;
            if (queue->getPoint(queue->getPointCount() - 1, sampleOffset, value) != Steinberg::kResultOk) {
                continue;
            }

            switch (queue->getParameterId()) {
            case kParamEnabled: params_.enabled = value >= 0.5; break;
            case kParamPitchSemitones: params_.pitchSemitones = float(value * 48.0 - 24.0); break;
            case kParamGrainHz: params_.grainHz = float(4.0 + value * 36.0); break;
            case kParamShape: params_.shape = float(value); break;
            case kParamJitter: params_.jitter = float(value); break;
            case kParamWet: params_.wet = float(value); break;
            case kParamPrefilter: params_.prefilter = value >= 0.5; break;
            default: break;
            }
        }
    }

    PitchShiftProcessor core_;
    PitchShiftParams params_;
};

} // namespace Dontfloat::Vst3
#endif // DONTFLOAT_HAS_VST3_SDK
