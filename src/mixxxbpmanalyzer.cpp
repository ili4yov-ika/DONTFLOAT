#include "../include/mixxxbpmanalyzer.h"

#include <vector>
#include <cmath>

// QM-DSP headers (vendored)
#include "../thirdparty/mixxx/lib/qm-dsp/dsp/onsets/DetectionFunction.h"
#include "../thirdparty/mixxx/lib/qm-dsp/dsp/tempotracking/TempoTrackV2.h"
#include "../thirdparty/mixxx/lib/qm-dsp/maths/MathUtilities.h"

namespace {
// ~12 ms step (as used by Mixxx QM analyzer)
constexpr float kStepSecs = 0.01161f;
// Max bin size used to derive window size (similar to Mixxx)
constexpr int kMaximumBinSizeHz = 50; // Hz

// Downmix interleaved stereo float32 to mono double
inline void downmixStereoToMono(const float* interleaved, int frames, std::vector<double>& mono) {
    mono.resize(frames);
    for (int i = 0; i < frames; ++i) {
        const float l = interleaved[i * 2];
        const float r = interleaved[i * 2 + 1];
        mono[i] = 0.5 * (static_cast<double>(l) + static_cast<double>(r));
    }
}
}

MixxxBeatAnalysis MixxxBpmAnalyzerFacade::analyzeStereoInterleaved(const QVector<float>& samplesInterleaved,
                                                                  int sampleRate,
                                                                  bool /*fastAnalysis*/) {
    MixxxBeatAnalysis out{};
    out.bpm = 0.0f;
    out.supportsBeatTracking = false;

    if (samplesInterleaved.isEmpty() || sampleRate <= 0) {
        return out;
    }

    const int totalSamples = samplesInterleaved.size();
    const int totalFrames = totalSamples / 2; // stereo interleaved

    const int stepSizeFrames = static_cast<int>(sampleRate * kStepSecs);
    const int windowSize = MathUtilities::nextPowerOfTwo(sampleRate / kMaximumBinSizeHz);

    // Prepare detection function
    DFConfig cfg;
    cfg.DFType = DF_COMPLEXSD;
    cfg.stepSize = stepSizeFrames;
    cfg.frameLength = windowSize;
    cfg.dbRise = 3;
    cfg.adaptiveWhitening = false;
    cfg.whiteningRelaxCoeff = -1;
    cfg.whiteningFloor = -1;

    DetectionFunction df(cfg);

    // Downmix to mono double
    std::vector<double> mono;
    downmixStereoToMono(samplesInterleaved.constData(), totalFrames, mono);

    // Overlapped framing and feed to detection function
    std::vector<double> window(windowSize, 0.0);
    std::vector<double> dfValues;
    dfValues.reserve(static_cast<size_t>(totalFrames / stepSizeFrames) + 8);

    // Center first frame in window (as Mixxx does)
    int writePos = windowSize / 2;
    int read = 0;
    while (read < totalFrames) {
        const int writeAvail = windowSize - writePos;
        const int readAvail = totalFrames - read;
        const int n = std::min(writeAvail, readAvail);
        if (n > 0) {
            std::copy(mono.begin() + read, mono.begin() + read + n, window.begin() + writePos);
            read += n;
            writePos += n;
        }
        if (writePos == windowSize) {
            // Process window
            dfValues.push_back(df.processTimeDomain(window.data()));
            // Slide by step size
            if (stepSizeFrames < windowSize) {
                std::copy(window.begin() + stepSizeFrames, window.end(), window.begin());
                writePos -= stepSizeFrames;
            } else {
                // No overlap
                writePos = 0;
            }
        }
    }
    // Finalize: append silence to flush last frames
    int framesToFill = windowSize - writePos;
    int minPad = std::max(framesToFill, windowSize / 2 - 1);
    int padWritten = 0;
    while (padWritten < minPad) {
        const int writeAvail = windowSize - writePos;
        const int n = std::min(writeAvail, minPad - padWritten);
        if (n > 0) {
            std::fill(window.begin() + writePos, window.begin() + writePos + n, 0.0);
            writePos += n;
            padWritten += n;
        }
        if (writePos == windowSize) {
            dfValues.push_back(df.processTimeDomain(window.data()));
            if (stepSizeFrames < windowSize) {
                std::copy(window.begin() + stepSizeFrames, window.end(), window.begin());
                writePos -= stepSizeFrames;
            } else {
                writePos = 0;
            }
        }
    }

    // Trim trailing zeros
    int nonZeroCount = static_cast<int>(dfValues.size());
    while (nonZeroCount > 0 && dfValues[nonZeroCount - 1] <= 0.0) {
        --nonZeroCount;
    }

    // Build df (skip first 2)
    const int required = std::max(0, nonZeroCount - 2);
    std::vector<double> dfSeries;
    dfSeries.reserve(required);
    for (int i = 2; i < nonZeroCount; ++i) {
        dfSeries.push_back(dfValues[i]);
    }

    // Tempo tracking
    TempoTrackV2 tt(sampleRate, stepSizeFrames);
    std::vector<double> beatPeriod(dfSeries.size(), 0.0);
    tt.calculateBeatPeriod(dfSeries, beatPeriod);

    std::vector<double> beats;
    tt.calculateBeats(dfSeries, beatPeriod, beats);

    if (!beats.empty()) {
        out.supportsBeatTracking = true;
        out.beatPositionsFrames.reserve(static_cast<int>(beats.size()));
        for (size_t i = 0; i < beats.size(); ++i) {
            const int framePos = static_cast<int>(beats[i] * stepSizeFrames + stepSizeFrames / 2);
            out.beatPositionsFrames.push_back(framePos);
        }
    }

    if (out.supportsBeatTracking && out.beatPositionsFrames.size() >= 2) {
        double total = 0.0;
        int count = 0;
        for (int i = 1; i < out.beatPositionsFrames.size(); ++i) {
            const int df = out.beatPositionsFrames[i] - out.beatPositionsFrames[i - 1];
            if (df > 0) {
                total += df;
                ++count;
            }
        }
        if (count > 0) {
            const double avgFrames = total / static_cast<double>(count);
            out.bpm = static_cast<float>(60.0 * static_cast<double>(sampleRate) / avgFrames);
        }
    }

    return out;
}