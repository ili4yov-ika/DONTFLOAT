// DONTFLOAT mini-DAW — CLAP host.
//
// Loads an audio file (default tests/midi/test_1.wav), instantiates the
// DONTFLOAT CLAP plugin for the compiled product, streams the whole file
// through its realtime process() in blocks, writes the processed output, and
// optionally hosts the plugin editor (--gui).

#include "clap_minimal.h"
#include "mini_daw_host.h"

#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
extern const clap_plugin_entry_t clap_entry;
}

namespace {

// Instantiate the CLAP plugin and stream the file through process() in blocks.
int runClapEngine(const MiniDaw::LoadedAudio& a, const MiniDaw::Options& o,
                  std::vector<float>& outL, std::vector<float>& outR)
{
    if (!clap_entry.init("dontfloat.clap")) {
        std::cerr << " [clap] clap_entry.init failed\n";
        return 1;
    }

    const auto* factory = static_cast<const clap_plugin_factory_t*>(
        clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) < 1) {
        std::cerr << " [clap] factory unavailable\n";
        clap_entry.deinit();
        return 1;
    }

    const clap_plugin_descriptor_t* descriptor = factory->get_plugin_descriptor(factory, 0);
    if (!descriptor) {
        std::cerr << " [clap] descriptor unavailable\n";
        clap_entry.deinit();
        return 1;
    }
    std::printf(" [clap] plugin: %s (%s)\n", descriptor->name, descriptor->id);

    clap_host_t host {};
    host.clap_version = CLAP_VERSION_INIT;
    host.name = "DONTFLOAT mini-DAW";

    const clap_plugin_t* plugin = factory->create_plugin(factory, &host, descriptor->id);
    if (!plugin || !plugin->init(plugin)) {
        std::cerr << " [clap] create/init failed\n";
        clap_entry.deinit();
        return 1;
    }

    const uint32_t block = static_cast<uint32_t>(o.blockSize);
    if (!plugin->activate(plugin, double(a.sampleRate), 1, block)
        || !plugin->start_processing(plugin)) {
        std::cerr << " [clap] activate/start failed\n";
        plugin->destroy(plugin);
        clap_entry.deinit();
        return 1;
    }

    outL.resize(static_cast<std::size_t>(a.frames));
    outR.resize(static_cast<std::size_t>(a.frames));

    std::vector<float> inL(block), inR(block), oL(block), oR(block);
    float* inputPtrs[] = { inL.data(), inR.data() };
    float* outputPtrs[] = { oL.data(), oR.data() };

    clap_audio_buffer_t input {};
    input.data32 = inputPtrs;
    input.channel_count = 2;
    clap_audio_buffer_t output {};
    output.data32 = outputPtrs;
    output.channel_count = 2;

    clap_process_t process {};
    process.audio_inputs = &input;
    process.audio_outputs = &output;

    long long pos = 0;
    long long processed = 0;
    int status = CLAP_PROCESS_CONTINUE;
    while (pos < a.frames) {
        const uint32_t n = static_cast<uint32_t>(std::min<long long>(block, a.frames - pos));
        for (uint32_t i = 0; i < n; ++i) {
            inL[i] = a.left[pos + i];
            inR[i] = a.right[pos + i];
        }
        process.frames_count = n;
        process.steady_time = pos;
        status = plugin->process(plugin, &process);
        if (status == CLAP_PROCESS_ERROR) {
            std::cerr << " [clap] process error at frame " << pos << "\n";
            break;
        }
        for (uint32_t i = 0; i < n; ++i) {
            outL[pos + i] = oL[i];
            outR[pos + i] = oR[i];
        }
        pos += n;
        processed += n;
    }

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    clap_entry.deinit();

    std::printf(" [clap] processed %lld frames in blocks of %u (in RMS %.4f -> out RMS %.4f)\n",
                processed, block, MiniDaw::rms(a.left), MiniDaw::rms(outL));
    return status == CLAP_PROCESS_ERROR ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const MiniDaw::Options options = MiniDaw::parseArgs(argc, argv, "clap");

    const MiniDaw::LoadedAudio audio = MiniDaw::loadAudio(options.input, options.maxSeconds);
    MiniDaw::printBanner("CLAP", options, audio);
    if (!audio.ok) {
        std::fprintf(stderr, "mini-DAW: failed to load %s: %s\n",
                     options.input.toLocal8Bit().constData(),
                     audio.error.toLocal8Bit().constData());
        return 2;
    }

    std::vector<float> outL, outR;
    if (runClapEngine(audio, options, outL, outR) != 0) {
        return 3;
    }

    MiniDaw::writeOutput(options, audio.sampleRate, outL, outR);
    MiniDaw::runOfflineAnalysis(audio);

    if (options.gui) {
        return MiniDaw::runEditorGui(app, audio, "CLAP");
    }
    std::printf(" [done] CLAP mini-DAW finished (headless). Use --gui to open the editor.\n");
    return 0;
}
