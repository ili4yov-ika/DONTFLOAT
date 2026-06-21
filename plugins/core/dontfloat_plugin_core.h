#ifndef DONTFLOAT_PLUGIN_CORE_H
#define DONTFLOAT_PLUGIN_CORE_H

#include <cstdint>

namespace Dontfloat::PluginCore {

struct AudioBufferView {
    const float* const* inputs = nullptr;
    float* const* outputs = nullptr;
    int inputChannelCount = 0;
    int outputChannelCount = 0;
    int frameCount = 0;
};

struct PitchShiftParams {
    bool enabled = false;
    float pitchSemitones = 0.0f;
    float grainHz = 8.0f;
    float shape = 0.5f;
    float jitter = 0.0f;
    float wet = 1.0f;
    bool prefilter = true;
};

class PitchShiftProcessor {
public:
    PitchShiftProcessor();
    ~PitchShiftProcessor();

    PitchShiftProcessor(const PitchShiftProcessor&) = delete;
    PitchShiftProcessor& operator=(const PitchShiftProcessor&) = delete;

    void prepare(int sampleRate, int maxBlockSize);
    void reset();

    void setParams(const PitchShiftParams& params);
    const PitchShiftParams& params() const { return params_; }

    void processReplacing(const AudioBufferView& buffer);

    int sampleRate() const { return sampleRate_; }
    int maxBlockSize() const { return maxBlockSize_; }
    bool isPrepared() const { return prepared_; }

private:
    class Impl;

    PitchShiftParams params_;
    Impl* impl_ = nullptr;
    int sampleRate_ = 44100;
    int maxBlockSize_ = 0;
    bool prepared_ = false;
};

PitchShiftParams sanitizePitchShiftParams(const PitchShiftParams& params);
bool isFiniteBuffer(const AudioBufferView& buffer);

} // namespace Dontfloat::PluginCore

#endif // DONTFLOAT_PLUGIN_CORE_H
