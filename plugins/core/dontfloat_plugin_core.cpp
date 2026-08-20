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

// ============================================================================
// HostCaptureQueue — мост между аудиопотоком и потоком интерфейса
// ============================================================================

HostCaptureQueue::HostCaptureQueue()
    : samples_(kSampleCapacity, 0.0f)
    , headers_(kHeaderCapacity)
{
}

std::size_t HostCaptureQueue::freeSamples(std::size_t writePos, std::size_t readPos) const noexcept
{
    // Одну ячейку держим свободной, иначе полное кольцо неотличимо от пустого
    const std::size_t used = (writePos + kSampleCapacity - readPos) % kSampleCapacity;
    return kSampleCapacity - used - 1u;
}

bool HostCaptureQueue::push(const float* const* inputs, int channelCount, int frameCount,
                            std::int64_t timelineFrame) noexcept
{
    if (!inputs || !inputs[0] || channelCount <= 0 || frameCount <= 0) {
        return false;
    }
    // Больше двух каналов захват не хранит — буфер сессии стерео
    const int channels = channelCount > 2 ? 2 : channelCount;
    const std::size_t needed = static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(channels);

    const std::size_t headerWrite = headerWrite_.load(std::memory_order_relaxed);
    const std::size_t headerNext = (headerWrite + 1u) % kHeaderCapacity;
    if (headerNext == headerRead_.load(std::memory_order_acquire)) {
        overflow_.store(true, std::memory_order_relaxed);
        return false;  // очередь заголовков полна
    }

    const std::size_t sampleWrite = sampleWrite_.load(std::memory_order_relaxed);
    if (needed > freeSamples(sampleWrite, sampleRead_.load(std::memory_order_acquire))) {
        overflow_.store(true, std::memory_order_relaxed);
        return false;  // не хватает места под сэмплы
    }

    // Каналы кладём чередуя: так блок занимает один непрерывный кусок кольца
    std::size_t pos = sampleWrite;
    for (int i = 0; i < frameCount; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            const float* src = inputs[ch];
            samples_[pos] = src ? src[i] : 0.0f;
            pos = (pos + 1u) % kSampleCapacity;
        }
    }

    Header& header = headers_[headerWrite];
    header.timelineFrame = timelineFrame;
    header.channelCount = channels;
    header.frameCount = frameCount;
    header.sampleOffset = sampleWrite;

    // Сначала двигаем сэмплы, потом публикуем заголовок: читатель, увидев
    // заголовок, обязан увидеть и сэмплы под ним
    sampleWrite_.store(pos, std::memory_order_release);
    headerWrite_.store(headerNext, std::memory_order_release);
    return true;
}

bool HostCaptureQueue::pop(Block& out)
{
    const std::size_t headerRead = headerRead_.load(std::memory_order_relaxed);
    if (headerRead == headerWrite_.load(std::memory_order_acquire)) {
        return false;
    }

    const Header header = headers_[headerRead];
    out.timelineFrame = header.timelineFrame;
    out.channelCount = header.channelCount;
    out.frameCount = header.frameCount;

    const std::size_t frames = static_cast<std::size_t>(header.frameCount);
    out.left.resize(frames);
    if (header.channelCount > 1) {
        out.right.resize(frames);
    } else {
        out.right.clear();
    }

    std::size_t pos = static_cast<std::size_t>(header.sampleOffset);
    for (std::size_t i = 0; i < frames; ++i) {
        out.left[i] = samples_[pos];
        pos = (pos + 1u) % kSampleCapacity;
        if (header.channelCount > 1) {
            out.right[i] = samples_[pos];
            pos = (pos + 1u) % kSampleCapacity;
        }
    }

    sampleRead_.store(pos, std::memory_order_release);
    headerRead_.store((headerRead + 1u) % kHeaderCapacity, std::memory_order_release);
    return true;
}

bool HostCaptureQueue::takeOverflow() noexcept
{
    return overflow_.exchange(false, std::memory_order_relaxed);
}

void HostCaptureQueue::clear() noexcept
{
    // Читатель двигает свои индексы к писательским: всё непрочитанное теряется
    headerRead_.store(headerWrite_.load(std::memory_order_acquire), std::memory_order_release);
    sampleRead_.store(sampleWrite_.load(std::memory_order_acquire), std::memory_order_release);
    overflow_.store(false, std::memory_order_relaxed);
}

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
    // Вызывается из process(), то есть из аудиопотока: здесь нельзя ни
    // выделять память, ни трогать общий буфер — им владеет поток интерфейса.
    // Блок просто кладётся в очередь, разбирает её drainHostCapture().
    if (!inputs || channelCount <= 0 || frameCount <= 0) {
        return TrackToolStatus::InvalidAudioInfo;
    }
    capture_.push(inputs, channelCount, frameCount, timelineFrame);
    return TrackToolStatus::Ok;
}

bool TrackToolSession::drainHostCapture()
{
    // Только поток интерфейса. Здесь и происходит вся работа с audioBuffer_:
    // размещение по таймлайну, сброс на новом проходе, пересчёт моно.
    bool changed = false;
    while (capture_.pop(captureBlock_)) {
        applyCaptureBlock(captureBlock_);
        changed = true;
    }
    return changed;
}

bool TrackToolSession::captureOverflowed()
{
    return capture_.takeOverflow();
}

TrackToolStatus TrackToolSession::applyCaptureBlock(const HostCaptureQueue::Block& block)
{
    const int channelCount = block.channelCount;
    const int frameCount = block.frameCount;
    const std::int64_t timelineFrame = block.timelineFrame;
    if (channelCount <= 0 || frameCount <= 0 || block.left.empty()) {
        return TrackToolStatus::InvalidAudioInfo;
    }

    const float* left = block.left.data();
    const float* right = block.right.empty() ? nullptr : block.right.data();

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

void TrackToolSession::setRenderedOutput(const TrackAudioBuffer& buffer,
                                         std::int64_t timelineStartFrame)
{
    // Готовим новый буфер в стороне и публикуем одним указателем: аудиопоток
    // либо видит старый результат целиком, либо новый — но никогда не читает
    // вектор, который прямо сейчас переезжает
    auto next = std::make_shared<TrackAudioBuffer>(buffer);
    if (next->mono.empty() && !next->left.empty()) {
        rebuildMonoFromChannels(*next);
    }
    renderedOutputStart_.store(std::max<std::int64_t>(0, timelineStartFrame),
                               std::memory_order_release);
    // Прошлый держим у себя: иначе последнюю ссылку мог бы отпустить
    // аудиопоток, и освобождение памяти случилось бы в реальном времени
    retiredRenderedOutput_ = std::atomic_load(&renderedOutput_);
    std::atomic_store(&renderedOutput_, std::shared_ptr<const TrackAudioBuffer>(next));
}

void TrackToolSession::clearRenderedOutput()
{
    retiredRenderedOutput_ = std::atomic_load(&renderedOutput_);
    std::atomic_store(&renderedOutput_, std::shared_ptr<const TrackAudioBuffer>());
    renderedOutputStart_.store(0, std::memory_order_release);
}

bool TrackToolSession::hasRenderedOutput() const
{
    const auto rendered = std::atomic_load(&renderedOutput_);
    return rendered && !rendered->mono.empty();
}

std::shared_ptr<const TrackAudioBuffer> TrackToolSession::renderedOutput() const
{
    return std::atomic_load(&renderedOutput_);
}

bool TrackToolSession::readRenderedOutput(float* const* outputs, int channelCount,
                                          int frameCount, std::int64_t timelineFrame) const
{
    // Аудиопоток. Берём указатель себе: пока он у нас, интерфейс может сколько
    // угодно публиковать новый результат — наш буфер под нами не переедет
    const std::shared_ptr<const TrackAudioBuffer> rendered = std::atomic_load(&renderedOutput_);
    if (!outputs || channelCount <= 0 || frameCount <= 0 || !rendered || rendered->mono.empty()) {
        return false;
    }

    const std::int64_t start = (timelineFrame >= 0 ? timelineFrame : 0)
        - renderedOutputStart_.load(std::memory_order_acquire);
    const std::int64_t total = static_cast<std::int64_t>(rendered->mono.size());
    if (start + frameCount <= 0 || start >= total) {
        return false;  // блок не пересекается с результатом
    }

    // Левый/правый: если результат моно, оба канала берут его же
    const std::vector<float>& left =
        rendered->left.empty() ? rendered->mono : rendered->left;
    const std::vector<float>& right =
        rendered->right.empty() ? left : rendered->right;

    for (int i = 0; i < frameCount; ++i) {
        const std::int64_t index = start + i;
        const bool inside = index >= 0 && index < total;
        for (int ch = 0; ch < channelCount; ++ch) {
            if (!outputs[ch]) {
                continue;
            }
            const std::vector<float>& source = (ch == 0) ? left : right;
            outputs[ch][i] = (inside && index < static_cast<std::int64_t>(source.size()))
                ? source[static_cast<std::size_t>(index)]
                : 0.0f;
        }
    }
    return true;
}

void TrackToolSession::clearHostCapture()
{
    // Незабранные блоки тоже выбрасываем: иначе они всплывут после сброса
    capture_.clear();
    audioBuffer_ = {};
    pitchAnalysis_ = {};
    clearRenderedOutput();
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
