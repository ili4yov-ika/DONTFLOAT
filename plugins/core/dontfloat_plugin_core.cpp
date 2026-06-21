#include "dontfloat_plugin_core.h"

#include "../../include/granularpitchshifter_engine.h"

#include <algorithm>
#include <cmath>

namespace Dontfloat::PluginCore {
namespace {

float clampFloat(float value, float minValue, float maxValue)
{
    if (!std::isfinite(value)) {
        return minValue;
    }
    return std::clamp(value, minValue, maxValue);
}

void copyOrClear(const AudioBufferView& buffer)
{
    const int outputChannels = std::max(0, buffer.outputChannelCount);
    const int inputChannels = std::max(0, buffer.inputChannelCount);

    for (int ch = 0; ch < outputChannels; ++ch) {
        float* out = buffer.outputs ? buffer.outputs[ch] : nullptr;
        if (!out) {
            continue;
        }

        const float* in = (buffer.inputs && ch < inputChannels) ? buffer.inputs[ch] : nullptr;
        if (in) {
            if (in != out) {
                std::copy(in, in + buffer.frameCount, out);
            }
        } else {
            std::fill(out, out + buffer.frameCount, 0.0f);
        }
    }
}

} // namespace

class PitchShiftProcessor::Impl {
public:
    GranularEngine::Engine engine;
};

PitchShiftProcessor::PitchShiftProcessor()
    : impl_(new Impl())
{
}

PitchShiftProcessor::~PitchShiftProcessor()
{
    delete impl_;
}

void PitchShiftProcessor::prepare(int sampleRate, int maxBlockSize)
{
    sampleRate_ = std::max(1, sampleRate);
    maxBlockSize_ = std::max(0, maxBlockSize);
    reset();
    prepared_ = true;
}

void PitchShiftProcessor::reset()
{
    const int ringReferenceSamples = std::max(sampleRate_, maxBlockSize_);
    impl_->engine.init(sampleRate_, ringReferenceSamples);
}

void PitchShiftProcessor::setParams(const PitchShiftParams& params)
{
    params_ = sanitizePitchShiftParams(params);
}

void PitchShiftProcessor::processReplacing(const AudioBufferView& buffer)
{
    if (!buffer.outputs || buffer.outputChannelCount <= 0 || buffer.frameCount <= 0) {
        return;
    }

    if (!prepared_) {
        prepare(sampleRate_, buffer.frameCount);
    }

    const PitchShiftParams p = sanitizePitchShiftParams(params_);
    if (!p.enabled || std::abs(p.pitchSemitones) < 0.0001f || p.wet <= 0.0f) {
        copyOrClear(buffer);
        return;
    }

    const float dry = 1.0f - p.wet;
    const double pitchSpeed = std::exp2(p.pitchSemitones / 12.0);

    for (int i = 0; i < buffer.frameCount; ++i) {
        const float inL = (buffer.inputs && buffer.inputChannelCount > 0 && buffer.inputs[0])
            ? buffer.inputs[0][i]
            : 0.0f;
        const float inR = (buffer.inputs && buffer.inputChannelCount > 1 && buffer.inputs[1])
            ? buffer.inputs[1][i]
            : inL;

        float shiftedL = 0.0f;
        float shiftedR = 0.0f;
        impl_->engine.process(inL, inR, p.grainHz, pitchSpeed, p.shape, p.jitter,
                              p.prefilter, shiftedL, shiftedR);

        if (buffer.outputs[0]) {
            buffer.outputs[0][i] = dry * inL + p.wet * shiftedL;
        }
        if (buffer.outputChannelCount > 1 && buffer.outputs[1]) {
            buffer.outputs[1][i] = dry * inR + p.wet * shiftedR;
        }
        for (int ch = 2; ch < buffer.outputChannelCount; ++ch) {
            if (!buffer.outputs[ch]) {
                continue;
            }
            const float* in = (buffer.inputs && ch < buffer.inputChannelCount) ? buffer.inputs[ch] : nullptr;
            buffer.outputs[ch][i] = in ? in[i] : 0.0f;
        }
    }
}

PitchShiftParams sanitizePitchShiftParams(const PitchShiftParams& params)
{
    PitchShiftParams out = params;
    out.pitchSemitones = clampFloat(out.pitchSemitones, -24.0f, 24.0f);
    out.grainHz = clampFloat(out.grainHz, 4.0f, 40.0f);
    out.shape = clampFloat(out.shape, 0.0f, 1.0f);
    out.jitter = clampFloat(out.jitter, 0.0f, 1.0f);
    out.wet = clampFloat(out.wet, 0.0f, 1.0f);
    return out;
}

bool isFiniteBuffer(const AudioBufferView& buffer)
{
    if (!buffer.outputs || buffer.outputChannelCount <= 0 || buffer.frameCount < 0) {
        return false;
    }
    for (int ch = 0; ch < buffer.outputChannelCount; ++ch) {
        const float* out = buffer.outputs[ch];
        if (!out) {
            continue;
        }
        for (int i = 0; i < buffer.frameCount; ++i) {
            if (!std::isfinite(out[i])) {
                return false;
            }
        }
    }
    return true;
}

} // namespace Dontfloat::PluginCore
