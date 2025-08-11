#include "../include/mixxxbpmanalyzer.h"

// Mixxx headers (vendored in thirdparty)
#include "../thirdparty/mixxx/src/analyzer/plugins/analyzerqueenmarybeats.h"
#include "../thirdparty/mixxx/src/analyzer/plugins/analyzersoundtouchbeats.h"
#include "../thirdparty/mixxx/src/analyzer/constants.h"
#include "../thirdparty/mixxx/src/audio/types.h"
#include "../thirdparty/mixxx/src/audio/frame.h"

MixxxBeatAnalysis MixxxBpmAnalyzerFacade::analyzeStereoInterleaved(const QVector<float>& samplesInterleaved,
                                                                  int sampleRate,
                                                                  bool /*fastAnalysis*/) {
    MixxxBeatAnalysis out{};
    out.bpm = 0.0f;
    out.supportsBeatTracking = false;

    if (samplesInterleaved.isEmpty() || sampleRate <= 0) {
        return out;
    }

    // Prefer beat tracking plugin (QM DSP)
    mixxx::AnalyzerQueenMaryBeats qm;
    if (!qm.initialize(mixxx::audio::SampleRate(sampleRate))) {
        return out;
    }

    // Process in chunks of kAnalysisSamplesPerChunk samples (stereo)
    const int totalSamples = samplesInterleaved.size();
    const int chunkSize = static_cast<int>(mixxx::kAnalysisSamplesPerChunk);

    int offset = 0;
    while (offset < totalSamples) {
        int remaining = totalSamples - offset;
        int len = remaining < chunkSize ? remaining : chunkSize;
        const mixxx::CSAMPLE* p = reinterpret_cast<const mixxx::CSAMPLE*>(samplesInterleaved.constData() + offset);
        qm.processSamples(p, len);
        offset += len;
    }

    if (!qm.finalize()) {
        return out;
    }

    // Collect beats
    const auto beats = qm.getBeats();
    if (!beats.isEmpty()) {
        out.supportsBeatTracking = true;
        out.beatPositionsFrames.reserve(beats.size());
        for (const auto& fp : beats) {
            // mixxx::audio::FramePos holds frames (per-channel). Convert to int frames.
            out.beatPositionsFrames.append(static_cast<int>(fp.value()));
        }
    }

    // Fallback constant BPM via SoundTouch if no beats
    if (!out.supportsBeatTracking) {
        mixxx::AnalyzerSoundTouchBeats st;
        if (st.initialize(mixxx::audio::SampleRate(sampleRate))) {
            offset = 0;
            while (offset < totalSamples) {
                int remaining = totalSamples - offset;
                int len = remaining < chunkSize ? remaining : chunkSize;
                const mixxx::CSAMPLE* p = reinterpret_cast<const mixxx::CSAMPLE*>(samplesInterleaved.constData() + offset);
                st.processSamples(p, len);
                offset += len;
            }
            if (st.finalize()) {
                out.bpm = static_cast<float>(st.getBpm().valueOr(0.0));
            }
        }
    }

    // If we have beats, compute predominant BPM using spacing
    if (out.supportsBeatTracking && out.beatPositionsFrames.size() >= 2) {
        // Compute average beat length in frames
        double total = 0.0;
        int count = 0;
        for (int i = 1; i < out.beatPositionsFrames.size(); ++i) {
            int df = out.beatPositionsFrames[i] - out.beatPositionsFrames[i - 1];
            if (df > 0) {
                total += df;
                ++count;
            }
        }
        if (count > 0) {
            double avgFrames = total / double(count);
            double bpm = 60.0 * double(sampleRate) / avgFrames;
            out.bpm = static_cast<float>(bpm);
        }
    }

    return out;
}