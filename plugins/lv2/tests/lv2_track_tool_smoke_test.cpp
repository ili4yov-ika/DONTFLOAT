#include "../lv2_minimal.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
const LV2_Descriptor* lv2_descriptor(uint32_t index);
}

namespace {

bool isFinite(const std::vector<float>& values)
{
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

bool nearlyEqual(float a, float b)
{
    return std::abs(a - b) <= 1.0e-6f;
}

} // namespace

int main()
{
    const LV2_Descriptor* descriptor = lv2_descriptor(0);
    if (!descriptor || std::strcmp(descriptor->URI, "https://github.com/ili4yov-ika/DONTFLOAT/plugins/track-tool") != 0) {
        std::cerr << "descriptor mismatch\n";
        return 1;
    }
    if (lv2_descriptor(1) != nullptr) {
        std::cerr << "unexpected second descriptor\n";
        return 1;
    }

    LV2_Handle instance = descriptor->instantiate(descriptor, 44100.0, nullptr, nullptr);
    if (!instance) {
        std::cerr << "instantiate failed\n";
        return 1;
    }

    constexpr uint32_t frames = 128;
    std::vector<float> inL(frames);
    std::vector<float> inR(frames);
    std::vector<float> outL(frames, 0.0f);
    std::vector<float> outR(frames, 0.0f);
    for (uint32_t i = 0; i < frames; ++i) {
        inL[i] = std::sin(float(i) * 0.02f);
        inR[i] = std::cos(float(i) * 0.05f);
    }

    descriptor->connect_port(instance, 0, inL.data());
    descriptor->connect_port(instance, 1, inR.data());
    descriptor->connect_port(instance, 2, outL.data());
    descriptor->connect_port(instance, 3, outR.data());

    descriptor->activate(instance);
    descriptor->run(instance, frames);
    descriptor->deactivate(instance);
    descriptor->cleanup(instance);

    if (!isFinite(outL) || !isFinite(outR)) {
        std::cerr << "non-finite output\n";
        return 1;
    }
    for (uint32_t i = 0; i < frames; ++i) {
        if (!nearlyEqual(outL[i], inL[i]) || !nearlyEqual(outR[i], inR[i])) {
            std::cerr << "passthrough mismatch\n";
            return 1;
        }
    }
    return 0;
}
