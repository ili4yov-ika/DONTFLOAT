#include "dontfloat_plugin_core.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
    // Хост без транспорта (LV2): блок ложится в конец захвата
    return writeHostFrames(inputs, channelCount, frameCount, -1);
}

TrackToolStatus TrackToolSession::writeHostFrames(const float* const* inputs,
                                                  int channelCount,
                                                  int frameCount,
                                                  std::int64_t timelineFrame)
{
    if (!inputs || channelCount <= 0 || frameCount <= 0) {
        return TrackToolStatus::InvalidAudioInfo;
    }

    const float* left = inputs[0];
    const float* right = channelCount > 1 ? inputs[1] : nullptr;
    if (!left) {
        return TrackToolStatus::InvalidAudioInfo;
    }

    // Host-captured audio must use the sample rate the plugin was activated with,
    // not the TrackAudioBuffer default (44100). Otherwise analysis (BPM/pitch)
    // runs at the wrong rate inside a DAW.
    if (audioInfo_.sampleRate > 0) {
        audioBuffer_.sampleRate = audioInfo_.sampleRate;
    } else if (audioBuffer_.sampleRate <= 0) {
        audioBuffer_.sampleRate = 44100;
    }
    audioBuffer_.channelCount = std::max(audioBuffer_.channelCount, channelCount);

    // Куда писать: по позиции таймлайна или в конец захвата
    std::int64_t writeStart = timelineFrame;
    if (writeStart < 0) {
        writeStart = static_cast<std::int64_t>(audioBuffer_.left.size());
    } else {
        // Хост отдал позицию далеко за концом (перемотка в пустоту) — не
        // раздуваем буфер тишиной, пишем в конец
        constexpr std::int64_t kMaxGapFrames = 60LL * 384000LL;  // минута на максимальной частоте
        // Небольшой разрыв между блоками — округления хоста, а не новый проход
        constexpr std::int64_t kPassGapFrames = 64;
        const std::int64_t gap = writeStart - static_cast<std::int64_t>(audioBuffer_.left.size());
        if (gap > kMaxGapFrames) {
            writeStart = static_cast<std::int64_t>(audioBuffer_.left.size());
        } else if (!audioBuffer_.left.empty() && std::llabs(writeStart - lastWriteEndFrame_) > kPassGapFrames) {
            // Блок пришёл не следом за предыдущим — DAW начала новый проход
            // (перемотка, повтор, перенос клипа). Старый захват выбрасываем:
            // иначе прошлый проход остался бы висеть на прежнем месте.
            audioBuffer_.left.clear();
            audioBuffer_.right.clear();
            audioBuffer_.mono.clear();
        }
    }

    const std::size_t writeIndex = static_cast<std::size_t>(writeStart);
    const std::size_t requiredSize = writeIndex + static_cast<std::size_t>(frameCount);
    if (audioBuffer_.left.size() < requiredSize) {
        audioBuffer_.left.resize(requiredSize, 0.0f);
    }
    if (right && audioBuffer_.right.size() < requiredSize) {
        audioBuffer_.right.resize(requiredSize, 0.0f);
    }

    for (int i = 0; i < frameCount; ++i) {
        audioBuffer_.left[writeIndex + static_cast<std::size_t>(i)] = left[i];
        if (right) {
            audioBuffer_.right[writeIndex + static_cast<std::size_t>(i)] = right[i];
        }
    }
    lastWriteEndFrame_ = static_cast<std::int64_t>(requiredSize);

    rebuildMonoFromChannels(audioBuffer_);
    pitchAnalysis_ = {};
    return setAudioInfo(audioInfoFromBuffer(audioBuffer_));
}

void TrackToolSession::clearHostCapture()
{
    audioBuffer_ = {};
    pitchAnalysis_ = {};
    lastWriteEndFrame_ = 0;
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

TrackContentFingerprint computeContentFingerprint(const TrackAudioBuffer& buffer)
{
    TrackContentFingerprint print;
    const std::vector<float>& mono = buffer.mono;
    if (mono.empty()) {
        return print;
    }

    // Тишину по краям отбрасываем: клип в DAW окружён пустотой дорожки
    constexpr float kSilence = 1.0e-4f;
    std::int64_t first = 0;
    const std::int64_t total = static_cast<std::int64_t>(mono.size());
    while (first < total && std::fabs(mono[static_cast<std::size_t>(first)]) <= kSilence) {
        ++first;
    }
    if (first >= total) {
        return print;  // одна тишина
    }
    std::int64_t last = total - 1;
    while (last > first && std::fabs(mono[static_cast<std::size_t>(last)]) <= kSilence) {
        --last;
    }

    print.startFrame = first;
    print.lengthFrames = last - first + 1;

    // FNV-1a по содержимому с постоянным числом точек: хеш не зависит ни от
    // позиции клипа, ни от длины буфера вокруг него
    constexpr int kProbeCount = 4096;
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    std::uint64_t hash = kFnvOffset;
    for (int probe = 0; probe < kProbeCount; ++probe) {
        const std::int64_t offset =
            print.lengthFrames <= 1
                ? 0
                : (print.lengthFrames - 1) * probe / (kProbeCount - 1);
        const float sample = mono[static_cast<std::size_t>(first + offset)];
        // Квантование до 16 бит: мелкая арифметическая разница не меняет хеш
        const auto quantized = static_cast<std::int32_t>(std::lround(sample * 32767.0f));
        hash = (hash ^ static_cast<std::uint64_t>(quantized & 0xFFFF)) * kFnvPrime;
    }
    print.hash = hash;
    return print;
}

bool detectContentShift(const TrackContentFingerprint& before,
                        const TrackContentFingerprint& after,
                        std::int64_t* deltaFrames)
{
    if (before.empty() || after.empty()) {
        return false;
    }
    if (before.hash != after.hash || before.lengthFrames != after.lengthFrames) {
        return false;  // другой материал — нужен полный анализ
    }
    if (before.startFrame == after.startFrame) {
        return false;  // ничего не двигали
    }
    if (deltaFrames) {
        *deltaFrames = after.startFrame - before.startFrame;
    }
    return true;
}

} // namespace Dontfloat::PluginCore
