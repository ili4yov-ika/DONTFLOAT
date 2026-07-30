// DONTFLOAT mini-DAW — VST3 host.
//
// Loads an audio file (default tests/midi/test_1.wav), routes it through the
// DONTFLOAT plugin core session for the compiled product, writes the output,
// and optionally hosts the plugin editor (--gui).
//
// NOTE: A realtime VST3 host requires the proprietary Steinberg VST3 SDK
// (set DONTFLOAT_VST3_SDK_ROOT). When the SDK is unavailable, the DONTFLOAT
// VST3 plugin binary itself cannot be built, so this VST3 mini-DAW streams the
// audio through the shared plugin core session (the same code path the VST3
// wrapper feeds via appendHostFrames) and hosts the product editor. Wire the
// SDK to exercise the full realtime VST3 module.

#include "mini_daw_host.h"

#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const MiniDaw::Options options = MiniDaw::parseArgs(argc, argv, "vst3");

    const MiniDaw::LoadedAudio audio = MiniDaw::loadAudio(options.input, options.maxSeconds);
    MiniDaw::printBanner("VST3", options, audio);
    if (!audio.ok) {
        std::fprintf(stderr, "mini-DAW: failed to load %s: %s\n",
                     options.input.toLocal8Bit().constData(),
                     audio.error.toLocal8Bit().constData());
        return 2;
    }

    std::printf(" [vst3] realtime VST3 hosting needs the Steinberg SDK; "
                "using the shared plugin core session engine.\n");

    std::vector<float> outL, outR;
    if (MiniDaw::runCoreEngine(audio, options, outL, outR) != 0) {
        return 3;
    }

    MiniDaw::writeOutput(options, audio.sampleRate, outL, outR);
    MiniDaw::runOfflineAnalysis(audio);

    if (options.gui) {
        return MiniDaw::runEditorGui(app, audio, "VST3");
    }
    std::printf(" [done] VST3 mini-DAW finished (headless). Use --gui to open the editor.\n");
    return 0;
}
