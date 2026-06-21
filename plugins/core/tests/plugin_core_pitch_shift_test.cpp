#include "../dontfloat_plugin_core.h"

#include <cmath>
#include <iostream>
#include <vector>

using Dontfloat::PluginCore::AudioBufferView;
using Dontfloat::PluginCore::PitchShiftParams;
using Dontfloat::PluginCore::PitchShiftProcessor;
using Dontfloat::PluginCore::isFiniteBuffer;

namespace {

bool nearlyEqual(float a, float b, float eps = 1.0e-6f)
{
    return std::abs(a - b) <= eps;
}

bool testBypassCopiesInput()
{
    constexpr int frames = 128;
    std::vector<float> inL(frames);
    std::vector<float> inR(frames);
    std::vector<float> outL(frames, -1.0f);
    std::vector<float> outR(frames, -1.0f);

    for (int i = 0; i < frames; ++i) {
        inL[i] = std::sin(float(i) * 0.05f);
        inR[i] = std::cos(float(i) * 0.05f);
    }

    const float* inputs[] = {inL.data(), inR.data()};
    float* outputs[] = {outL.data(), outR.data()};

    PitchShiftProcessor processor;
    processor.prepare(44100, frames);
    processor.setParams(PitchShiftParams{});
    processor.processReplacing(AudioBufferView{inputs, outputs, 2, 2, frames});

    for (int i = 0; i < frames; ++i) {
        if (!nearlyEqual(outL[i], inL[i]) || !nearlyEqual(outR[i], inR[i])) {
            return false;
        }
    }
    return true;
}

bool testEnabledProducesFiniteStereo()
{
    constexpr int frames = 512;
    std::vector<float> inL(frames);
    std::vector<float> inR(frames);
    std::vector<float> outL(frames, 0.0f);
    std::vector<float> outR(frames, 0.0f);

    for (int i = 0; i < frames; ++i) {
        inL[i] = std::sin(float(i) * 0.03f);
        inR[i] = std::sin(float(i) * 0.07f);
    }

    const float* inputs[] = {inL.data(), inR.data()};
    float* outputs[] = {outL.data(), outR.data()};

    PitchShiftParams params;
    params.enabled = true;
    params.pitchSemitones = 7.0f;
    params.grainHz = 12.0f;
    params.shape = 0.5f;
    params.jitter = 0.1f;
    params.wet = 0.75f;
    params.prefilter = true;

    PitchShiftProcessor processor;
    processor.prepare(44100, frames);
    processor.setParams(params);
    processor.processReplacing(AudioBufferView{inputs, outputs, 2, 2, frames});

    return isFiniteBuffer(AudioBufferView{inputs, outputs, 2, 2, frames});
}

bool testMonoToStereo()
{
    constexpr int frames = 128;
    std::vector<float> in(frames, 0.25f);
    std::vector<float> outL(frames, 0.0f);
    std::vector<float> outR(frames, 0.0f);

    const float* inputs[] = {in.data()};
    float* outputs[] = {outL.data(), outR.data()};

    PitchShiftProcessor processor;
    processor.prepare(48000, frames);
    PitchShiftParams params;
    params.enabled = false;
    processor.setParams(params);
    processor.processReplacing(AudioBufferView{inputs, outputs, 1, 2, frames});

    for (int i = 0; i < frames; ++i) {
        if (!nearlyEqual(outL[i], in[i]) || !nearlyEqual(outR[i], 0.0f)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    if (!testBypassCopiesInput()) {
        std::cerr << "testBypassCopiesInput failed\n";
        return 1;
    }
    if (!testEnabledProducesFiniteStereo()) {
        std::cerr << "testEnabledProducesFiniteStereo failed\n";
        return 1;
    }
    if (!testMonoToStereo()) {
        std::cerr << "testMonoToStereo failed\n";
        return 1;
    }
    return 0;
}
