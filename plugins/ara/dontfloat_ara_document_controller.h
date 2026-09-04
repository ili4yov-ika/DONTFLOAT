#ifndef DONTFLOAT_ARA_DOCUMENT_CONTROLLER_H
#define DONTFLOAT_ARA_DOCUMENT_CONTROLLER_H

/**
 * ARA 2 (Audio Random Access) для DONTFLOAT.
 *
 * Что даёт ARA по сравнению с обычным плагином:
 *  - звук дорожки читается целиком и в любой момент (`HostAudioReader`), а не
 *    накапливается по блокам во время проигрывания;
 *  - разметка живёт в общей с хостом модели документа: ноты плагина хост может
 *    прочитать сам (`kARAContentTypeNotes`), темп и тактовую сетку плагин
 *    берёт из musical context хоста;
 *  - все экземпляры плагина в одном проекте DAW обслуживает **один**
 *    document controller, поэтому ноты дорожки A видны экземпляру на дорожке B —
 *    на этом построен показ референсных нот с соседней дорожки.
 */

#include "ARA_Library/PlugIn/ARAPlug.h"

#include "../core/dontfloat_plugin_core.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Dontfloat::Ara {

/** Тип содержимого, который мы разбираем: ноты. */
constexpr ARA::ARAContentType kNoteContentType = ARA::kARAContentTypeNotes;

/** Тактовая сетка хоста, прочитанная из musical context ARA. */
struct AraBeatGrid {
    double tempoBpm = 120.0;
    int beatsPerBar = 4;
    /** Секунда, с которой начинается такт 1. */
    double gridStartSeconds = 0.0;
    bool valid = false;
};

/** Ноты одного аудиоисточника: результат нашего анализа в модели ARA. */
struct AraNoteSet {
    std::vector<Dontfloat::PluginCore::TrackPitchNote> notes;
    double sampleRate = 44100.0;
    ARA::ARAContentGrade grade = ARA::kARAContentGradeInitial;
    bool valid = false;
};

/**
 * Клип на дорожке глазами плагина: где он стоит и какой кусок источника берёт.
 * По этим полям редактор понимает разрез, перенос и растяжение клипа.
 */
struct AraClipPlacement {
    double startInPlaybackSeconds = 0.0;
    double durationInPlaybackSeconds = 0.0;
    double startInSourceSeconds = 0.0;
    double durationInSourceSeconds = 0.0;
    /** Коэффициент растяжения: длительность в проекте к длительности в источнике. */
    double stretchFactor() const
    {
        return durationInSourceSeconds > 0.0
            ? durationInPlaybackSeconds / durationInSourceSeconds
            : 1.0;
    }
};

/** Аудиоисточник ARA с нашими нотами. */
class AraAudioSource : public ARA::PlugIn::AudioSource {
public:
    using ARA::PlugIn::AudioSource::AudioSource;

    void setNoteSet(AraNoteSet noteSet) noexcept;
    const AraNoteSet& noteSet() const noexcept { return noteSet_; }
    bool hasNotes() const noexcept { return noteSet_.valid; }

    /**
     * Моно-копия звука дорожки, прочитанная через ARA.
     * Именно она позволяет редактору показать волну и слушать ноты **без**
     * проигрывания в DAW: захват блоками больше не нужен.
     */
    void setMonoSamples(std::vector<float> samples) noexcept;
    const std::vector<float>& monoSamples() const noexcept { return monoSamples_; }
    /**
     * Готовы ли сэмплы для чтения из аудиопотока.
     *
     * Вектор наполняется один раз в главном потоке и дальше не меняется,
     * но пока он наполняется, читать его нельзя — отсюда флаг.
     */
    bool samplesReady() const noexcept { return samplesReady_.load(std::memory_order_acquire); }

    /** Ход разбора 0..100 — для плашки прогресса в редакторе. */
    int analysisProgress() const noexcept { return analysisProgress_.load(); }
    void setAnalysisProgress(int percent) noexcept { analysisProgress_.store(percent); }

    /** Идёт ли фоновый разбор этого источника. */
    bool analysisRunning() const noexcept { return analysisRunning_; }
    void setAnalysisRunning(bool running) noexcept { analysisRunning_ = running; }

private:
    AraNoteSet noteSet_;
    std::vector<float> monoSamples_;
    std::atomic<bool> samplesReady_ { false };
    std::atomic<int> analysisProgress_ { 0 };
    bool analysisRunning_ = false;
};

/**
 * Роль воспроизведения: через неё экземпляр плагина узнаёт свои клипы, а по
 * ним — свой аудиоисточник (у ARA нет прямой связи «экземпляр → дорожка»).
 */
class AraPlaybackRenderer : public ARA::PlugIn::PlaybackRenderer {
public:
    using ARA::PlugIn::PlaybackRenderer::PlaybackRenderer;

    /**
     * Отдаёт звук назначенных клипов в выходной блок.
     *
     * В роли ARA-рендерера хост не подаёт исходный звук на вход и ждёт, что
     * дорожку выдаст плагин. Пока этого не было, под ARA дорожка молчала:
     * роль заявлена, а выход пустой.
     *
     * Вызывается из аудиопотока: ничего не выделяет и не блокирует.
     *
     * timelineStartFrame — начало блока на таймлайне проекта в кадрах,
     * sampleRate — частота хоста. Возвращает true, если что-то отдал.
     */
    bool renderBlock(float* const* outputs, int channelCount, int frameCount,
                     std::int64_t timelineStartFrame, double sampleRate) noexcept;

    /** Сколько клипов назначил хост этому рендереру. */
    std::size_t assignedRegionCount() const noexcept { return getPlaybackRegions().size(); }

    /** Сколько кадров реально отдано в выход — для диагностики (см. Diagnostics). */
    std::int64_t renderedFrames() const noexcept
    {
        return renderedFrames_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::int64_t> renderedFrames_ { 0 };
};

/** Роль редактора: выбор в хосте и связь с клипами дорожки. */
class AraEditorView : public ARA::PlugIn::EditorView {
public:
    using ARA::PlugIn::EditorView::EditorView;

    /**
     * Окно редактора открыто. ARA разрешает спрашивать выбор хоста только в
     * этом состоянии, поэтому храним флаг рядом с оболочкой ARA.
     */
    bool isEditorOpen() const noexcept { return editorOpen_; }
    void setEditorOpenState(bool open) noexcept
    {
        editorOpen_ = open;
        setEditorOpen(open);
    }

private:
    bool editorOpen_ = false;
};

/**
 * Document controller: один на весь проект DAW (на все экземпляры плагина).
 */
class AraDocumentController : public ARA::PlugIn::DocumentController {
public:
    using ARA::PlugIn::DocumentController::DocumentController;

    /** Фабрика ARA — её отдают обёртки VST3 и CLAP. */
    static const ARA::ARAFactory* getARAFactory() noexcept;

    /**
     * Ноты **другого** источника документа — референс для соседней дорожки.
     * Берётся последний разобранный источник, отличный от \a exclude;
     * пустой набор, если такого нет.
     */
    AraNoteSet referenceNotesExcluding(const ARA::PlugIn::AudioSource* exclude) const noexcept;

    /** Разбор запускается сам при появлении источника (как это делает Melodyne). */
    void analyzeIfNeeded(AraAudioSource* audioSource) noexcept;

    /**
     * Темп и размер такта из musical context хоста — вместо чтения транспорта.
     * Возвращает valid == false, если хост карту темпа не отдаёт.
     */
    AraBeatGrid hostBeatGrid() const noexcept;

    /** Клипы (playback regions) этого источника: разрез, перенос, растяжение. */
    /**
     * Просит хост начать или остановить воспроизведение.
     *
     * Под ARA у плагина нет своего транспорта: дорожку играет DAW. Кнопка
     * воспроизведения в редакторе поэтому не «слушает кусок у себя», а
     * управляет транспортом хоста — тогда и каретки совпадают, и слышно то
     * же, что видно.
     *
     * Возвращает false, если хост управление транспортом не отдал.
     */
    bool requestHostPlayback(bool start) noexcept;

    /** Просит хост встать в позицию (секунды таймлайна проекта). */
    bool requestHostPlaybackPosition(double seconds) noexcept;

    std::vector<AraClipPlacement> clipsForAudioSource(
        const ARA::PlugIn::AudioSource* audioSource) const noexcept;

    /** Счётчик правок разметки: редактор по нему понимает, что пора обновиться. */
    std::uint64_t modelRevision() const noexcept { return modelRevision_.load(); }

    /**
     * Источник, с которым работает конкретный экземпляр плагина: ищется по
     * клипам, назначенным его ролям (сначала редактор, потом воспроизведение).
     */
    static AraAudioSource* audioSourceForInstance(
        const ARA::PlugIn::PlugInExtension& extension) noexcept;

    /**
     * Клип **этого** экземпляра: сначала регионы его ролей, и только потом
     * выбор в DAW.
     *
     * Обе половины окна обязаны переводить каретку по одному и тому же
     * клипу. Раньше каждая брала первый клип источника (clipsForAudioSource
     * и front), а после разреза клипов их у источника несколько, и половины
     * начинали считать в разных координатах — каретки расходились.
     */
    static bool clipForInstance(const ARA::PlugIn::PlugInExtension& extension,
                                AraClipPlacement* placement) noexcept;

    /**
     * Единственный источник документа — запасной путь, когда хост ещё не
     * назначил экземпляру клипы (роли приходят позже привязки).
     */
    AraAudioSource* onlyAudioSource() const noexcept;

protected:
    ARA::PlugIn::AudioSource* doCreateAudioSource(ARA::PlugIn::Document* document,
                                                 ARA::ARAAudioSourceHostRef hostRef) noexcept override;
    void willDestroyAudioSource(ARA::PlugIn::AudioSource* audioSource) noexcept override;

    void willEnableAudioSourceSamplesAccess(ARA::PlugIn::AudioSource* audioSource,
                                            bool enable) noexcept override;
    void didEnableAudioSourceSamplesAccess(ARA::PlugIn::AudioSource* audioSource,
                                           bool enable) noexcept override;

    /** Хост сообщил, что его разметка поменялась. Мы её не кэшируем — принимать
     *  нечего, но методы обязательны к реализации. */
    void doUpdateMusicalContextContent(ARA::PlugIn::MusicalContext* musicalContext,
                                       const ARA::ARAContentTimeRange* range,
                                       ARA::ContentUpdateScopes scopeFlags) noexcept override;
    void doUpdateAudioSourceContent(ARA::PlugIn::AudioSource* audioSource,
                                    const ARA::ARAContentTimeRange* range,
                                    ARA::ContentUpdateScopes scopeFlags) noexcept override;

    bool doIsAudioSourceContentAvailable(const ARA::PlugIn::AudioSource* audioSource,
                                         ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade doGetAudioSourceContentGrade(const ARA::PlugIn::AudioSource* audioSource,
                                                     ARA::ARAContentType type) noexcept override;
    ARA::PlugIn::ContentReader* doCreateAudioSourceContentReader(
        ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type,
        const ARA::ARAContentTimeRange* range) noexcept override;

    bool doIsAudioModificationContentAvailable(const ARA::PlugIn::AudioModification* audioModification,
                                               ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade doGetAudioModificationContentGrade(
        const ARA::PlugIn::AudioModification* audioModification,
        ARA::ARAContentType type) noexcept override;
    ARA::PlugIn::ContentReader* doCreateAudioModificationContentReader(
        ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type,
        const ARA::ARAContentTimeRange* range) noexcept override;

    /** Содержимое клипа: ноты в координатах проекта с учётом растяжения. */
    bool doIsPlaybackRegionContentAvailable(const ARA::PlugIn::PlaybackRegion* playbackRegion,
                                            ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade doGetPlaybackRegionContentGrade(
        const ARA::PlugIn::PlaybackRegion* playbackRegion,
        ARA::ARAContentType type) noexcept override;
    ARA::PlugIn::ContentReader* doCreatePlaybackRegionContentReader(
        ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type,
        const ARA::ARAContentTimeRange* range) noexcept override;

    bool doIsAudioSourceContentAnalysisIncomplete(const ARA::PlugIn::AudioSource* audioSource,
                                                  ARA::ARAContentType contentType) noexcept override;
    void doRequestAudioSourceContentAnalysis(
        ARA::PlugIn::AudioSource* audioSource,
        std::vector<ARA::ARAContentType> const& contentTypes) noexcept override;

    /** Хост опрашивает обновления модели — здесь забираем готовый разбор. */
    void willNotifyModelUpdates() noexcept override;

    ARA::PlugIn::PlaybackRenderer* doCreatePlaybackRenderer() noexcept override;
    ARA::PlugIn::EditorView* doCreateEditorView() noexcept override;

    /** Пишет положение клипа в дневник диагностики (см. Diagnostics). */
    static void logRegionToDiagnostics(const char* event,
                                       const ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept;

    /** Клип добавили на дорожку — разбираем его источник, если ещё не разбирали. */
    void didAddPlaybackRegionToRegionSequence(ARA::PlugIn::RegionSequence* regionSequence,
                                              ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;
    /** Клип подвинули, обрезали или растянули — разметка встаёт по-новому. */
    void didUpdatePlaybackRegionProperties(
        ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;
    /** Хост завёл источник (новый клип с новым файлом) — сразу в разбор. */
    void didAddAudioSourceToDocument(ARA::PlugIn::Document* document,
                                     ARA::PlugIn::AudioSource* audioSource) noexcept override;

    bool doRestoreObjectsFromArchive(ARA::PlugIn::HostArchiveReader* archiveReader,
                                     const ARA::PlugIn::RestoreObjectsFilter* filter) noexcept override;
    bool doStoreObjectsToArchive(ARA::PlugIn::HostArchiveWriter* archiveWriter,
                                 const ARA::PlugIn::StoreObjectsFilter* filter) noexcept override;

private:
    struct PendingAnalysis {
        AraAudioSource* audioSource = nullptr;
        std::future<AraNoteSet> future;
    };

    /** Забирает результаты завершённых разборов в модель (главный поток). */
    void collectFinishedAnalyses() noexcept;

    /** Растёт на каждое изменение разметки: и от разбора, и от правок клипов. */
    std::atomic<std::uint64_t> modelRevision_ { 0 };
    mutable std::mutex mutex_;
    std::vector<PendingAnalysis> pendingAnalyses_;
    /** Порядок появления нот: последний разобранный источник идёт референсом. */
    std::vector<AraAudioSource*> analyzedOrder_;
};

} // namespace Dontfloat::Ara

#endif // DONTFLOAT_ARA_DOCUMENT_CONTROLLER_H
