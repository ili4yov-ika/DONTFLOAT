#ifndef DONTFLOAT_MINI_DAW_HOST_H
#define DONTFLOAT_MINI_DAW_HOST_H

// Shared helpers for the DONTFLOAT "mini-DAW" hosts.
//
// A mini-DAW is a tiny in-process plugin host: it loads an audio file
// (default tests/midi/test_1.wav), instantiates the DONTFLOAT plugin for the
// compiled product (Full / Scratch / Pitcher), streams the whole file through
// the plugin's realtime process callback, writes the processed output to a WAV,
// and (with --gui) hosts the plugin editor with the loaded audio.
//
// The product is selected at compile time via DONTFLOAT_PLUGIN_PRODUCT_INDEX;
// the format-specific audio engine lives in each format's main .cpp.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QVector>

#include "audiofileservice.h"
#include "wavwriter.h"
#include "dontfloat_plugin_core.h"
#include "plugin_product.h"
#include "plugin_host_config.h"

#include <QtWidgets/QApplication>

namespace MiniDaw {

struct Options {
    QString input = QStringLiteral("tests/midi/test_1.wav");
    QString output;            // default derived from product + format
    int blockSize = 512;       // host processing block size (frames)
    double maxSeconds = 8.0;   // cap processed length (<= 0 => whole file)
    bool writeOutput = true;   // write the processed output WAV
};

inline const char* productName()
{
    return Dontfloat::PluginHost::desc().clapName;
}

inline Options parseArgs(int argc, char** argv, const char* formatTag)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&](const QString& def) -> QString {
            return (i + 1 < argc) ? QString::fromLocal8Bit(argv[++i]) : def;
        };
        if (a == QLatin1String("--input") || a == QLatin1String("-i")) {
            o.input = next(o.input);
        } else if (a == QLatin1String("--output") || a == QLatin1String("-o")) {
            o.output = next(o.output);
        } else if (a == QLatin1String("--block")) {
            o.blockSize = next(QStringLiteral("512")).toInt();
        } else if (a == QLatin1String("--seconds")) {
            o.maxSeconds = next(QStringLiteral("8")).toDouble();
        } else if (a == QLatin1String("--full")) {
            o.maxSeconds = 0.0;
        } else if (a == QLatin1String("--no-output")) {
            o.writeOutput = false;
        }
        // --headless is accepted for compatibility (this host is always headless).
    }
    if (o.output.isEmpty()) {
        o.output = QStringLiteral("mini_daw_%1_%2_out.wav")
                       .arg(QString::fromLatin1(formatTag).toLower())
                       .arg(QString::fromUtf8(Dontfloat::PluginHost::desc().clapFileBase));
    }
    if (o.blockSize < 16) {
        o.blockSize = 16;
    }
    return o;
}

struct LoadedAudio {
    std::vector<float> left;
    std::vector<float> right;
    int sampleRate = 0;
    long long frames = 0;
    bool ok = false;
    QString error;
};

inline LoadedAudio loadAudio(const QString& path, double maxSeconds)
{
    LoadedAudio a;
    const auto decoded = AudioFileService::decode(path);
    if (!decoded.ok || decoded.channels.isEmpty()) {
        a.error = decoded.error.isEmpty() ? QStringLiteral("decode failed") : decoded.error;
        return a;
    }
    a.sampleRate = decoded.sampleRate;
    const QVector<float>& L = decoded.channels.first();
    const QVector<float>& R = decoded.channels.size() > 1 ? decoded.channels[1] : decoded.channels.first();

    long long n = std::min<long long>(L.size(), R.size());
    if (maxSeconds > 0.0 && a.sampleRate > 0) {
        const long long cap = static_cast<long long>(maxSeconds * a.sampleRate);
        if (cap > 0) {
            n = std::min<long long>(n, cap);
        }
    }
    a.left.assign(L.begin(), L.begin() + n);
    a.right.assign(R.begin(), R.begin() + n);
    a.frames = n;
    a.ok = n > 0;
    if (!a.ok) {
        a.error = QStringLiteral("no audio frames after decode");
    }
    return a;
}

inline Dontfloat::PluginCore::TrackAudioBuffer makeBuffer(const LoadedAudio& a)
{
    Dontfloat::PluginCore::TrackAudioBuffer buf;
    buf.sampleRate = a.sampleRate;
    buf.channelCount = 2;
    buf.left = a.left;
    buf.right = a.right;
    buf.mono.resize(a.left.size());
    for (std::size_t i = 0; i < a.left.size(); ++i) {
        buf.mono[i] = 0.5f * (a.left[i] + a.right[i]);
    }
    return buf;
}

inline double rms(const std::vector<float>& v)
{
    if (v.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float x : v) {
        sum += double(x) * double(x);
    }
    return std::sqrt(sum / double(v.size()));
}

inline void printBanner(const char* format, const Options& o, const LoadedAudio& a)
{
    std::printf("=================================================\n");
    std::printf(" DONTFLOAT mini-DAW  [%s]\n", format);
    std::printf(" Product : %s (index %d)\n", productName(), DONTFLOAT_PLUGIN_PRODUCT_INDEX);
    std::printf(" Input   : %s\n", o.input.toLocal8Bit().constData());
    std::printf(" Audio   : %d Hz, %lld frames (%.2f s), block=%d\n",
                a.sampleRate, a.frames,
                a.sampleRate > 0 ? double(a.frames) / double(a.sampleRate) : 0.0,
                o.blockSize);
    std::printf("=================================================\n");
}

// Offline exercise of the shared plugin core session (prepare + analyze).
inline void runOfflineAnalysis(const LoadedAudio& a)
{
    using namespace Dontfloat::PluginCore;
    TrackToolSession session;
    if (session.setAudioBuffer(makeBuffer(a)) != TrackToolStatus::Ok) {
        std::printf(" [core] setAudioBuffer failed\n");
        return;
    }
    TrackAnalysisOptions opt;
    TrackAnalysisResult res;
    session.analyze(opt, &res);
    std::printf(" [core] prepared=%d frames=%lld analyze.status=%d bpm=%.2f chromaBins=%zu\n",
                int(session.isPrepared()),
                static_cast<long long>(session.audioBuffer().frameCount()),
                int(res.status), res.bpm, res.chroma.size());
}

inline int writeOutput(const Options& o, int sampleRate,
                       const std::vector<float>& L, const std::vector<float>& R)
{
    if (!o.writeOutput) {
        return 0;
    }
    QVector<QVector<float>> channels;
    QVector<float> qL;
    qL.reserve(int(L.size()));
    for (float x : L) {
        qL.append(x);
    }
    QVector<float> qR;
    qR.reserve(int(R.size()));
    for (float x : R) {
        qR.append(x);
    }
    channels.append(qL);
    channels.append(qR);

    QString err;
    if (!WavWriter::writeFile(o.output, channels, sampleRate, &err)) {
        std::printf(" [out] write failed: %s\n", err.toLocal8Bit().constData());
        return 1;
    }
    std::printf(" [out] wrote %s (%zu frames)\n", o.output.toLocal8Bit().constData(), L.size());
    return 0;
}

// Generic "core" audio engine: stream the file through the plugin core session
// via appendHostFrames (the same path the format wrappers use), output = input
// passthrough. Used by the VST3 mini-DAW when no Steinberg SDK is available.
inline int runCoreEngine(const LoadedAudio& a, const Options& o,
                         std::vector<float>& outL, std::vector<float>& outR)
{
    using namespace Dontfloat::PluginCore;
    TrackToolSession session;
    outL.resize(static_cast<std::size_t>(a.frames));
    outR.resize(static_cast<std::size_t>(a.frames));

    const int block = o.blockSize;
    long long pos = 0;
    long long processed = 0;
    while (pos < a.frames) {
        const int n = static_cast<int>(std::min<long long>(block, a.frames - pos));
        for (int i = 0; i < n; ++i) {
            outL[pos + i] = a.left[pos + i];
            outR[pos + i] = a.right[pos + i];
        }
        const float* inCh[2] = { a.left.data() + pos, a.right.data() + pos };
        session.appendHostFrames(inCh, 2, n);
        pos += n;
        processed += n;
    }
    std::printf(" [core] streamed %lld frames through session (in RMS %.4f)\n",
                processed, rms(a.left));
    return 0;
}

} // namespace MiniDaw

#endif // DONTFLOAT_MINI_DAW_HOST_H
