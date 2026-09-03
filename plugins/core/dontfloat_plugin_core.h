#ifndef DONTFLOAT_PLUGIN_CORE_H
#define DONTFLOAT_PLUGIN_CORE_H

#include <atomic>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dontfloat::PluginCore {

enum class TrackToolStatus {
    Ok,
    InvalidAudioInfo,
    NotPrepared,
    InvalidRenderRequest,
    Unsupported,
};

enum class TrackKey : int {
    Unknown = -1,
    CMajor,
    CMinor,
    CSharpMajor,
    CSharpMinor,
    DMajor,
    DMinor,
    DSharpMajor,
    DSharpMinor,
    EMajor,
    EMinor,
    FMajor,
    FMinor,
    FSharpMajor,
    FSharpMinor,
    GMajor,
    GMinor,
    GSharpMajor,
    GSharpMinor,
    AMajor,
    AMinor,
    ASharpMajor,
    ASharpMinor,
    BMajor,
    BMinor,
};

enum class WavSampleFormat {
    Pcm16,
    Pcm24,
    Float32,
};

struct TrackAudioInfo {
    int sampleRate = 44100;
    int channelCount = 0;
    std::int64_t frameCount = 0;
};

struct TrackBeat {
    std::int64_t positionFrames = 0;
    std::int64_t expectedPositionFrames = 0;
    float confidence = 0.0f;
    float deviationFrames = 0.0f;
    float energy = 0.0f;
};

struct TrackKeyInfo {
    TrackKey key = TrackKey::Unknown;
    float confidence = 0.0f;
    float strength = 0.0f;
    bool isMajor = false;
};

struct TrackAnalysisOptions {
    bool analyzeBpm = true;
    bool analyzeKey = true;
    bool assumeFixedTempo = true;
    bool fastAnalysis = false;
    float minBpm = 60.0f;
    float maxBpm = 200.0f;
    float initialBpm = 0.0f;
    bool useInitialBpm = false;
};

struct TrackAnalysisResult {
    TrackToolStatus status = TrackToolStatus::NotPrepared;
    float bpm = 0.0f;
    float bpmConfidence = 0.0f;
    bool isFixedTempo = true;
    bool hasIrregularBeats = false;
    float averageBeatDeviationFrames = 0.0f;
    std::int64_t gridStartFrame = 0;

    TrackKeyInfo primaryKey;
    TrackKeyInfo secondaryKey;
    float keyConfidence = 0.0f;
    bool hasKeyChange = false;

    std::vector<TrackBeat> beats;
    std::vector<float> chroma;
};

struct TrackMarker {
    std::int64_t positionFrames = 0;
    std::int64_t originalPositionFrames = 0;
    bool fixed = false;
    bool endMarker = false;
};

struct TrackAlignmentOptions {
    float targetBpm = 0.0f;
    int beatsPerBar = 4;
    bool preservePitch = true;
    bool alignToFixedTempoGrid = true;
};

struct TrackRenderOptions {
    bool applyMarkerStretch = true;
    bool applyPitchShift = false;
    float pitchSemitones = 0.0f;
    WavSampleFormat exportFormat = WavSampleFormat::Pcm16;
    bool dither = true;
};

struct TrackRenderRequest {
    TrackAlignmentOptions alignment;
    TrackRenderOptions render;
    std::int64_t requestedFrameCount = 0;
};

struct TrackRenderResult {
    TrackToolStatus status = TrackToolStatus::NotPrepared;
    std::int64_t framesRendered = 0;
    bool requiresOfflineRender = true;
};

struct TrackPitchNote {
    std::int64_t startSample = 0;
    std::int64_t endSample = 0;
    float midiPitch = 60.0f;
    float detectedPitch = 60.0f;
    float confidence = 0.0f;
    /**
     * Откуда берётся звук ноты (−1 — оттуда же, где она нарисована).
     * Заполняется, когда ноту передвинули по времени: коррекция переносит
     * звук с этого места на новое.
     */
    std::int64_t sourceStartSample = -1;
    std::int64_t sourceEndSample = -1;
};

struct TrackAudioBuffer {
    int sampleRate = 44100;
    int channelCount = 0;
    std::vector<float> mono;
    std::vector<float> left;
    std::vector<float> right;
    std::int64_t frameCount() const { return static_cast<std::int64_t>(mono.size()); }
    bool empty() const { return mono.empty(); }
};

struct TrackKeyAnalysis {
    TrackKeyInfo primaryKey;
    TrackKeyInfo secondaryKey;
    bool hasKeyChange = false;
};

struct TrackPitchAnalysis {
    std::vector<TrackPitchNote> notes;
    TrackKeyAnalysis keys;
    bool valid = false;
};

/**
 * Очередь захвата: аудиопоток кладёт сюда блоки, поток интерфейса разгребает.
 *
 * Зачем: раньше `writeHostFrames` прямо из `process()` делал `resize` и
 * `clear` на `std::vector` общего буфера, а поток интерфейса в это же время
 * копировал оттуда сэмплы для отрисовки. Гонка на векторе ломала кучу — DAW
 * падала с c0000374 (под отладчиком) или с обращением по чужому адресу внутри
 * Qt (без него). Теперь общий буфер принадлежит только потоку интерфейса, а
 * аудиопоток пишет в эту очередь: без блокировок и без выделения памяти.
 *
 * Один писатель и один читатель — больше и не бывает: аудиопоток у хоста один.
 * Переполнение (интерфейс надолго завис) роняет блок и поднимает флаг, а не
 * тормозит аудиопоток: захват — дело наживное, DAW пришлёт материал снова.
 */
class HostCaptureQueue {
public:
    HostCaptureQueue();

    /** Один разобранный блок; векторы переиспользуются между вызовами pop. */
    struct Block {
        std::int64_t timelineFrame = -1;
        int channelCount = 0;
        int frameCount = 0;
        std::vector<float> left;
        std::vector<float> right;
    };

    /** Аудиопоток. false — блок не поместился и потерян. */
    bool push(const float* const* inputs, int channelCount, int frameCount,
              std::int64_t timelineFrame) noexcept;

    /** Поток интерфейса. false — очередь пуста. */
    bool pop(Block& out);

    /** Были ли потери с прошлой проверки (флаг сбрасывается). */
    bool takeOverflow() noexcept;

    /** Выбрасывает всё, что не разобрано (сброс захвата). */
    void clear() noexcept;

private:
    /** Заголовок блока; сэмплы лежат в кольце samples_ с этого смещения. */
    struct Header {
        std::int64_t timelineFrame;
        std::int32_t channelCount;
        std::int32_t frameCount;
        std::uint64_t sampleOffset;
    };

    // Секунды стерео на 192 кГц хватает с запасом: интерфейс разгребает
    // очередь на каждое уведомление, то есть десятки раз в секунду
    static constexpr std::size_t kSampleCapacity = 192000u * 2u;
    static constexpr std::size_t kHeaderCapacity = 512u;

    std::size_t freeSamples(std::size_t writePos, std::size_t readPos) const noexcept;

    std::vector<float> samples_;
    std::vector<Header> headers_;
    std::atomic<std::size_t> headerWrite_ { 0 };
    std::atomic<std::size_t> headerRead_ { 0 };
    std::atomic<std::size_t> sampleWrite_ { 0 };
    std::atomic<std::size_t> sampleRead_ { 0 };
    std::atomic<bool> overflow_ { false };
};

class TrackToolSession {
public:
    TrackToolSession() = default;

    void reset();
    TrackToolStatus prepare(const TrackAudioInfo& audioInfo);
    TrackToolStatus setAudioInfo(const TrackAudioInfo& audioInfo);

    TrackToolStatus analyze(const TrackAnalysisOptions& options, TrackAnalysisResult* result);
    TrackToolStatus render(const TrackRenderRequest& request, TrackRenderResult* result);

    TrackToolStatus setAudioBuffer(const TrackAudioBuffer& buffer);
    /** Запись блока в конец захвата (хосты без транспорта, например LV2). */
    TrackToolStatus appendHostFrames(const float* const* inputs, int channelCount, int frameCount);
    /**
     * Запись блока по позиции таймлайна DAW: буфер повторяет дорожку, а не
     * порядок приходов. Благодаря этому сдвиг клипа в DAW виден плагину как
     * сдвиг содержимого (см. detectContentShift).
     * @param timelineFrame позиция начала блока; отрицательная — писать в конец.
     */
    TrackToolStatus writeHostFrames(const float* const* inputs, int channelCount, int frameCount,
                                    std::int64_t timelineFrame);
    /**
     * Разбирает очередь захвата в общий буфер. **Только поток интерфейса.**
     * Всё, что раньше делал writeHostFrames (resize, склейка проходов,
     * пересчёт моно), происходит здесь: аудиопоток общий буфер не трогает.
     * @return true, если что-то пришло — вид пора обновить.
     */
    bool drainHostCapture();
    /** Были ли потери блоков с прошлой проверки (интерфейс не успевал). */
    bool captureOverflowed();
    void clearHostCapture();

    /**
     * Готовый результат работы плагина (коррекция высот, растяжение и т.п.).
     * Его обёртка формата отдаёт в выход `process()` вместо входа — иначе
     * правки слышны только внутри плагина, а DAW играет исходный звук.
     * @param timelineStartFrame позиция результата на таймлайне DAW.
     */
    void setRenderedOutput(const TrackAudioBuffer& buffer, std::int64_t timelineStartFrame = 0);
    void clearRenderedOutput();
    bool hasRenderedOutput() const;
    /** Снимок результата; держит буфер живым, пока живёт возвращённый указатель. */
    std::shared_ptr<const TrackAudioBuffer> renderedOutput() const;
    std::int64_t renderedOutputStart() const
    {
        return renderedOutputStart_.load(std::memory_order_acquire);
    }

    /**
     * Копирует готовый результат в выходной блок.
     * @param timelineFrame позиция блока; отрицательная — считаем от начала.
     * @return false, если на этой позиции результата нет (играем вход как есть).
     */
    bool readRenderedOutput(float* const* outputs, int channelCount, int frameCount,
                            std::int64_t timelineFrame) const;

    const TrackAudioBuffer& audioBuffer() const { return audioBuffer_; }
    const TrackPitchAnalysis& pitchAnalysis() const { return pitchAnalysis_; }
    TrackPitchAnalysis& pitchAnalysis() { return pitchAnalysis_; }

    const TrackAudioInfo& audioInfo() const { return audioInfo_; }
    const TrackAnalysisOptions& analysisOptions() const { return analysisOptions_; }
    const TrackAnalysisResult& analysis() const { return analysis_; }
    const std::vector<TrackMarker>& markers() const { return markers_; }

    std::vector<TrackMarker>& markers() { return markers_; }

    bool isPrepared() const { return prepared_; }
    bool analysisValid() const { return analysisValid_; }
    bool markersValid() const { return markersValid_; }
    std::uint32_t version() const { return version_; }

private:
    /** Применяет один разобранный блок к общему буферу (поток интерфейса). */
    TrackToolStatus applyCaptureBlock(const HostCaptureQueue::Block& block);

    TrackAudioInfo audioInfo_;
    TrackAnalysisOptions analysisOptions_;
    TrackAnalysisResult analysis_;
    TrackAlignmentOptions alignment_;
    TrackRenderOptions renderOptions_;
    std::vector<TrackMarker> markers_;

    /** Мост из аудиопотока: пишет push(), разбирает drainHostCapture(). */
    HostCaptureQueue capture_;
    /** Переиспользуемый приёмник для pop() — чтобы не выделять на каждый блок. */
    HostCaptureQueue::Block captureBlock_;

    TrackAudioBuffer audioBuffer_;
    /**
     * Обработанный звук, который плагин отдаёт в выход (см. setRenderedOutput).
     *
     * Публикуется целиком под новым указателем, а не правится на месте: его
     * читает аудиопоток в readRenderedOutput, и переаллокация векторов под ним
     * ломала кучу — DAW падала так же, как на захвате. Аудиопоток берёт
     * указатель себе, и буфер живёт, пока он с ним работает.
     */
    std::shared_ptr<const TrackAudioBuffer> renderedOutput_;
    /** Прошлый результат: держим, чтобы удалять его в интерфейсе, а не в RT. */
    std::shared_ptr<const TrackAudioBuffer> retiredRenderedOutput_;
    std::atomic<std::int64_t> renderedOutputStart_ { 0 };
    TrackPitchAnalysis pitchAnalysis_;

    /** Конец последней записи по таймлайну: по нему видно новый проход DAW. */
    std::int64_t lastWriteEndFrame_ = 0;
    std::uint32_t version_ = 1;
    bool prepared_ = false;
    bool analysisValid_ = false;
    bool markersValid_ = false;
};

bool isValidAudioInfo(const TrackAudioInfo& audioInfo);
TrackAnalysisOptions sanitizeAnalysisOptions(const TrackAnalysisOptions& options);
TrackAlignmentOptions sanitizeAlignmentOptions(const TrackAlignmentOptions& options);
TrackRenderOptions sanitizeRenderOptions(const TrackRenderOptions& options);

/**
 * Отпечаток содержимого дорожки: где лежит звук и что это за звук.
 * `hash` считается по самому содержимому и не зависит от его позиции —
 * поэтому перемещение клипа в DAW отличимо от смены материала.
 */
struct TrackContentFingerprint {
    std::int64_t startFrame = 0;   ///< первый незвенящий кадр (тишина в начале отброшена)
    std::int64_t lengthFrames = 0; ///< длина содержимого без тишины по краям
    std::uint64_t hash = 0;        ///< хеш содержимого; 0 — тишина/пусто
    bool empty() const { return lengthFrames <= 0; }
};

/** Отпечаток захваченного буфера (тишина по краям отбрасывается). */
TrackContentFingerprint computeContentFingerprint(const TrackAudioBuffer& buffer);

/**
 * Тот же материал, но на другой позиции? Так плагин узнаёт о перемещении
 * клипа: метки растяжения и ноты нужно сдвинуть, а не пересчитывать.
 * @return true и \a deltaFrames (новая позиция минус старая), если содержимое
 *         совпало, а позиция изменилась.
 *
 * @note \a deltaFrames — именно `std::int64_t`, а не `qint64`. Ядро плагина
 *       живёт без Qt, а на Linux (LP64) `int64_t` — это `long`, тогда как
 *       `qint64` — `long long`: размеры совпадают, типы нет, и `qint64*` сюда
 *       не приводится. На Windows и macOS оба типа — `long long`, поэтому
 *       такая ошибка видна только в сборке под Linux.
 */
bool detectContentShift(const TrackContentFingerprint& before,
                        const TrackContentFingerprint& after,
                        std::int64_t* deltaFrames);

} // namespace Dontfloat::PluginCore

#endif // DONTFLOAT_PLUGIN_CORE_H
