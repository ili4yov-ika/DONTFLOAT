// DONTFLOAT mini-DAW — LV2 host.
//
// Loads an audio file (default tests/midi/test_1.wav), instantiates the
// DONTFLOAT LV2 plugin for the compiled product, streams the whole file
// through its run() callback in blocks, writes the processed output, and
// optionally hosts the plugin editor (--gui).

#include "lv2_minimal.h"
#include "mini_daw_host.h"

#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
const LV2_Descriptor* lv2_descriptor(uint32_t index);
}

namespace {

// LV2 port layout of the DONTFLOAT plugin: 0=inL, 1=inR, 2=outL, 3=outR.
int runLv2Engine(const MiniDaw::LoadedAudio& a, const MiniDaw::Options& o,
                 std::vector<float>& outL, std::vector<float>& outR)
{
    const LV2_Descriptor* descriptor = lv2_descriptor(0);
    if (!descriptor) {
        std::cerr << " [lv2] lv2_descriptor(0) is null\n";
        return 1;
    }
    std::printf(" [lv2] plugin URI: %s\n", descriptor->URI);

    LV2_Handle instance = descriptor->instantiate(
        descriptor, double(a.sampleRate), "tests/midi/", nullptr);
    if (!instance) {
        std::cerr << " [lv2] instantiate failed\n";
        return 1;
    }

    const uint32_t block = static_cast<uint32_t>(o.blockSize);
    std::vector<float> inL(block), inR(block), oL(block), oR(block);
    descriptor->connect_port(instance, 0, inL.data());
    descriptor->connect_port(instance, 1, inR.data());
    descriptor->connect_port(instance, 2, oL.data());
    descriptor->connect_port(instance, 3, oR.data());
    descriptor->activate(instance);

    outL.resize(static_cast<std::size_t>(a.frames));
    outR.resize(static_cast<std::size_t>(a.frames));

    long long pos = 0;
    long long processed = 0;
    while (pos < a.frames) {
        const uint32_t n = static_cast<uint32_t>(std::min<long long>(block, a.frames - pos));
        for (uint32_t i = 0; i < n; ++i) {
            inL[i] = a.left[pos + i];
            inR[i] = a.right[pos + i];
        }
        descriptor->run(instance, n);
        for (uint32_t i = 0; i < n; ++i) {
            outL[pos + i] = oL[i];
            outR[pos + i] = oR[i];
        }
        pos += n;
        processed += n;
    }

    descriptor->deactivate(instance);
    descriptor->cleanup(instance);

    std::printf(" [lv2] processed %lld frames in blocks of %u (in RMS %.4f -> out RMS %.4f)\n",
                processed, block, MiniDaw::rms(a.left), MiniDaw::rms(outL));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const MiniDaw::Options options = MiniDaw::parseArgs(argc, argv, "lv2");

    const MiniDaw::LoadedAudio audio = MiniDaw::loadAudio(options.input, options.maxSeconds);
    MiniDaw::printBanner("LV2", options, audio);
    if (!audio.ok) {
        std::fprintf(stderr, "mini-DAW: failed to load %s: %s\n",
                     options.input.toLocal8Bit().constData(),
                     audio.error.toLocal8Bit().constData());
        return 2;
    }

    std::vector<float> outL, outR;
    if (runLv2Engine(audio, options, outL, outR) != 0) {
        return 3;
    }

    MiniDaw::writeOutput(options, audio.sampleRate, outL, outR);
    MiniDaw::runOfflineAnalysis(audio);

    if (options.gui) {
        return MiniDaw::runEditorGui(app, audio, "LV2");
    }
    std::printf(" [done] LV2 mini-DAW finished (headless). Use --gui to open the editor.\n");
    return 0;
}
