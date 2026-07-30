#ifndef DONTFLOAT_PLUGIN_CORE_H
#define DONTFLOAT_PLUGIN_CORE_H

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

class TrackToolSession {
public:
    TrackToolSession() = default;

    void reset();
    TrackToolStatus prepare(const TrackAudioInfo& audioInfo);
    TrackToolStatus setAudioInfo(const TrackAudioInfo& audioInfo);

    TrackToolStatus analyze(const TrackAnalysisOptions& options, TrackAnalysisResult* result);
    TrackToolStatus render(const TrackRenderRequest& request, TrackRenderResult* result);

    TrackToolStatus setAudioBuffer(const TrackAudioBuffer& buffer);
    TrackToolStatus appendHostFrames(const float* const* inputs, int channelCount, int frameCount);
    void clearHostCapture();

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
    TrackAudioInfo audioInfo_;
    TrackAnalysisOptions analysisOptions_;
    TrackAnalysisResult analysis_;
    TrackAlignmentOptions alignment_;
    TrackRenderOptions renderOptions_;
    std::vector<TrackMarker> markers_;

    TrackAudioBuffer audioBuffer_;
    TrackPitchAnalysis pitchAnalysis_;

    std::uint32_t version_ = 1;
    bool prepared_ = false;
    bool analysisValid_ = false;
    bool markersValid_ = false;
};

bool isValidAudioInfo(const TrackAudioInfo& audioInfo);
TrackAnalysisOptions sanitizeAnalysisOptions(const TrackAnalysisOptions& options);
TrackAlignmentOptions sanitizeAlignmentOptions(const TrackAlignmentOptions& options);
TrackRenderOptions sanitizeRenderOptions(const TrackRenderOptions& options);

} // namespace Dontfloat::PluginCore

#endif // DONTFLOAT_PLUGIN_CORE_H
