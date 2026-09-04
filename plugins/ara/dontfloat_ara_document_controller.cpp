#include "dontfloat_ara_document_controller.h"

#include "../core/dontfloat_diagnostics.h"

#include <cstdio>
#include "dontfloat_version.h"

#include "../../include/pitchdetector.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <functional>
#include <limits>
#include <utility>

namespace Dontfloat::Ara {
namespace {

using Dontfloat::PluginCore::TrackPitchNote;

constexpr const char* kFactoryId = "org.dontfloat.ara.factory";
constexpr const char* kPlugInName = "DONTFLOAT";
constexpr const char* kManufacturerName = "DONTFLOAT";
constexpr const char* kInformationUrl = "https://github.com/ili4yov-ika/DONTFLOAT";
constexpr const char* kVersionString = DONTFLOAT_VERSION_STRING;
constexpr const char* kDocumentArchiveId = "org.dontfloat.ara.archive.v1";

/** Единственный тип содержимого, который мы разбираем и отдаём хосту. */
const ARA::ARAContentType kAnalyzeableContentTypes[] = { ARA::kARAContentTypeNotes };

/** Читает весь источник в моно через ARA-доступ к сэмплам. */
std::vector<float> readMonoSamples(const ARA::PlugIn::AudioSource* audioSource)
{
    const auto frameCount = static_cast<std::int64_t>(audioSource->getSampleCount());
    const int channelCount = std::max(1, audioSource->getChannelCount());
    if (frameCount <= 0) {
        return {};
    }

    ARA::PlugIn::HostAudioReader reader { audioSource };

    // Читаем кусками: у ARA произвольный доступ ко всему файлу, ждать
    // проигрывания дорожки (как при захвате по блокам) не нужно
    constexpr std::int64_t kChunkFrames = 1 << 15;
    std::vector<std::vector<float>> chunk(static_cast<std::size_t>(channelCount),
                                          std::vector<float>(static_cast<std::size_t>(kChunkFrames), 0.0f));
    std::vector<void*> chunkPointers(static_cast<std::size_t>(channelCount), nullptr);

    std::vector<float> mono;
    mono.reserve(static_cast<std::size_t>(frameCount));

    for (std::int64_t position = 0; position < frameCount; position += kChunkFrames) {
        const std::int64_t frames = std::min<std::int64_t>(kChunkFrames, frameCount - position);
        for (int channel = 0; channel < channelCount; ++channel) {
            chunkPointers[static_cast<std::size_t>(channel)] =
                chunk[static_cast<std::size_t>(channel)].data();
        }
        if (!reader.readAudioSamples(position, static_cast<ARA::ARASampleCount>(frames),
                                     chunkPointers.data())) {
            break;  // хост отказал в доступе — берём то, что успели прочитать
        }
        for (std::int64_t i = 0; i < frames; ++i) {
            float sum = 0.0f;
            for (int channel = 0; channel < channelCount; ++channel) {
                sum += chunk[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)];
            }
            mono.push_back(sum / float(channelCount));
        }
    }
    return mono;
}

/** Наш детектор нот поверх прочитанных сэмплов. */
AraNoteSet analyzeSamples(std::vector<float> mono, double sampleRate,
                          const std::function<void(int)>& onProgress = {})
{
    AraNoteSet result;
    result.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    if (mono.empty()) {
        return result;
    }

    QVector<float> qMono(static_cast<int>(mono.size()));
    std::copy(mono.begin(), mono.end(), qMono.begin());

    const QVector<PitchDetector::PitchNote> detected =
        PitchDetector::detectNotes(qMono, int(result.sampleRate), PitchDetector::Options {},
                                   onProgress);

    result.notes.reserve(static_cast<std::size_t>(detected.size()));
    for (const PitchDetector::PitchNote& note : detected) {
        TrackPitchNote out;
        out.startSample = note.startSample;
        out.endSample = note.endSample;
        out.midiPitch = note.midiPitch;
        out.detectedPitch = note.detectedPitch;
        out.confidence = note.confidence;
        result.notes.push_back(out);
    }
    result.grade = ARA::kARAContentGradeDetected;
    result.valid = true;
    return result;
}

/** Ноты в события ARA (kARAContentTypeNotes), время — секунды источника. */
class AraNoteContentReader : public ARA::PlugIn::ContentReader {
public:
    /** Ноты источника как есть (координаты — секунды самого источника). */
    AraNoteContentReader(const AraNoteSet& noteSet, const ARA::ARAContentTimeRange* range)
        : AraNoteContentReader(noteSet, range, 0.0, 1.0, 0.0,
                               std::numeric_limits<double>::max())
    {
    }

    /**
     * Ноты клипа в координатах проекта. Разрез и обрезка клипа отсекают ноты
     * за его границами, перенос сдвигает их, растяжение — масштабирует.
     */
    AraNoteContentReader(const AraNoteSet& noteSet, const ARA::ARAContentTimeRange* range,
                         double timeOffset, double stretchFactor, double clipStartInSource,
                         double clipDurationInSource)
    {
        const double sampleRate = noteSet.sampleRate > 0.0 ? noteSet.sampleRate : 44100.0;
        const double stretch = stretchFactor > 0.0 ? stretchFactor : 1.0;
        const double clipEndInSource = clipDurationInSource >= std::numeric_limits<double>::max()
            ? std::numeric_limits<double>::max()
            : clipStartInSource + clipDurationInSource;

        events_.reserve(noteSet.notes.size());
        for (const TrackPitchNote& note : noteSet.notes) {
            const double sourceStart = double(note.startSample) / sampleRate;
            const double sourceDuration =
                double(std::max<std::int64_t>(1, note.endSample - note.startSample)) / sampleRate;
            // Нота целиком вне клипа — её здесь нет (разрез/обрезка)
            if (sourceStart + sourceDuration <= clipStartInSource || sourceStart >= clipEndInSource) {
                continue;
            }

            const double start = timeOffset + (sourceStart - clipStartInSource) * stretch;
            const double duration = sourceDuration * stretch;
            if (range && (start + duration <= range->start || range->start + range->duration <= start)) {
                continue;  // нота вне запрошенного отрезка
            }

            ARA::ARAContentNote event {};
            const float midiPitch = note.midiPitch;
            event.frequency = PitchDetector::midiToFrequency(midiPitch);
            event.pitchNumber = static_cast<ARA::ARAPitchNumber>(std::lround(midiPitch));
            event.volume = std::clamp(note.confidence, 0.0f, 1.0f);
            event.startPosition = start;
            event.attackDuration = 0.0;
            event.noteDuration = duration;
            event.signalDuration = duration;
            events_.push_back(event);
        }
    }

    ARA::ARAInt32 getEventCount() noexcept override
    {
        return static_cast<ARA::ARAInt32>(events_.size());
    }

    const void* getDataForEvent(ARA::ARAInt32 eventIndex) noexcept override
    {
        if (eventIndex < 0 || static_cast<std::size_t>(eventIndex) >= events_.size()) {
            return nullptr;
        }
        return &events_[static_cast<std::size_t>(eventIndex)];
    }

private:
    std::vector<ARA::ARAContentNote> events_;
};

/** Формат архива разметки: свой, простой и версионируемый. */
constexpr std::uint32_t kArchiveMagic = 0x444E4641;  // "DNFA"
constexpr std::uint32_t kArchiveVersion = 2;  // v2: + исходный отрезок ноты

/** Последовательная запись в архив хоста. */
class ArchiveWriteCursor {
public:
    explicit ArchiveWriteCursor(ARA::PlugIn::HostArchiveWriter* writer) noexcept : writer_ { writer } {}

    template <typename T>
    void write(const T& value) noexcept
    {
        writeBytes(reinterpret_cast<const ARA::ARAByte*>(&value), sizeof(T));
    }

    void writeString(const char* text) noexcept
    {
        const std::string value { text ? text : "" };
        write(static_cast<std::uint32_t>(value.size()));
        writeBytes(reinterpret_cast<const ARA::ARAByte*>(value.data()), value.size());
    }

    bool ok() const noexcept { return ok_; }

private:
    void writeBytes(const ARA::ARAByte* bytes, std::size_t length) noexcept
    {
        if (!ok_ || !writer_ || length == 0) {
            return;
        }
        ok_ = writer_->writeBytesToArchive(position_, length, bytes);
        position_ += length;
    }

    ARA::PlugIn::HostArchiveWriter* writer_ = nullptr;
    ARA::ARASize position_ = 0;
    bool ok_ = true;
};

/** Последовательное чтение архива хоста. */
class ArchiveReadCursor {
public:
    explicit ArchiveReadCursor(ARA::PlugIn::HostArchiveReader* reader) noexcept : reader_ { reader } {}

    template <typename T>
    bool read(T& value) noexcept
    {
        return readBytes(reinterpret_cast<ARA::ARAByte*>(&value), sizeof(T));
    }

    bool readString(std::string& value) noexcept
    {
        std::uint32_t length = 0;
        if (!read(length)) {
            return false;
        }
        value.assign(length, char(0));
        return length == 0 || readBytes(reinterpret_cast<ARA::ARAByte*>(value.data()), length);
    }

private:
    bool readBytes(ARA::ARAByte* bytes, std::size_t length) noexcept
    {
        if (!reader_ || length == 0) {
            return reader_ != nullptr;
        }
        if (!reader_->readBytesFromArchive(position_, length, bytes)) {
            return false;
        }
        position_ += length;
        return true;
    }

    ARA::PlugIn::HostArchiveReader* reader_ = nullptr;
    ARA::ARASize position_ = 0;
};

/** Описание плагина для хоста. */
class AraFactoryConfig : public ARA::PlugIn::FactoryConfig {
public:
    ARA::ARAAPIGeneration getHighestSupportedApiGeneration() const noexcept override
    {
        return ARA::kARAAPIGeneration_2_3_Final;
    }
    const char* getFactoryID() const noexcept override { return kFactoryId; }
    const char* getPlugInName() const noexcept override { return kPlugInName; }
    const char* getManufacturerName() const noexcept override { return kManufacturerName; }
    const char* getInformationURL() const noexcept override { return kInformationUrl; }
    const char* getVersion() const noexcept override { return kVersionString; }
    const char* getDocumentArchiveID() const noexcept override { return kDocumentArchiveId; }

    ARA::ARASize getAnalyzeableContentTypesCount() const noexcept override
    {
        return std::size(kAnalyzeableContentTypes);
    }

    /**
     * Что хост вправе делать с нашими клипами: растягивать (в том числе вслед
     * за сменой темпа проекта) и подрезать края. Без этих флагов DAW считает
     * плагин нерастяжимым и запрещает сжатие/разжатие клипа.
     */
    ARA::ARAPlaybackTransformationFlags getSupportedPlaybackTransformationFlags() const noexcept override
    {
        return ARA::kARAPlaybackTransformationTimestretch
            | ARA::kARAPlaybackTransformationTimestretchReflectingTempo
            | ARA::kARAPlaybackTransformationContentBasedFadeAtTail
            | ARA::kARAPlaybackTransformationContentBasedFadeAtHead;
    }
    const ARA::ARAContentType* getAnalyzeableContentTypes() const noexcept override
    {
        return kAnalyzeableContentTypes;
    }
};

} // namespace

void AraAudioSource::setNoteSet(AraNoteSet noteSet) noexcept
{
    noteSet_ = std::move(noteSet);
}

void AraAudioSource::setMonoSamples(std::vector<float> samples) noexcept
{
    samplesReady_.store(false, std::memory_order_release);
    monoSamples_ = std::move(samples);
    samplesReady_.store(!monoSamples_.empty(), std::memory_order_release);
}

void AraAudioSource::setEditedSamples(std::vector<float> samples) noexcept
{
    auto next = samples.empty()
        ? std::shared_ptr<const std::vector<float>>()
        : std::make_shared<const std::vector<float>>(std::move(samples));
    std::atomic_store(&editedSamples_, next);
}

const ARA::ARAFactory* AraDocumentController::getARAFactory() noexcept
{
    return ARA::PlugIn::PlugInEntry::getPlugInEntry<AraFactoryConfig, AraDocumentController>()
        ->getFactory();
}

ARA::PlugIn::AudioSource* AraDocumentController::doCreateAudioSource(
    ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef) noexcept
{
    return new AraAudioSource(document, hostRef);
}

void AraDocumentController::willDestroyAudioSource(ARA::PlugIn::AudioSource* audioSource) noexcept
{
    Dontfloat::PluginCore::Diagnostics::log("shutdown.ara.source-destroy");
    auto* source = static_cast<AraAudioSource*>(audioSource);
    const std::lock_guard<std::mutex> lock(mutex_);
    analyzedOrder_.erase(std::remove(analyzedOrder_.begin(), analyzedOrder_.end(), source),
                         analyzedOrder_.end());
    pendingAnalyses_.erase(
        std::remove_if(pendingAnalyses_.begin(), pendingAnalyses_.end(),
                       [source](const PendingAnalysis& pending) {
                           return pending.audioSource == source;
                       }),
        pendingAnalyses_.end());
}

void AraDocumentController::willEnableAudioSourceSamplesAccess(
    ARA::PlugIn::AudioSource* /*audioSource*/, bool /*enable*/) noexcept
{
    // Сэмплы читаются целиком внутри doRequestAudioSourceContentAnalysis,
    // поэтому отключение доступа фоновому разбору уже не мешает
}

void AraDocumentController::didEnableAudioSourceSamplesAccess(
    ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    if (enable) {
        analyzeIfNeeded(static_cast<AraAudioSource*>(audioSource));
    }
}

void AraDocumentController::analyzeIfNeeded(AraAudioSource* audioSource) noexcept
{
    if (!audioSource || audioSource->hasNotes() || audioSource->analysisRunning()
        || !audioSource->isSampleAccessEnabled()) {
        return;
    }
    doRequestAudioSourceContentAnalysis(audioSource, { ARA::kARAContentTypeNotes });
}

void AraDocumentController::doRequestAudioSourceContentAnalysis(
    ARA::PlugIn::AudioSource* audioSource,
    std::vector<ARA::ARAContentType> const& contentTypes) noexcept
{
    const bool wantsNotes = std::find(contentTypes.begin(), contentTypes.end(),
                                      ARA::kARAContentTypeNotes) != contentTypes.end();
    auto* source = static_cast<AraAudioSource*>(audioSource);
    if (!wantsNotes || !source || source->analysisRunning() || !source->isSampleAccessEnabled()) {
        return;
    }

    // Сэмплы читаем здесь (доступ к ним даёт хост), а разбор уводим в фон —
    // хост не должен ждать анализа внутри своего вызова
    std::vector<float> mono = readMonoSamples(source);
    const double sampleRate = source->getSampleRate();
    // Копию звука держим у источника: по ней редактор рисует волну и слушает
    // ноты, не дожидаясь проигрывания дорожки
    source->setMonoSamples(mono);

    source->setAnalysisRunning(true);
    source->setAnalysisProgress(0);
    notifyAudioSourceAnalysisProgressStarted(source);

    const std::lock_guard<std::mutex> lock(mutex_);
    PendingAnalysis pending;
    pending.audioSource = source;
    pending.future = std::async(std::launch::async,
                                [source, mono = std::move(mono), sampleRate]() mutable {
                                    return analyzeSamples(std::move(mono), sampleRate,
                                                          [source](int percent) {
                                                              source->setAnalysisProgress(percent);
                                                          });
                                });
    pendingAnalyses_.push_back(std::move(pending));
}

void AraDocumentController::collectFinishedAnalyses() noexcept
{
    std::vector<std::pair<AraAudioSource*, AraNoteSet>> finished;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = pendingAnalyses_.begin(); it != pendingAnalyses_.end();) {
            if (!it->future.valid()) {
                it = pendingAnalyses_.erase(it);
                continue;
            }
            if (it->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                ++it;
                continue;
            }
            finished.emplace_back(it->audioSource, it->future.get());
            it = pendingAnalyses_.erase(it);
        }
    }

    for (auto& entry : finished) {
        AraAudioSource* source = entry.first;
        source->setNoteSet(std::move(entry.second));
        source->setAnalysisRunning(false);
        source->setAnalysisProgress(100);
        std::size_t analyzedCount = 0;
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            analyzedOrder_.erase(std::remove(analyzedOrder_.begin(), analyzedOrder_.end(), source),
                                 analyzedOrder_.end());
            analyzedOrder_.push_back(source);
            analyzedCount = analyzedOrder_.size();
        }
        if (Dontfloat::PluginCore::Diagnostics::enabled()) {
            char line[160];
            std::snprintf(line, sizeof(line), "ara.notes.parsed count=%zu analyzed=%zu",
                          source->noteSet().notes.size(), analyzedCount);
            Dontfloat::PluginCore::Diagnostics::log(line);
        }
        ++modelRevision_;
        notifyAudioSourceContentChanged(source, ARA::ContentUpdateScopes::notesAreAffected());
        notifyAudioSourceAnalysisProgressCompleted(source);
    }
}

void AraDocumentController::willNotifyModelUpdates() noexcept
{
    collectFinishedAnalyses();
}

AraNoteSet AraDocumentController::referenceNotesExcluding(
    const ARA::PlugIn::AudioSource* exclude) const noexcept
{
    const std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = analyzedOrder_.rbegin(); it != analyzedOrder_.rend(); ++it) {
        if (*it == exclude || !(*it)->hasNotes()) {
            continue;
        }
        if (Dontfloat::PluginCore::Diagnostics::enabled()) {
            // Источник пишем оба: и чей референс отдали, и кому. По этой паре
            // сразу видно случай «один файл на двух дорожках» — у ARA это один
            // источник, и своих нот у соседа тогда попросту нет
            char line[256];
            std::snprintf(line, sizeof(line),
                          "ara.notes.reference count=%zu pool=%zu from=%s for=%s",
                          (*it)->noteSet().notes.size(), analyzedOrder_.size(),
                          (*it)->getPersistentID().c_str(),
                          exclude ? exclude->getPersistentID().c_str() : "<none>");
            Dontfloat::PluginCore::Diagnostics::log(line);
        }
        return (*it)->noteSet();
    }
    return {};
}

void AraDocumentController::doUpdateMusicalContextContent(
    ARA::PlugIn::MusicalContext* /*musicalContext*/, const ARA::ARAContentTimeRange* /*range*/,
    ARA::ContentUpdateScopes /*scopeFlags*/) noexcept
{
    // Темп и тактовую сетку читаем у хоста по требованию, кэша нет
}

void AraDocumentController::doUpdateAudioSourceContent(
    ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* /*range*/,
    ARA::ContentUpdateScopes scopeFlags) noexcept
{
    // Хост переписал звук источника — наш разбор устарел, считаем заново
    if (!scopeFlags.affectSamples()) {
        return;
    }
    auto* source = static_cast<AraAudioSource*>(audioSource);
    source->setNoteSet({});
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        analyzedOrder_.erase(std::remove(analyzedOrder_.begin(), analyzedOrder_.end(), source),
                             analyzedOrder_.end());
    }
    analyzeIfNeeded(source);
}

void AraDocumentController::didAddAudioSourceToDocument(
    ARA::PlugIn::Document* /*document*/, ARA::PlugIn::AudioSource* audioSource) noexcept
{
    // Новый клип с новым файлом на дорожке — разбираем сразу, не дожидаясь
    // ни проигрывания, ни просьбы хоста (так работает Melodyne)
    ++modelRevision_;
    // Один раз за сеанс пишем, отдал ли хост управление транспортом: без
    // этого интерфейса кнопки воспроизведения в плагине бессильны, и по
    // дневнику сразу видно, в плагине дело или в DAW
    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        static std::atomic<bool> logged { false };
        if (!logged.exchange(true)) {
            char line[64];
            std::snprintf(line, sizeof(line), "ara.host.playback=%d",
                          getHostPlaybackController() != nullptr ? 1 : 0);
            Dontfloat::PluginCore::Diagnostics::log(line);
        }
    }
    analyzeIfNeeded(static_cast<AraAudioSource*>(audioSource));
}

// Правки клипов приходят только сюда, и снаружи хоста их не видно: у плагина
// нет ни параметров, ни своего окна отчётов. Интеграционный тест с настоящей
// DAW читает эти строки — без них проверить нарезку, перенос и растяжение
// можно было бы только глазами.
void AraDocumentController::logRegionToDiagnostics(
    const char* event, const ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    if (!Dontfloat::PluginCore::Diagnostics::enabled() || !playbackRegion) {
        return;
    }
    char line[256];
    std::snprintf(line, sizeof(line),
                  "%s start=%.6f dur=%.6f srcStart=%.6f srcDur=%.6f",
                  event,
                  playbackRegion->getStartInPlaybackTime(),
                  playbackRegion->getDurationInPlaybackTime(),
                  playbackRegion->getStartInAudioModificationTime(),
                  playbackRegion->getDurationInAudioModificationTime());
    Dontfloat::PluginCore::Diagnostics::log(line);
}

void AraDocumentController::didAddPlaybackRegionToRegionSequence(
    ARA::PlugIn::RegionSequence* /*regionSequence*/,
    ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    // Клип положили на дорожку: если его источник ещё не разобран — в очередь.
    // Так новый участок дорожки получает ноты сам по себе
    ++modelRevision_;
    logRegionToDiagnostics("ara.region.add", playbackRegion);
    if (!playbackRegion || !playbackRegion->getAudioModification()) {
        return;
    }
    analyzeIfNeeded(static_cast<AraAudioSource*>(
        playbackRegion->getAudioModification()->getAudioSource()));
}

void AraDocumentController::didUpdatePlaybackRegionProperties(
    ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    // Разрез, перенос, обрезка и растяжение клипа приходят сюда одинаково:
    // сам звук не изменился, поменялось только его место на таймлайне,
    // поэтому разбор не перезапускаем — двигается разметка
    ++modelRevision_;
    logRegionToDiagnostics("ara.region.update", playbackRegion);
    if (playbackRegion) {
        notifyPlaybackRegionContentChanged(playbackRegion,
                                           ARA::ContentUpdateScopes::timelineIsAffected());
    }
}

bool AraPlaybackRenderer::renderBlock(float* const* outputs, int channelCount, int frameCount,
                                      std::int64_t timelineStartFrame, double sampleRate) noexcept
{
    if (!outputs || channelCount <= 0 || frameCount <= 0 || sampleRate <= 0.0) {
        return false;
    }

    const double blockStartSeconds = double(timelineStartFrame) / sampleRate;
    const double blockEndSeconds = double(timelineStartFrame + frameCount) / sampleRate;

    bool rendered = false;

    for (const ARA::PlugIn::PlaybackRegion* region : getPlaybackRegions()) {
        if (!region || !region->getAudioModification()) {
            continue;
        }

        const double regionStart = region->getStartInPlaybackTime();
        const double regionDuration = region->getDurationInPlaybackTime();
        if (regionDuration <= 0.0
            || blockEndSeconds <= regionStart
            || blockStartSeconds >= regionStart + regionDuration) {
            continue;  // клип не пересекается с этим блоком
        }

        const auto* source = static_cast<const AraAudioSource*>(
            region->getAudioModification()->getAudioSource());
        if (!source) {
            continue;
        }
        // Правки плагина важнее исходника: ради них он тут и стоит. Указатель
        // держим до конца блока — правку могут применить прямо сейчас, из
        // интерфейса, и буфер под нами сменится
        const std::shared_ptr<const std::vector<float>> edited = source->editedSamples();
        if (!edited && !source->samplesReady()) {
            continue;  // разбор ещё не отдал сэмплы
        }

        const std::vector<float>& samples = edited ? *edited : source->monoSamples();
        const double sourceRate = source->getSampleRate() > 0.0 ? source->getSampleRate()
                                                                : sampleRate;
        const double sourceStart = region->getStartInAudioModificationTime();
        const double sourceDuration = region->getDurationInAudioModificationTime();
        // Растяжение клипа: длительность в проекте к длительности в источнике
        const double stretch = (sourceDuration > 0.0) ? (regionDuration / sourceDuration) : 1.0;
        if (stretch <= 0.0) {
            continue;
        }

        for (int i = 0; i < frameCount; ++i) {
            const double timeSeconds = double(timelineStartFrame + i) / sampleRate;
            const double offsetInRegion = timeSeconds - regionStart;
            if (offsetInRegion < 0.0 || offsetInRegion >= regionDuration) {
                continue;
            }

            // Место внутри файла с учётом того, что клип могли растянуть
            const double positionInSource = sourceStart + offsetInRegion / stretch;
            const double sampleIndex = positionInSource * sourceRate;
            if (sampleIndex < 0.0) {
                continue;
            }

            const std::size_t index = std::size_t(sampleIndex);
            if (index + 1 >= samples.size()) {
                continue;
            }

            // Линейная интерполяция: при растяжении индекс попадает между отсчётами
            const float fraction = float(sampleIndex - double(index));
            const float value = samples[index] * (1.0f - fraction) + samples[index + 1] * fraction;

            for (int channel = 0; channel < channelCount; ++channel) {
                if (outputs[channel]) {
                    outputs[channel][i] += value;
                }
            }
            rendered = true;
            renderedFrames_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    return rendered;
}

bool AraDocumentController::requestHostPlayback(bool start) noexcept
{
    auto* playback = getHostPlaybackController();
    if (!playback) {
        Dontfloat::PluginCore::Diagnostics::log("ara.transport.unavailable");
        return false;  // хост управление транспортом не отдал
    }
    if (start) {
        playback->requestStartPlayback();
    } else {
        playback->requestStopPlayback();
    }
    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        Dontfloat::PluginCore::Diagnostics::log(start ? "ara.transport.start"
                                                      : "ara.transport.stop");
    }
    return true;
}

bool AraDocumentController::requestHostPlaybackPosition(double seconds) noexcept
{
    auto* playback = getHostPlaybackController();
    if (!playback) {
        return false;
    }
    playback->requestSetPlaybackPosition(ARA::ARATimePosition(std::max(0.0, seconds)));
    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        char line[128];
        std::snprintf(line, sizeof(line), "ara.transport.seek seconds=%.3f", seconds);
        Dontfloat::PluginCore::Diagnostics::log(line);
    }
    return true;
}

void AraDocumentController::publishEditedAudio(AraAudioSource* audioSource,
                                               std::vector<float> samples) noexcept
{
    if (!audioSource) {
        return;
    }
    const std::size_t frames = samples.size();
    audioSource->setEditedSamples(std::move(samples));
    ++modelRevision_;
    // Хосту говорим, что звук клипов изменился: он перечитает то, что кэшировал
    for (ARA::PlugIn::AudioModification* modification : audioSource->getAudioModifications()) {
        notifyAudioModificationContentChanged(modification,
                                              ARA::ContentUpdateScopes::samplesAreAffected());
    }
    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        char line[256];
        std::snprintf(line, sizeof(line), "ara.audio.edited frames=%zu source=%s",
                      frames, audioSource->getPersistentID().c_str());
        Dontfloat::PluginCore::Diagnostics::log(line);
    }
}

std::vector<AraClipPlacement> AraDocumentController::clipsForAudioSource(
    const ARA::PlugIn::AudioSource* audioSource) const noexcept
{
    std::vector<AraClipPlacement> clips;
    if (!audioSource) {
        return clips;
    }
    for (const ARA::PlugIn::AudioModification* modification : audioSource->getAudioModifications()) {
        for (const ARA::PlugIn::PlaybackRegion* region : modification->getPlaybackRegions()) {
            AraClipPlacement clip;
            clip.startInPlaybackSeconds = region->getStartInPlaybackTime();
            clip.durationInPlaybackSeconds = region->getDurationInPlaybackTime();
            clip.startInSourceSeconds = region->getStartInAudioModificationTime();
            clip.durationInSourceSeconds = region->getDurationInAudioModificationTime();
            clips.push_back(clip);
        }
    }
    // По порядку на таймлайне — так редактору проще искать нужный клип
    std::sort(clips.begin(), clips.end(), [](const AraClipPlacement& a, const AraClipPlacement& b) {
        return a.startInPlaybackSeconds < b.startInPlaybackSeconds;
    });
    return clips;
}

ARA::PlugIn::PlaybackRenderer* AraDocumentController::doCreatePlaybackRenderer() noexcept
{
    return new AraPlaybackRenderer(this);
}

ARA::PlugIn::EditorView* AraDocumentController::doCreateEditorView() noexcept
{
    return new AraEditorView(this);
}

AraAudioSource* AraDocumentController::audioSourceForInstance(
    const ARA::PlugIn::PlugInExtension& extension) noexcept
{
    const auto sourceOfRegion = [](const ARA::PlugIn::PlaybackRegion* region) -> AraAudioSource* {
        if (!region || !region->getAudioModification()) {
            return nullptr;
        }
        return static_cast<AraAudioSource*>(region->getAudioModification()->getAudioSource());
    };

    // Порядок здесь и есть ответ на «почему плагин показывал ноты соседней
    // дорожки»: сначала клипы собственных ролей экземпляра, и только если их
    // нет — выбор в DAW. Раньше выбор стоял первым, и экземпляр на первой
    // дорожке цеплял клип, выделенный на второй
    if (const auto* renderer = extension.getPlaybackRenderer<AraPlaybackRenderer>()) {
        if (!renderer->getPlaybackRegions().empty()) {
            if (AraAudioSource* source = sourceOfRegion(renderer->getPlaybackRegions().front())) {
                return source;
            }
        }
    }
    // Роль редактора: хост назначает ей клипы дорожки даже тогда, когда
    // воспроизводящему рендереру ничего не дал. Без этого пути экземпляр
    // не знает своей дорожки: полная редакция цепляла клип из выделения в
    // DAW (то есть чужой), а Pitcher не находил ничего
    if (const auto* editorRenderer = extension.getEditorRenderer<ARA::PlugIn::EditorRenderer>()) {
        if (!editorRenderer->getPlaybackRegions().empty()) {
            if (AraAudioSource* source =
                    sourceOfRegion(editorRenderer->getPlaybackRegions().front())) {
                return source;
            }
        }
        // Дорожку по последовательностям берём, только если она одна: когда
        // хост отдал редактору несколько, какая из них наша — неизвестно, и
        // «первая попавшаяся» приводила к нотам соседа под видом своих
        const auto& sequences = editorRenderer->getRegionSequences();
        if (sequences.size() == 1 && sequences.front()
            && !sequences.front()->getPlaybackRegions().empty()) {
            if (AraAudioSource* source =
                    sourceOfRegion(sequences.front()->getPlaybackRegions().front())) {
                return source;
            }
        }
    }

    // Выбор в DAW — последняя надежда: он про то, что выделил человек, а не
    // про то, на каком клипе висит этот экземпляр
    if (const auto* view = extension.getEditorView<AraEditorView>();
        view && view->isEditorOpen()) {
        const auto& selection = view->getViewSelection();
        if (!selection.getPlaybackRegions().empty()) {
            if (AraAudioSource* source = sourceOfRegion(selection.getPlaybackRegions().front())) {
                return source;
            }
        }
    }

    if (Dontfloat::PluginCore::Diagnostics::enabled()) {
        const auto* renderer = extension.getPlaybackRenderer<AraPlaybackRenderer>();
        const auto* editorRenderer = extension.getEditorRenderer<ARA::PlugIn::EditorRenderer>();
        char line[200];
        std::snprintf(line, sizeof(line),
                      "ara.source.unknown playback=%zu editor=%zu sequences=%zu",
                      renderer ? renderer->getPlaybackRegions().size() : 0,
                      editorRenderer ? editorRenderer->getPlaybackRegions().size() : 0,
                      editorRenderer ? editorRenderer->getRegionSequences().size() : 0);
        Dontfloat::PluginCore::Diagnostics::log(line);
    }
    return nullptr;
}

bool AraDocumentController::clipForInstance(const ARA::PlugIn::PlugInExtension& extension,
                                            AraClipPlacement* placement) noexcept
{
    if (!placement) {
        return false;
    }
    const auto fill = [placement](const ARA::PlugIn::PlaybackRegion* region) {
        if (!region) {
            return false;
        }
        placement->startInPlaybackSeconds = region->getStartInPlaybackTime();
        placement->durationInPlaybackSeconds = region->getDurationInPlaybackTime();
        placement->startInSourceSeconds = region->getStartInAudioModificationTime();
        placement->durationInSourceSeconds = region->getDurationInAudioModificationTime();
        return true;
    };

    if (const auto* renderer = extension.getPlaybackRenderer<AraPlaybackRenderer>()) {
        if (!renderer->getPlaybackRegions().empty()) {
            return fill(renderer->getPlaybackRegions().front());
        }
    }
    if (const auto* editorRenderer = extension.getEditorRenderer<ARA::PlugIn::EditorRenderer>()) {
        if (!editorRenderer->getPlaybackRegions().empty()) {
            return fill(editorRenderer->getPlaybackRegions().front());
        }
        // Та же осторожность, что и в audioSourceForInstance: несколько
        // последовательностей — значит, своя неизвестна
        const auto& sequences = editorRenderer->getRegionSequences();
        if (sequences.size() == 1 && sequences.front()
            && !sequences.front()->getPlaybackRegions().empty()) {
            return fill(sequences.front()->getPlaybackRegions().front());
        }
    }
    if (const auto* view = extension.getEditorView<AraEditorView>();
        view && view->isEditorOpen()) {
        const auto& selection = view->getViewSelection();
        if (!selection.getPlaybackRegions().empty()) {
            return fill(selection.getPlaybackRegions().front());
        }
    }
    return false;
}

AraAudioSource* AraDocumentController::onlyAudioSource() const noexcept
{
    const ARA::PlugIn::Document* document = getDocument();
    if (!document || document->getAudioSources().size() != 1) {
        return nullptr;
    }
    return static_cast<AraAudioSource*>(document->getAudioSources().front());
}

AraBeatGrid AraDocumentController::hostBeatGrid() const noexcept
{
    AraBeatGrid grid;
    const ARA::PlugIn::Document* document = getDocument();
    if (!document || document->getMusicalContexts().empty()) {
        return grid;
    }
    const ARA::PlugIn::MusicalContext* context = document->getMusicalContexts().front();

    // Темп: две соседние точки карты темпа дают BPM участка
    ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader { context };
    if (tempoReader && tempoReader.getEventCount() >= 2) {
        const ARA::ARAContentTempoEntry first = tempoReader.getDataForEvent(0);
        const ARA::ARAContentTempoEntry second = tempoReader.getDataForEvent(1);
        const double seconds = second.timePosition - first.timePosition;
        const double quarters = second.quarterPosition - first.quarterPosition;
        if (seconds > 0.0 && quarters > 0.0) {
            grid.tempoBpm = 60.0 * quarters / seconds;
            grid.gridStartSeconds = first.timePosition;
            grid.valid = true;
        }
    }

    // Размер такта: берём первый, как и главное окно (одна сетка на трек)
    ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> barReader { context };
    if (barReader && barReader.getEventCount() > 0) {
        const ARA::ARAContentBarSignature signature = barReader.getDataForEvent(0);
        if (signature.numerator > 0) {
            grid.beatsPerBar = signature.numerator;
        }
    }
    return grid;
}

bool AraDocumentController::doIsAudioSourceContentAvailable(
    const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept
{
    if (type != ARA::kARAContentTypeNotes) {
        return false;
    }
    collectFinishedAnalyses();
    return static_cast<const AraAudioSource*>(audioSource)->hasNotes();
}

ARA::ARAContentGrade AraDocumentController::doGetAudioSourceContentGrade(
    const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept
{
    if (!doIsAudioSourceContentAvailable(audioSource, type)) {
        return ARA::kARAContentGradeInitial;
    }
    return static_cast<const AraAudioSource*>(audioSource)->noteSet().grade;
}

ARA::PlugIn::ContentReader* AraDocumentController::doCreateAudioSourceContentReader(
    ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type,
    const ARA::ARAContentTimeRange* range) noexcept
{
    if (type != ARA::kARAContentTypeNotes) {
        return nullptr;
    }
    return new AraNoteContentReader(static_cast<const AraAudioSource*>(audioSource)->noteSet(), range);
}

bool AraDocumentController::doIsAudioModificationContentAvailable(
    const ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type) noexcept
{
    // Правки высот пока не меняют разметку — отдаём ноты исходного источника
    return doIsAudioSourceContentAvailable(audioModification->getAudioSource(), type);
}

ARA::ARAContentGrade AraDocumentController::doGetAudioModificationContentGrade(
    const ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type) noexcept
{
    return doGetAudioSourceContentGrade(audioModification->getAudioSource(), type);
}

ARA::PlugIn::ContentReader* AraDocumentController::doCreateAudioModificationContentReader(
    ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type,
    const ARA::ARAContentTimeRange* range) noexcept
{
    return doCreateAudioSourceContentReader(audioModification->getAudioSource(), type, range);
}

bool AraDocumentController::doIsPlaybackRegionContentAvailable(
    const ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type) noexcept
{
    if (!playbackRegion || !playbackRegion->getAudioModification()) {
        return false;
    }
    return doIsAudioSourceContentAvailable(
        playbackRegion->getAudioModification()->getAudioSource(), type);
}

ARA::ARAContentGrade AraDocumentController::doGetPlaybackRegionContentGrade(
    const ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type) noexcept
{
    if (!playbackRegion || !playbackRegion->getAudioModification()) {
        return ARA::kARAContentGradeInitial;
    }
    return doGetAudioSourceContentGrade(
        playbackRegion->getAudioModification()->getAudioSource(), type);
}

ARA::PlugIn::ContentReader* AraDocumentController::doCreatePlaybackRegionContentReader(
    ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type,
    const ARA::ARAContentTimeRange* range) noexcept
{
    if (type != ARA::kARAContentTypeNotes || !playbackRegion
        || !playbackRegion->getAudioModification()) {
        return nullptr;
    }
    const auto* source = static_cast<const AraAudioSource*>(
        playbackRegion->getAudioModification()->getAudioSource());

    // Клип задаёт три вещи разом: где он лежит в проекте, какой кусок
    // источника берёт и во сколько раз растянут
    const double clipStartInSource = playbackRegion->getStartInAudioModificationTime();
    const double clipDurationInSource = playbackRegion->getDurationInAudioModificationTime();
    const double stretch = clipDurationInSource > 0.0
        ? playbackRegion->getDurationInPlaybackTime() / clipDurationInSource
        : 1.0;
    return new AraNoteContentReader(source->noteSet(), range,
                                    playbackRegion->getStartInPlaybackTime(), stretch,
                                    clipStartInSource, clipDurationInSource);
}

bool AraDocumentController::doIsAudioSourceContentAnalysisIncomplete(
    const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType contentType) noexcept
{
    if (contentType != ARA::kARAContentTypeNotes) {
        return false;
    }
    collectFinishedAnalyses();
    const auto* source = static_cast<const AraAudioSource*>(audioSource);
    return source->analysisRunning() || !source->hasNotes();
}

bool AraDocumentController::doRestoreObjectsFromArchive(
    ARA::PlugIn::HostArchiveReader* archiveReader,
    const ARA::PlugIn::RestoreObjectsFilter* filter) noexcept
{
    // Разметка из архива проекта: без неё каждый раз при открытии проекта
    // пришлось бы разбирать дорожки заново (и терять правки высот)
    ArchiveReadCursor cursor { archiveReader };

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!cursor.read(magic) || magic != kArchiveMagic || !cursor.read(version)
        || version != kArchiveVersion) {
        return false;
    }

    std::uint32_t sourceCount = 0;
    if (!cursor.read(sourceCount)) {
        return false;
    }
    for (std::uint32_t i = 0; i < sourceCount; ++i) {
        std::string persistentId;
        AraNoteSet noteSet;
        if (!cursor.readString(persistentId) || !cursor.read(noteSet.sampleRate)) {
            return false;
        }
        std::uint32_t noteCount = 0;
        if (!cursor.read(noteCount)) {
            return false;
        }
        noteSet.notes.resize(noteCount);
        for (TrackPitchNote& note : noteSet.notes) {
            if (!cursor.read(note.startSample) || !cursor.read(note.endSample)
                || !cursor.read(note.midiPitch) || !cursor.read(note.detectedPitch)
                || !cursor.read(note.confidence) || !cursor.read(note.sourceStartSample)
                || !cursor.read(note.sourceEndSample)) {
                return false;
            }
        }
        noteSet.grade = ARA::kARAContentGradeDetected;
        noteSet.valid = !noteSet.notes.empty();

        AraAudioSource* source = filter
            ? filter->getAudioSourceToRestoreStateWithID<AraAudioSource>(persistentId.c_str())
            : nullptr;
        if (!source || !noteSet.valid) {
            continue;  // источник в этот документ не попал — запись пропускаем
        }
        source->setNoteSet(std::move(noteSet));
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            analyzedOrder_.push_back(source);
        }
        notifyAudioSourceContentChanged(source, ARA::ContentUpdateScopes::notesAreAffected());
    }
    return true;
}

bool AraDocumentController::doStoreObjectsToArchive(
    ARA::PlugIn::HostArchiveWriter* archiveWriter,
    const ARA::PlugIn::StoreObjectsFilter* filter) noexcept
{
    collectFinishedAnalyses();

    std::vector<const AraAudioSource*> sources;
    if (filter) {
        for (const AraAudioSource* source : filter->getAudioSourcesToStore<AraAudioSource>()) {
            sources.push_back(source);
        }
    } else if (const ARA::PlugIn::Document* document = getDocument()) {
        for (const ARA::PlugIn::AudioSource* source : document->getAudioSources()) {
            sources.push_back(static_cast<const AraAudioSource*>(source));
        }
    }

    ArchiveWriteCursor cursor { archiveWriter };
    cursor.write(kArchiveMagic);
    cursor.write(kArchiveVersion);
    cursor.write(static_cast<std::uint32_t>(sources.size()));
    for (const AraAudioSource* source : sources) {
        const AraNoteSet& noteSet = source->noteSet();
        cursor.writeString(source->getPersistentID().c_str());
        cursor.write(noteSet.sampleRate);
        cursor.write(static_cast<std::uint32_t>(noteSet.notes.size()));
        for (const TrackPitchNote& note : noteSet.notes) {
            cursor.write(note.startSample);
            cursor.write(note.endSample);
            cursor.write(note.midiPitch);
            cursor.write(note.detectedPitch);
            cursor.write(note.confidence);
            cursor.write(note.sourceStartSample);
            cursor.write(note.sourceEndSample);
        }
    }
    return cursor.ok();
}

} // namespace Dontfloat::Ara
