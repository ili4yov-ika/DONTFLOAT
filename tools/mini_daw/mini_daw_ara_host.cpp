#include "mini_daw_ara_host.h"

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>

namespace Dontfloat::PluginTester {
namespace {

/** Точки карты темпа: две штуки задают постоянный темп дорожки. */
struct TempoMap {
    ARA::ARAContentTempoEntry entries[2] {};
    ARA::ARAContentBarSignature barSignature {};
};

} // namespace

/** Доступ к сэмплам дорожки — ради этого ARA и нужна. */
class AraHostAudioAccess : public ARA::Host::AudioAccessControllerInterface {
public:
    explicit AraHostAudioAccess(const AraHostTrack* track) noexcept : track_ { track } {}

    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef,
                                                          bool) noexcept override
    {
        return reinterpret_cast<ARA::ARAAudioReaderHostRef>(const_cast<AraHostTrack*>(track_));
    }

    bool readAudioSamples(ARA::ARAAudioReaderHostRef, ARA::ARASamplePosition samplePosition,
                          ARA::ARASampleCount samplesPerChannel,
                          void* const buffers[]) noexcept override
    {
        if (!track_ || !buffers) {
            return false;
        }
        const int channels = track_->channelCount();
        for (int channel = 0; channel < channels; ++channel) {
            auto* out = static_cast<float*>(buffers[channel]);
            if (!out) {
                return false;
            }
            const QVector<float>& source = (channel == 0 || track_->right.isEmpty())
                ? track_->left
                : track_->right;
            for (ARA::ARASampleCount i = 0; i < samplesPerChannel; ++i) {
                const qint64 index = samplePosition + i;
                out[i] = (index >= 0 && index < source.size()) ? source[int(index)] : 0.0f;
            }
        }
        return true;
    }

    void destroyAudioReader(ARA::ARAAudioReaderHostRef) noexcept override {}

private:
    const AraHostTrack* track_ = nullptr;
};

/** Архивы мини-DAW не хранит: проект живёт только в памяти. */
class AraHostArchiving : public ARA::Host::ArchivingControllerInterface {
public:
    ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef) noexcept override { return 0; }
    bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef, ARA::ARASize, ARA::ARASize,
                              ARA::ARAByte[]) noexcept override
    {
        return false;
    }
    bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef, ARA::ARASize, ARA::ARASize,
                             const ARA::ARAByte[]) noexcept override
    {
        return true;
    }
    void notifyDocumentArchivingProgress(float) noexcept override {}
    void notifyDocumentUnarchivingProgress(float) noexcept override {}
    ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef) noexcept override
    {
        return nullptr;
    }
};

/** Разметка хоста: темп и размер такта из панели мини-DAW. */
class AraHostContentAccess : public ARA::Host::ContentAccessControllerInterface {
public:
    explicit AraHostContentAccess(const AraHostTrack* track) noexcept : track_ { track }
    {
        rebuild();
    }

    void rebuild() noexcept
    {
        if (!track_) {
            return;
        }
        const double secondsPerQuarter = 60.0 / std::max(1.0, track_->tempoBpm);
        tempo_.entries[0] = { 0.0, 0.0 };
        tempo_.entries[1] = { secondsPerQuarter, 1.0 };
        tempo_.barSignature = { track_->beatsPerBar, 4, 0.0 };
    }

    bool isMusicalContextContentAvailable(ARA::ARAMusicalContextHostRef,
                                          ARA::ARAContentType type) noexcept override
    {
        return type == ARA::kARAContentTypeTempoEntries
            || type == ARA::kARAContentTypeBarSignatures;
    }

    ARA::ARAContentGrade getMusicalContextContentGrade(ARA::ARAMusicalContextHostRef,
                                                       ARA::ARAContentType) noexcept override
    {
        return ARA::kARAContentGradeAdjusted;
    }

    ARA::ARAContentReaderHostRef createMusicalContextContentReader(
        ARA::ARAMusicalContextHostRef, ARA::ARAContentType type,
        const ARA::ARAContentTimeRange*) noexcept override
    {
        return reinterpret_cast<ARA::ARAContentReaderHostRef>(
            static_cast<std::uintptr_t>(type));
    }

    bool isAudioSourceContentAvailable(ARA::ARAAudioSourceHostRef,
                                       ARA::ARAContentType) noexcept override
    {
        return false;  // свою разметку дорожки хост не отдаёт — её делает плагин
    }
    ARA::ARAContentGrade getAudioSourceContentGrade(ARA::ARAAudioSourceHostRef,
                                                    ARA::ARAContentType) noexcept override
    {
        return ARA::kARAContentGradeInitial;
    }
    ARA::ARAContentReaderHostRef createAudioSourceContentReader(
        ARA::ARAAudioSourceHostRef, ARA::ARAContentType,
        const ARA::ARAContentTimeRange*) noexcept override
    {
        return nullptr;
    }

    ARA::ARAInt32 getContentReaderEventCount(ARA::ARAContentReaderHostRef readerRef) noexcept override
    {
        switch (contentTypeOf(readerRef)) {
        case ARA::kARAContentTypeTempoEntries:
            return 2;
        case ARA::kARAContentTypeBarSignatures:
            return 1;
        default:
            return 0;
        }
    }

    const void* getContentReaderDataForEvent(ARA::ARAContentReaderHostRef readerRef,
                                             ARA::ARAInt32 eventIndex) noexcept override
    {
        switch (contentTypeOf(readerRef)) {
        case ARA::kARAContentTypeTempoEntries:
            return (eventIndex >= 0 && eventIndex < 2) ? &tempo_.entries[eventIndex] : nullptr;
        case ARA::kARAContentTypeBarSignatures:
            return (eventIndex == 0) ? &tempo_.barSignature : nullptr;
        default:
            return nullptr;
        }
    }

    void destroyContentReader(ARA::ARAContentReaderHostRef) noexcept override {}

private:
    static ARA::ARAContentType contentTypeOf(ARA::ARAContentReaderHostRef ref) noexcept
    {
        return static_cast<ARA::ARAContentType>(reinterpret_cast<std::uintptr_t>(ref));
    }

    const AraHostTrack* track_ = nullptr;
    TempoMap tempo_;
};

/** Сюда плагин сообщает о ходе разбора и об изменившейся разметке. */
class AraHostModelUpdates : public ARA::Host::ModelUpdateControllerInterface {
public:
    void notifyAudioSourceAnalysisProgress(ARA::ARAAudioSourceHostRef,
                                           ARA::ARAAnalysisProgressState state,
                                           float) noexcept override
    {
        if (state == ARA::kARAAnalysisProgressCompleted) {
            analysisCompleted = true;
        }
    }
    void notifyAudioSourceContentChanged(ARA::ARAAudioSourceHostRef,
                                         const ARA::ARAContentTimeRange*,
                                         ARA::ContentUpdateScopes) noexcept override
    {
        ++contentUpdates;
    }
    void notifyAudioModificationContentChanged(ARA::ARAAudioModificationHostRef,
                                               const ARA::ARAContentTimeRange*,
                                               ARA::ContentUpdateScopes) noexcept override
    {
    }
    void notifyPlaybackRegionContentChanged(ARA::ARAPlaybackRegionHostRef,
                                            const ARA::ARAContentTimeRange*,
                                            ARA::ContentUpdateScopes) noexcept override
    {
    }
    void notifyDocumentDataChanged() noexcept override {}

    bool analysisCompleted = false;
    int contentUpdates = 0;
};

/** Транспортом из плагина мини-DAW пока не управляет. */
class AraHostPlayback : public ARA::Host::PlaybackControllerInterface {
public:
    void requestStartPlayback() noexcept override {}
    void requestStopPlayback() noexcept override {}
    void requestSetPlaybackPosition(ARA::ARATimePosition) noexcept override {}
    void requestSetCycleRange(ARA::ARATimePosition, ARA::ARATimeDuration) noexcept override {}
    void requestEnableCycle(bool) noexcept override {}
};

struct AraHostDocument::Impl {
    AraHostTrack track;
    std::unique_ptr<AraHostAudioAccess> audioAccess;
    AraHostArchiving archiving;
    std::unique_ptr<AraHostContentAccess> contentAccess;
    AraHostModelUpdates modelUpdates;
    AraHostPlayback playback;
    std::unique_ptr<ARA::Host::DocumentControllerHostInstance> hostInstance;
    std::unique_ptr<ARA::Host::DocumentController> documentController;

    ARA::ARAMusicalContextRef musicalContextRef = nullptr;
    ARA::ARARegionSequenceRef regionSequenceRef = nullptr;
    ARA::ARAAudioSourceRef audioSourceRef = nullptr;
    ARA::ARAAudioModificationRef audioModificationRef = nullptr;
    ARA::ARAPlaybackRegionRef playbackRegionRef = nullptr;
    const ARA::ARAFactory* factory = nullptr;
    bool open = false;
};

AraHostDocument::AraHostDocument() : impl_ { std::make_unique<Impl>() } {}

AraHostDocument::~AraHostDocument()
{
    close();
}

bool AraHostDocument::open(const ARA::ARAFactory* factory, const AraHostTrack& track,
                           QString* error)
{
    const auto fail = [error](const QString& text) {
        if (error) {
            *error = text;
        }
        return false;
    };

    if (!factory) {
        return fail(QStringLiteral("плагин не отдал фабрику ARA"));
    }
    if (track.frameCount() <= 0) {
        return fail(QStringLiteral("дорожка пуста — нечего отдавать в ARA"));
    }
    close();

    impl_->track = track;
    impl_->factory = factory;
    impl_->audioAccess = std::make_unique<AraHostAudioAccess>(&impl_->track);
    impl_->contentAccess = std::make_unique<AraHostContentAccess>(&impl_->track);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAInterfaceConfiguration, assertFunctionAddress)>
        interfaceConfig { ARA::kARAAPIGeneration_2_0_Final, nullptr };
    factory->initializeARAWithConfiguration(&interfaceConfig);

    impl_->hostInstance = std::make_unique<ARA::Host::DocumentControllerHostInstance>(
        impl_->audioAccess.get(), &impl_->archiving, impl_->contentAccess.get(),
        &impl_->modelUpdates, &impl_->playback);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARADocumentProperties, name)> documentProperties {
        "DONTFLOAT mini-DAW"
    };
    const ARA::ARADocumentControllerInstance* instance =
        factory->createDocumentControllerWithDocument(impl_->hostInstance.get(),
                                                      &documentProperties);
    if (!instance) {
        return fail(QStringLiteral("createDocumentControllerWithDocument вернул nullptr"));
    }
    impl_->documentController = std::make_unique<ARA::Host::DocumentController>(instance);
    impl_->open = true;

    ARA::Host::DocumentController& controller = *impl_->documentController;
    controller.beginEditing();

    // Musical context: темп и размер такта, как их показывает панель мини-DAW
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAMusicalContextProperties, color)>
        musicalContextProperties { "mini-DAW timeline", 0, nullptr };
    impl_->musicalContextRef = controller.createMusicalContext(
        reinterpret_cast<ARA::ARAMusicalContextHostRef>(impl_.get()), &musicalContextProperties);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARARegionSequenceProperties, color)>
        regionSequenceProperties { "mini-DAW track", 0, impl_->musicalContextRef, nullptr };
    impl_->regionSequenceRef = controller.createRegionSequence(
        reinterpret_cast<ARA::ARARegionSequenceHostRef>(impl_.get()), &regionSequenceProperties);

    const std::string trackName = impl_->track.name.isEmpty()
        ? std::string { "mini-DAW audio" }
        : impl_->track.name.toStdString();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioSourceProperties, merits64BitSamples)>
        audioSourceProperties {
            trackName.c_str(),
            "mini-daw-audio-source",
            static_cast<ARA::ARASampleCount>(impl_->track.frameCount()),
            impl_->track.sampleRate,
            impl_->track.channelCount(),
            ARA::kARAFalse,
        };
    impl_->audioSourceRef = controller.createAudioSource(
        reinterpret_cast<ARA::ARAAudioSourceHostRef>(&impl_->track), &audioSourceProperties);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioModificationProperties, persistentID)>
        audioModificationProperties { trackName.c_str(), "mini-daw-audio-modification" };
    impl_->audioModificationRef = controller.createAudioModification(
        impl_->audioSourceRef, reinterpret_cast<ARA::ARAAudioModificationHostRef>(impl_.get()),
        &audioModificationProperties);

    const double durationSeconds = double(impl_->track.frameCount()) / impl_->track.sampleRate;
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAPlaybackRegionProperties, color)>
        playbackRegionProperties {
            ARA::kARAPlaybackTransformationNoChanges,
            0.0,
            durationSeconds,
            0.0,
            durationSeconds,
            impl_->musicalContextRef,
            impl_->regionSequenceRef,
            trackName.c_str(),
            nullptr,
        };
    impl_->playbackRegionRef = controller.createPlaybackRegion(
        impl_->audioModificationRef, reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(impl_.get()),
        &playbackRegionProperties);

    controller.endEditing();

    // Доступ к сэмплам и запрос разбора: дальше плагин работает сам
    controller.enableAudioSourceSamplesAccess(impl_->audioSourceRef, true);
    const ARA::ARAContentType noteContent = ARA::kARAContentTypeNotes;
    controller.requestAudioSourceContentAnalysis(impl_->audioSourceRef, 1, &noteContent);
    return true;
}

void* AraHostDocument::documentControllerRef() const
{
    if (!impl_->documentController) {
        return nullptr;
    }
    return impl_->documentController->getRef();
}

unsigned long long AraHostDocument::knownRoles()
{
    return ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole;
}

unsigned long long AraHostDocument::assignedRoles()
{
    return ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole;
}

void AraHostDocument::pumpModelUpdates()
{
    if (impl_->documentController) {
        impl_->documentController->notifyModelUpdates();
    }
}

bool AraHostDocument::analysisCompleted() const
{
    return impl_->modelUpdates.analysisCompleted;
}

int AraHostDocument::readNoteCount() const
{
    if (!impl_->documentController || !impl_->audioSourceRef) {
        return 0;
    }
    ARA::Host::DocumentController& controller = *impl_->documentController;
    if (!controller.isAudioSourceContentAvailable(impl_->audioSourceRef,
                                                  ARA::kARAContentTypeNotes)) {
        return 0;
    }
    const ARA::ARAContentReaderRef reader = controller.createAudioSourceContentReader(
        impl_->audioSourceRef, ARA::kARAContentTypeNotes, nullptr);
    if (!reader) {
        return 0;
    }
    const int count = int(controller.getContentReaderEventCount(reader));
    controller.destroyContentReader(reader);
    return count;
}

void AraHostDocument::close()
{
    if (!impl_->open || !impl_->documentController) {
        impl_->open = false;
        return;
    }
    ARA::Host::DocumentController& controller = *impl_->documentController;

    if (impl_->audioSourceRef) {
        controller.enableAudioSourceSamplesAccess(impl_->audioSourceRef, false);
    }
    // Порядок разрушения задан ARA: сверху вниз по модели, потом контроллер
    controller.beginEditing();
    if (impl_->playbackRegionRef) {
        controller.destroyPlaybackRegion(impl_->playbackRegionRef);
    }
    if (impl_->audioModificationRef) {
        controller.destroyAudioModification(impl_->audioModificationRef);
    }
    if (impl_->audioSourceRef) {
        controller.destroyAudioSource(impl_->audioSourceRef);
    }
    if (impl_->regionSequenceRef) {
        controller.destroyRegionSequence(impl_->regionSequenceRef);
    }
    if (impl_->musicalContextRef) {
        controller.destroyMusicalContext(impl_->musicalContextRef);
    }
    controller.endEditing();
    controller.destroyDocumentController();

    impl_->playbackRegionRef = nullptr;
    impl_->audioModificationRef = nullptr;
    impl_->audioSourceRef = nullptr;
    impl_->regionSequenceRef = nullptr;
    impl_->musicalContextRef = nullptr;
    impl_->documentController.reset();
    impl_->hostInstance.reset();
    if (impl_->factory) {
        impl_->factory->uninitializeARA();
        impl_->factory = nullptr;
    }
    impl_->open = false;
}

} // namespace Dontfloat::PluginTester
