#include "../dontfloat_plugin_core.h"

#include <iostream>

using Dontfloat::PluginCore::TrackAnalysisOptions;
using Dontfloat::PluginCore::TrackAnalysisResult;
using Dontfloat::PluginCore::TrackAudioInfo;
using Dontfloat::PluginCore::TrackRenderRequest;
using Dontfloat::PluginCore::TrackRenderResult;
using Dontfloat::PluginCore::TrackToolSession;
using Dontfloat::PluginCore::TrackToolStatus;
using Dontfloat::PluginCore::isValidAudioInfo;
using Dontfloat::PluginCore::sanitizeAnalysisOptions;
using Dontfloat::PluginCore::sanitizeAlignmentOptions;
using Dontfloat::PluginCore::sanitizeRenderOptions;

namespace {

bool testAudioInfoValidation()
{
    if (!isValidAudioInfo(TrackAudioInfo{44100, 2, 44100})) {
        return false;
    }
    if (isValidAudioInfo(TrackAudioInfo{0, 2, 44100})) {
        return false;
    }
    if (isValidAudioInfo(TrackAudioInfo{44100, 0, 44100})) {
        return false;
    }
    return !isValidAudioInfo(TrackAudioInfo{44100, 2, -1});
}

bool testPrepareAndAnalyzeStub()
{
    TrackToolSession session;
    TrackAnalysisResult result;
    if (session.analyze(TrackAnalysisOptions{}, &result) != TrackToolStatus::NotPrepared) {
        return false;
    }
    if (session.prepare(TrackAudioInfo{48000, 2, 96000}) != TrackToolStatus::Ok) {
        return false;
    }

    TrackAnalysisOptions options;
    options.minBpm = 220.0f;
    options.maxBpm = 80.0f;
    options.useInitialBpm = true;
    options.initialBpm = 128.0f;
    if (session.analyze(options, &result) != TrackToolStatus::Ok) {
        return false;
    }
    return session.isPrepared()
        && session.analysisValid()
        && result.status == TrackToolStatus::Ok
        && result.bpm == 128.0f
        && result.chroma.size() == 12;
}

bool testRenderStub()
{
    TrackToolSession session;
    TrackRenderResult result;
    if (session.render(TrackRenderRequest{}, &result) != TrackToolStatus::NotPrepared) {
        return false;
    }
    if (session.prepare(TrackAudioInfo{44100, 2, 1000}) != TrackToolStatus::Ok) {
        return false;
    }

    TrackRenderRequest request;
    request.requestedFrameCount = 400;
    request.alignment.beatsPerBar = 99;
    request.render.pitchSemitones = 80.0f;
    if (session.render(request, &result) != TrackToolStatus::Ok) {
        return false;
    }
    if (result.status != TrackToolStatus::Ok || result.framesRendered != 400 || !result.requiresOfflineRender) {
        return false;
    }

    request.requestedFrameCount = -1;
    return session.render(request, &result) == TrackToolStatus::InvalidRenderRequest;
}

bool testSanitizeHelpers()
{
    TrackAnalysisOptions analysis;
    analysis.minBpm = 500.0f;
    analysis.maxBpm = 30.0f;
    const TrackAnalysisOptions sanitizedAnalysis = sanitizeAnalysisOptions(analysis);
    if (sanitizedAnalysis.minBpm != 30.0f || sanitizedAnalysis.maxBpm != 400.0f) {
        return false;
    }

    auto alignment = sanitizeAlignmentOptions({});
    if (alignment.beatsPerBar != 4) {
        return false;
    }
    alignment.beatsPerBar = sanitizeAlignmentOptions({0.0f, 99, true, true}).beatsPerBar;
    if (alignment.beatsPerBar != 32) {
        return false;
    }

    auto render = sanitizeRenderOptions({});
    if (render.pitchSemitones != 0.0f) {
        return false;
    }
    render.pitchSemitones = 100.0f;
    return sanitizeRenderOptions(render).pitchSemitones == 24.0f;
}

} // namespace

int main()
{
    if (!testAudioInfoValidation()) {
        std::cerr << "testAudioInfoValidation failed\n";
        return 1;
    }
    if (!testPrepareAndAnalyzeStub()) {
        std::cerr << "testPrepareAndAnalyzeStub failed\n";
        return 1;
    }
    if (!testRenderStub()) {
        std::cerr << "testRenderStub failed\n";
        return 1;
    }
    if (!testSanitizeHelpers()) {
        std::cerr << "testSanitizeHelpers failed\n";
        return 1;
    }
    return 0;
}
