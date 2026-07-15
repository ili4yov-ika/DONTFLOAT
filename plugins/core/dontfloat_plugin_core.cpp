#include "dontfloat_plugin_core.h"

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

int clampInt(int value, int minValue, int maxValue)
{
    return std::clamp(value, minValue, maxValue);
}

void rebuildMonoFromChannels(TrackAudioBuffer& buffer)
{
    buffer.mono.clear();
    if (buffer.left.empty()) {
        return;
    }
    const std::size_t frames = buffer.left.size();
    buffer.mono.resize(frames);
    if (buffer.right.size() == frames) {
        for (std::size_t i = 0; i < frames; ++i) {
            buffer.mono[i] = 0.5f * (buffer.left[i] + buffer.right[i]);
        }
    } else {
        buffer.mono = buffer.left;
    }
}

TrackAudioInfo audioInfoFromBuffer(const TrackAudioBuffer& buffer)
{
    TrackAudioInfo info;
    info.sampleRate = buffer.sampleRate;
    info.channelCount = std::max(1, buffer.channelCount);
    info.frameCount = buffer.frameCount();
    return info;
}

} // namespace

void TrackToolSession::reset()
{
    audioInfo_ = {};
    analysisOptions_ = {};
    analysis_ = {};
    alignment_ = {};
    renderOptions_ = {};
    markers_.clear();
    audioBuffer_ = {};
    pitchAnalysis_ = {};
    prepared_ = false;
    analysisValid_ = false;
    markersValid_ = false;
}

TrackToolStatus TrackToolSession::prepare(const TrackAudioInfo& audioInfo)
{
    return setAudioInfo(audioInfo);
}

TrackToolStatus TrackToolSession::setAudioInfo(const TrackAudioInfo& audioInfo)
{
    if (!isValidAudioInfo(audioInfo)) {
        prepared_ = false;
        analysisValid_ = false;
        markersValid_ = false;
        return TrackToolStatus::InvalidAudioInfo;
    }
    audioInfo_ = audioInfo;
    prepared_ = true;
    analysisValid_ = false;
    markersValid_ = !markers_.empty();
    return TrackToolStatus::Ok;
}

TrackToolStatus TrackToolSession::setAudioBuffer(const TrackAudioBuffer& buffer)
{
    if (buffer.sampleRate < 8000 || buffer.sampleRate > 384000 || buffer.mono.empty()) {
        return TrackToolStatus::InvalidAudioInfo;
    }

    audioBuffer_ = buffer;
    if (audioBuffer_.channelCount <= 0) {
        audioBuffer_.channelCount = audioBuffer_.right.empty() ? 1 : 2;
    }
    pitchAnalysis_ = {};
    return setAudioInfo(audioInfoFromBuffer(audioBuffer_));
}

TrackToolStatus TrackToolSession::appendHostFrames(const float* const* inputs,
                                                   int channelCount,
                                                   int frameCount)
{
    if (!inputs || channelCount <= 0 || frameCount <= 0) {
        return TrackToolStatus::InvalidAudioInfo;
    }

    const float* left = inputs[0];
    const float* right = channelCount > 1 ? inputs[1] : nullptr;
    if (!left) {
        return TrackToolStatus::InvalidAudioInfo;
    }

    if (audioBuffer_.sampleRate <= 0) {
        audioBuffer_.sampleRate = audioInfo_.sampleRate > 0 ? audioInfo_.sampleRate : 44100;
    }
    audioBuffer_.channelCount = std::max(audioBuffer_.channelCount, channelCount);

    const std::size_t oldSize = audioBuffer_.left.size();
    audioBuffer_.left.resize(oldSize + static_cast<std::size_t>(frameCount));
    if (right) {
        if (audioBuffer_.right.size() < oldSize) {
            audioBuffer_.right.resize(oldSize, 0.0f);
        }
        audioBuffer_.right.resize(oldSize + static_cast<std::size_t>(frameCount));
    }

    for (int i = 0; i < frameCount; ++i) {
        audioBuffer_.left[oldSize + static_cast<std::size_t>(i)] = left[i];
        if (right) {
            audioBuffer_.right[oldSize + static_cast<std::size_t>(i)] = right[i];
        }
    }

    rebuildMonoFromChannels(audioBuffer_);
    pitchAnalysis_ = {};
    return setAudioInfo(audioInfoFromBuffer(audioBuffer_));
}

void TrackToolSession::clearHostCapture()
{
    audioBuffer_ = {};
    pitchAnalysis_ = {};
    prepared_ = false;
    analysisValid_ = false;
}

TrackToolStatus TrackToolSession::analyze(const TrackAnalysisOptions& options, TrackAnalysisResult* result)
{
    if (!prepared_) {
        if (result) {
            *result = {};
            result->status = TrackToolStatus::NotPrepared;
        }
        return TrackToolStatus::NotPrepared;
    }

    analysisOptions_ = sanitizeAnalysisOptions(options);
    analysis_ = {};
    analysis_.status = TrackToolStatus::Ok;
    analysis_.isFixedTempo = analysisOptions_.assumeFixedTempo;
    analysis_.bpm = analysisOptions_.useInitialBpm ? analysisOptions_.initialBpm : 0.0f;
    analysis_.bpmConfidence = analysisOptions_.useInitialBpm ? 1.0f : 0.0f;
    analysis_.gridStartFrame = 0;
    analysis_.chroma.assign(12, 0.0f);

    analysisValid_ = true;
    if (result) {
        *result = analysis_;
    }
    return TrackToolStatus::Ok;
}

TrackToolStatus TrackToolSession::render(const TrackRenderRequest& request, TrackRenderResult* result)
{
    if (!prepared_) {
        if (result) {
            *result = {};
            result->status = TrackToolStatus::NotPrepared;
        }
        return TrackToolStatus::NotPrepared;
    }

    if (request.requestedFrameCount < 0) {
        if (result) {
            *result = {};
            result->status = TrackToolStatus::InvalidRenderRequest;
        }
        return TrackToolStatus::InvalidRenderRequest;
    }

    alignment_ = sanitizeAlignmentOptions(request.alignment);
    renderOptions_ = sanitizeRenderOptions(request.render);

    TrackRenderResult out;
    out.status = TrackToolStatus::Ok;
    out.framesRendered = request.requestedFrameCount > 0
        ? std::min(request.requestedFrameCount, audioInfo_.frameCount)
        : audioInfo_.frameCount;
    out.requiresOfflineRender = true;

    if (result) {
        *result = out;
    }
    return TrackToolStatus::Ok;
}

bool isValidAudioInfo(const TrackAudioInfo& audioInfo)
{
    return audioInfo.sampleRate >= 8000
        && audioInfo.sampleRate <= 384000
        && audioInfo.channelCount > 0
        && audioInfo.channelCount <= 64
        && audioInfo.frameCount >= 0;
}

TrackAnalysisOptions sanitizeAnalysisOptions(const TrackAnalysisOptions& options)
{
    TrackAnalysisOptions out = options;
    out.minBpm = clampFloat(out.minBpm, 20.0f, 400.0f);
    out.maxBpm = clampFloat(out.maxBpm, 20.0f, 400.0f);
    if (out.maxBpm < out.minBpm) {
        std::swap(out.minBpm, out.maxBpm);
    }
    out.initialBpm = clampFloat(out.initialBpm, 0.0f, 400.0f);
    if (out.useInitialBpm && out.initialBpm <= 0.0f) {
        out.useInitialBpm = false;
    }
    return out;
}

TrackAlignmentOptions sanitizeAlignmentOptions(const TrackAlignmentOptions& options)
{
    TrackAlignmentOptions out = options;
    out.targetBpm = clampFloat(out.targetBpm, 0.0f, 400.0f);
    out.beatsPerBar = clampInt(out.beatsPerBar, 1, 32);
    return out;
}

TrackRenderOptions sanitizeRenderOptions(const TrackRenderOptions& options)
{
    TrackRenderOptions out = options;
    out.pitchSemitones = clampFloat(out.pitchSemitones, -24.0f, 24.0f);
    return out;
}

} // namespace Dontfloat::PluginCore
