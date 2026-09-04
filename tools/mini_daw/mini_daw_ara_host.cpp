#include "mini_daw_ara_host.h"

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace Dontfloat::PluginTester {
namespace {

/** Точки карты темпа: две штуки задают постоянный темп дорожки. */
struct TempoMap {
    ARA::ARAContentTempoEntry entries[2] {};
    ARA::ARAContentBarSignature barSignature {};
};

/**
 * Счётчик инициализаций фабрики ARA.
 *
 * ARA требует ровно одного initializeARAWithConfiguration на фабрику: второй
 * вызов ловится ассертом SDK (_usedApiGeneration == 0 в ARAPlug.cpp). Мини-DAW
 * же поднимает по документу на дорожку, и обе дорожки берут фабрику из одного
 * загруженного модуля плагина. Поэтому фабрику инициализирует первый документ,
 * а деинициализирует последний закрывшийся.
 *
 * Все вызовы идут из UI-потока окна, поэтому обычной карты достаточно.
 */
int& factoryUseCount(const ARA::ARAFactory* factory)
{
    static std::map<const ARA::ARAFactory*, int> counts;
    return counts[factory];
}

void retainFactory(const ARA::ARAFactory* factory)
{
    int& count = factoryUseCount(factory);
    if (count == 0) {
        const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAInterfaceConfiguration, assertFunctionAddress)>
            interfaceConfig { ARA::kARAAPIGeneration_2_0_Final, nullptr };
        factory->initializeARAWithConfiguration(&interfaceConfig);
    }
    ++count;
}

void releaseFactory(const ARA::ARAFactory* factory)
{
    int& count = factoryUseCount(factory);
    if (count <= 0) {
        return;
    }
    if (--count == 0) {
        factory->uninitializeARA();
    }
}

} // namespace

/**
 * Доступ к сэмплам дорожек — ради этого ARA и нужна.
 *
 * Дорожек в документе несколько, поэтому объект не помнит ни одной: хост-ссылка
 * источника — это адрес самой AraHostTrack (см. AraHostDocument::addTrack),
 * и ридер просто носит её дальше.
 */
class AraHostAudioAccess : public ARA::Host::AudioAccessControllerInterface {
public:
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef sourceRef,
                                                          bool) noexcept override
    {
        return reinterpret_cast<ARA::ARAAudioReaderHostRef>(sourceRef);
    }

    bool readAudioSamples(ARA::ARAAudioReaderHostRef readerRef,
                          ARA::ARASamplePosition samplePosition,
                          ARA::ARASampleCount samplesPerChannel,
                          void* const buffers[]) noexcept override
    {
        const auto* track = reinterpret_cast<const AraHostTrack*>(readerRef);
        if (!track || !buffers) {
            return false;
        }
        const int channels = track->channelCount();
        for (int channel = 0; channel < channels; ++channel) {
            auto* out = static_cast<float*>(buffers[channel]);
            if (!out) {
                return false;
            }
            const QVector<float>& source = (channel == 0 || track->right.isEmpty())
                ? track->left
                : track->right;
            for (ARA::ARASampleCount i = 0; i < samplesPerChannel; ++i) {
                const qint64 index = samplePosition + i;
                out[i] = (index >= 0 && index < source.size()) ? source[int(index)] : 0.0f;
            }
        }
        return true;
    }

    void destroyAudioReader(ARA::ARAAudioReaderHostRef) noexcept override {}
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

/** Разметка хоста: темп и размер такта из панели мини-DAW (общие на документ). */
class AraHostContentAccess : public ARA::Host::ContentAccessControllerInterface {
public:
    AraHostContentAccess() noexcept { setTempo(120.0, 4); }

    void setTempo(double bpm, int beatsPerBar) noexcept
    {
        const double secondsPerQuarter = 60.0 / std::max(1.0, bpm);
        tempo_.entries[0] = { 0.0, 0.0 };
        tempo_.entries[1] = { secondsPerQuarter, 1.0 };
        tempo_.barSignature = { std::max(1, beatsPerBar), 4, 0.0 };
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

/**
 * Транспорт хоста глазами плагина.
 *
 * Кнопки воспроизведения в редакторе — дублёры кнопок DAW и ходят сюда.
 * Счётчики нужны тесту: иначе проверить, дошло ли нажатие до хоста, можно
 * было бы только в настоящей DAW руками.
 */
class AraHostPlayback : public ARA::Host::PlaybackControllerInterface {
public:
    void requestStartPlayback() noexcept override { ++startRequests; }
    void requestStopPlayback() noexcept override { ++stopRequests; }
    void requestSetPlaybackPosition(ARA::ARATimePosition position) noexcept override
    {
        lastPosition = double(position);
    }
    void requestSetCycleRange(ARA::ARATimePosition, ARA::ARATimeDuration) noexcept override {}
    void requestEnableCycle(bool) noexcept override {}

    int startRequests = 0;
    int stopRequests = 0;
    double lastPosition = -1.0;
};

struct AraHostDocument::Impl {
    /** Одна дорожка документа: её звук и объекты модели ARA. */
    struct TrackEntry {
        AraHostTrack track;
        std::string name;  ///< c_str() из свойств живёт, пока жива запись
        ARA::ARARegionSequenceRef regionSequenceRef = nullptr;
        ARA::ARAAudioSourceRef audioSourceRef = nullptr;
        ARA::ARAAudioModificationRef audioModificationRef = nullptr;
        ARA::ARAPlaybackRegionRef playbackRegionRef = nullptr;
        std::unique_ptr<ARA::Host::PlaybackRenderer> playbackRenderer;
        std::unique_ptr<ARA::Host::EditorRenderer> editorRenderer;
        std::unique_ptr<ARA::Host::EditorView> editorView;
    };

    std::unique_ptr<AraHostAudioAccess> audioAccess;
    AraHostArchiving archiving;
    std::unique_ptr<AraHostContentAccess> contentAccess;
    AraHostModelUpdates modelUpdates;
    AraHostPlayback playback;
    std::unique_ptr<ARA::Host::DocumentControllerHostInstance> hostInstance;
    std::unique_ptr<ARA::Host::DocumentController> documentController;

    ARA::ARAMusicalContextRef musicalContextRef = nullptr;
    // unique_ptr: хост-ссылки указывают внутрь записей, поэтому адреса
    // обязаны пережить добавление соседних дорожек
    std::vector<std::unique_ptr<TrackEntry>> tracks;
    const ARA::ARAFactory* factory = nullptr;
    bool open = false;

    TrackEntry* entry(int index) const
    {
        return (index >= 0 && index < int(tracks.size())) ? tracks[std::size_t(index)].get()
                                                          : nullptr;
    }
};

AraHostDocument::AraHostDocument() : impl_ { std::make_unique<Impl>() } {}

AraHostDocument::~AraHostDocument()
{
    close();
}

bool AraHostDocument::open(const ARA::ARAFactory* factory, QString* error)
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
    close();

    impl_->factory = factory;
    impl_->audioAccess = std::make_unique<AraHostAudioAccess>();
    impl_->contentAccess = std::make_unique<AraHostContentAccess>();

    // Фабрика инициализируется один раз на весь процесс — см. retainFactory
    retainFactory(factory);

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
    // Musical context один на документ: у дорожек мини-DAW общий транспорт
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAMusicalContextProperties, color)>
        musicalContextProperties { "mini-DAW timeline", 0, nullptr };
    impl_->musicalContextRef = controller.createMusicalContext(
        reinterpret_cast<ARA::ARAMusicalContextHostRef>(impl_.get()), &musicalContextProperties);
    controller.endEditing();
    return true;
}

bool AraHostDocument::open(const ARA::ARAFactory* factory, const AraHostTrack& track,
                           QString* error)
{
    if (track.frameCount() <= 0) {
        if (error) {
            *error = QStringLiteral("дорожка пуста — нечего отдавать в ARA");
        }
        return false;
    }
    if (!open(factory, error)) {
        return false;
    }
    if (addTrack(track) < 0) {
        if (error) {
            *error = QStringLiteral("не удалось добавить дорожку в документ ARA");
        }
        return false;
    }
    return true;
}

int AraHostDocument::addTrack(const AraHostTrack& track)
{
    if (!impl_->open || !impl_->documentController || track.frameCount() <= 0) {
        return -1;
    }
    ARA::Host::DocumentController& controller = *impl_->documentController;

    auto entry = std::make_unique<Impl::TrackEntry>();
    entry->track = track;
    entry->name = track.name.isEmpty() ? std::string { "mini-DAW audio" }
                                       : std::string { track.name.toUtf8().constData() };
    Impl::TrackEntry& state = *entry;
    const int index = int(impl_->tracks.size());
    impl_->tracks.push_back(std::move(entry));

    // Темп документа задаёт первая дорожка: транспорт в мини-DAW общий
    if (index == 0) {
        impl_->contentAccess->setTempo(state.track.tempoBpm, state.track.beatsPerBar);
    }

    // Уникальные persistent ID: по ним плагин отличает дорожки в архиве
    const std::string sourceId = "mini-daw-audio-source-" + std::to_string(index);
    const std::string modificationId = "mini-daw-audio-modification-" + std::to_string(index);

    controller.beginEditing();
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARARegionSequenceProperties, color)>
        regionSequenceProperties { state.name.c_str(), index, impl_->musicalContextRef, nullptr };
    state.regionSequenceRef = controller.createRegionSequence(
        reinterpret_cast<ARA::ARARegionSequenceHostRef>(&state), &regionSequenceProperties);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioSourceProperties, merits64BitSamples)>
        audioSourceProperties {
            state.name.c_str(),
            sourceId.c_str(),
            static_cast<ARA::ARASampleCount>(state.track.frameCount()),
            state.track.sampleRate,
            state.track.channelCount(),
            ARA::kARAFalse,
        };
    // Хост-ссылка источника — адрес самой дорожки: по ней читаются сэмплы
    state.audioSourceRef = controller.createAudioSource(
        reinterpret_cast<ARA::ARAAudioSourceHostRef>(&state.track), &audioSourceProperties);

    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAAudioModificationProperties, persistentID)>
        audioModificationProperties { state.name.c_str(), modificationId.c_str() };
    state.audioModificationRef = controller.createAudioModification(
        state.audioSourceRef, reinterpret_cast<ARA::ARAAudioModificationHostRef>(&state),
        &audioModificationProperties);

    const double durationSeconds = double(state.track.frameCount()) / state.track.sampleRate;
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAPlaybackRegionProperties, color)>
        playbackRegionProperties {
            ARA::kARAPlaybackTransformationNoChanges,
            0.0,
            durationSeconds,
            0.0,
            durationSeconds,
            impl_->musicalContextRef,
            state.regionSequenceRef,
            state.name.c_str(),
            nullptr,
        };
    state.playbackRegionRef = controller.createPlaybackRegion(
        state.audioModificationRef, reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&state),
        &playbackRegionProperties);
    controller.endEditing();

    // Доступ к сэмплам и запрос разбора: дальше плагин работает сам
    controller.enableAudioSourceSamplesAccess(state.audioSourceRef, true);
    const ARA::ARAContentType noteContent = ARA::kARAContentTypeNotes;
    controller.requestAudioSourceContentAnalysis(state.audioSourceRef, 1, &noteContent);
    return index;
}

void AraHostDocument::unbindInstance(int trackIndex)
{
    Impl::TrackEntry* state = impl_->entry(trackIndex);
    if (!state) {
        return;
    }
    // Роли экземпляра отпускаем до разрушения клипа — иначе плагин держит
    // ссылку на уже уничтоженный playback region
    if (state->playbackRenderer && state->playbackRegionRef) {
        state->playbackRenderer->removePlaybackRegion(state->playbackRegionRef);
    }
    state->playbackRenderer.reset();
    state->editorRenderer.reset();
    state->editorView.reset();
}

void AraHostDocument::removeTrack(int index)
{
    Impl::TrackEntry* state = impl_->entry(index);
    if (!state || !impl_->documentController) {
        return;
    }
    ARA::Host::DocumentController& controller = *impl_->documentController;
    unbindInstance(index);

    if (state->audioSourceRef) {
        controller.enableAudioSourceSamplesAccess(state->audioSourceRef, false);
    }
    controller.beginEditing();
    if (state->playbackRegionRef) {
        controller.destroyPlaybackRegion(state->playbackRegionRef);
    }
    if (state->audioModificationRef) {
        controller.destroyAudioModification(state->audioModificationRef);
    }
    if (state->audioSourceRef) {
        controller.destroyAudioSource(state->audioSourceRef);
    }
    if (state->regionSequenceRef) {
        controller.destroyRegionSequence(state->regionSequenceRef);
    }
    controller.endEditing();

    // Запись оставляем на месте, но пустой: индексы дорожек не должны съезжать
    state->playbackRegionRef = nullptr;
    state->audioModificationRef = nullptr;
    state->audioSourceRef = nullptr;
    state->regionSequenceRef = nullptr;
    state->track = AraHostTrack {};
}

int AraHostDocument::trackCount() const
{
    return int(impl_->tracks.size());
}

void AraHostDocument::bindInstance(int trackIndex, const void* plugInExtensionInstance)
{
    Impl::TrackEntry* state = impl_->entry(trackIndex);
    if (!plugInExtensionInstance || !state || !state->playbackRegionRef) {
        return;
    }
    const auto* instance =
        static_cast<const ARA::ARAPlugInExtensionInstance*>(plugInExtensionInstance);
    // Клип уходит в роль воспроизведения — по нему плагин находит свой источник
    if (instance->playbackRendererInterface) {
        state->playbackRenderer = std::make_unique<ARA::Host::PlaybackRenderer>(instance);
        state->playbackRenderer->addPlaybackRegion(state->playbackRegionRef);
    }
    // И в роль редактора: настоящий хост назначает ей клипы дорожки даже
    // тогда, когда роль воспроизведения пуста
    if (instance->editorRendererInterface) {
        state->editorRenderer = std::make_unique<ARA::Host::EditorRenderer>(instance);
        state->editorRenderer->addPlaybackRegion(state->playbackRegionRef);
    }
    // И в выбор редактора: так плагин показывает именно эту дорожку
    if (instance->editorViewInterface) {
        state->editorView = std::make_unique<ARA::Host::EditorView>(instance);
        const ARA::ARAPlaybackRegionRef regions[] = { state->playbackRegionRef };
        const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAViewSelection, timeRange)> selection {
            ARA::ARASize { 1 }, regions, ARA::ARASize { 0 }, nullptr, nullptr
        };
        state->editorView->notifySelection(&selection);
    }
}

void AraHostDocument::selectClipOfTrack(int viewTrackIndex, int selectedTrackIndex)
{
    Impl::TrackEntry* view = impl_->entry(viewTrackIndex);
    Impl::TrackEntry* selected = impl_->entry(selectedTrackIndex);
    if (!view || !selected || !view->editorView || !selected->playbackRegionRef) {
        return;
    }
    const ARA::ARAPlaybackRegionRef regions[] = { selected->playbackRegionRef };
    const ARA::SizedStruct<ARA_STRUCT_MEMBER(ARAViewSelection, timeRange)> selection {
        ARA::ARASize { 1 }, regions, ARA::ARASize { 0 }, nullptr, nullptr
    };
    view->editorView->notifySelection(&selection);
}

int AraHostDocument::transportStartRequests() const
{
    return impl_->playback.startRequests;
}

int AraHostDocument::transportStopRequests() const
{
    return impl_->playback.stopRequests;
}

double AraHostDocument::lastRequestedPlaybackPosition() const
{
    return impl_->playback.lastPosition;
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

int AraHostDocument::readNoteCount(int trackIndex) const
{
    Impl::TrackEntry* state = impl_->entry(trackIndex);
    if (!impl_->documentController || !state || !state->audioSourceRef) {
        return 0;
    }
    ARA::Host::DocumentController& controller = *impl_->documentController;
    if (!controller.isAudioSourceContentAvailable(state->audioSourceRef,
                                                  ARA::kARAContentTypeNotes)) {
        return 0;
    }
    const ARA::ARAContentReaderRef reader = controller.createAudioSourceContentReader(
        state->audioSourceRef, ARA::kARAContentTypeNotes, nullptr);
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
        // Фабрику могли уже удержать, а документ — не подняться: счётчик
        // всё равно надо вернуть, иначе фабрика не деинициализируется
        if (impl_->factory) {
            releaseFactory(impl_->factory);
            impl_->factory = nullptr;
        }
        impl_->open = false;
        return;
    }
    ARA::Host::DocumentController& controller = *impl_->documentController;

    // Порядок разрушения задан ARA: сверху вниз по модели, потом контроллер
    for (int i = int(impl_->tracks.size()) - 1; i >= 0; --i) {
        removeTrack(i);
    }
    impl_->tracks.clear();

    controller.beginEditing();
    if (impl_->musicalContextRef) {
        controller.destroyMusicalContext(impl_->musicalContextRef);
    }
    controller.endEditing();
    controller.destroyDocumentController();

    impl_->musicalContextRef = nullptr;
    impl_->documentController.reset();
    impl_->hostInstance.reset();
    if (impl_->factory) {
        releaseFactory(impl_->factory);
        impl_->factory = nullptr;
    }
    impl_->open = false;
}

} // namespace Dontfloat::PluginTester
