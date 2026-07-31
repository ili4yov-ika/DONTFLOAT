// DONTFLOAT mini-DAW — CLAP host.
//
// Loads an audio file (default tests/midi/test_1.wav), instantiates the
// DONTFLOAT CLAP plugin for the compiled product, streams the whole file
// through its realtime process() in blocks, writes the processed output, and
// optionally hosts the plugin editor (--gui).

#include "clap_minimal.h"
#include "mini_daw_host.h"

#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
extern const clap_plugin_entry_t clap_entry;
}

namespace {

const void* hostGetExtension(const clap_host_t*, const char*)
{
    // Our host is Qt-based and runs its own event loop, so we do not need to
    // provide clap.timer-support; the plugin's timer registration fails
    // gracefully and the editor is pumped by the host QApplication loop.
    return nullptr;
}

// Feed the whole file into the plugin so its session captures the audio, then
// create the plugin editor via the clap.gui X11 extension, reparent it into a
// host window, and run the Qt event loop — exactly how a DAW hosts the plugin.
int runClapGuiHost(QApplication& app, const MiniDaw::LoadedAudio& a, const MiniDaw::Options& o)
{
    if (!clap_entry.init("dontfloat.clap")) {
        std::cerr << " [clap] clap_entry.init failed\n";
        return 1;
    }
    const auto* factory = static_cast<const clap_plugin_factory_t*>(
        clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) < 1) {
        std::cerr << " [clap] factory unavailable\n";
        return 1;
    }
    const clap_plugin_descriptor_t* descriptor = factory->get_plugin_descriptor(factory, 0);

    clap_host_t host {};
    host.clap_version = CLAP_VERSION_INIT;
    host.name = "DONTFLOAT mini-DAW";
    host.get_extension = hostGetExtension;

    const clap_plugin_t* plugin = factory->create_plugin(factory, &host, descriptor->id);
    if (!plugin || !plugin->init(plugin)) {
        std::cerr << " [clap] create/init failed\n";
        return 1;
    }

    const uint32_t block = static_cast<uint32_t>(o.blockSize);
    plugin->activate(plugin, double(a.sampleRate), 1, block);
    plugin->start_processing(plugin);

    // Stream the file so the plugin session captures the audio for the editor.
    std::vector<float> inL(block), inR(block), oL(block), oR(block);
    float* inPtr[] = { inL.data(), inR.data() };
    float* outPtr[] = { oL.data(), oR.data() };
    clap_audio_buffer_t input {};
    input.data32 = inPtr;
    input.channel_count = 2;
    clap_audio_buffer_t output {};
    output.data32 = outPtr;
    output.channel_count = 2;
    clap_process_t process {};
    process.audio_inputs = &input;
    process.audio_outputs = &output;
    for (long long pos = 0; pos < a.frames;) {
        const uint32_t n = static_cast<uint32_t>(std::min<long long>(block, a.frames - pos));
        for (uint32_t i = 0; i < n; ++i) {
            inL[i] = a.left[pos + i];
            inR[i] = a.right[pos + i];
        }
        process.frames_count = n;
        process.steady_time = pos;
        plugin->process(plugin, &process);
        pos += n;
    }
    plugin->stop_processing(plugin);
    std::printf(" [clap] session filled with %lld frames; opening editor…\n", a.frames);

    const auto* gui = static_cast<const clap_plugin_gui_t*>(
        plugin->get_extension(plugin, CLAP_EXT_GUI));
    if (!gui) {
        std::cerr << " [clap] gui extension unavailable\n";
        return 1;
    }
    const char* api = nullptr;
    bool floating = false;
    if (!gui->get_preferred_api(plugin, &api, &floating) || !api) {
        std::cerr << " [clap] no preferred GUI api (no display / unsupported platform)\n";
        return 1;
    }
    if (!gui->is_api_supported(plugin, api, false) || !gui->create(plugin, api, false)) {
        std::cerr << " [clap] gui create failed for api " << api << "\n";
        return 1;
    }
    uint32_t w = 960;
    uint32_t h = 640;
    gui->get_size(plugin, &w, &h);

    // Host window that the DAW would provide.
    auto* hostWindow = new QWidget;
    hostWindow->setWindowTitle(
        QStringLiteral("DONTFLOAT mini-DAW [CLAP host] — %1").arg(QString::fromUtf8(MiniDaw::productName())));
    hostWindow->setAttribute(Qt::WA_NativeWindow, true);
    hostWindow->resize(int(w), int(h));
    hostWindow->show();
    (void)hostWindow->winId();

    clap_window_t window {};
    window.api = api;
    window.x11 = static_cast<uintptr_t>(hostWindow->winId());
    if (!gui->set_parent(plugin, &window)) {
        std::cerr << " [clap] gui set_parent failed for api " << api << "\n";
    }
    gui->set_size(plugin, w, h);
    gui->show(plugin);
    std::printf(" [clap] editor embedded (api=%s, %ux%u). Close the window to exit.\n", api, w, h);

    const int rc = app.exec();

    gui->hide(plugin);
    gui->destroy(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    clap_entry.deinit();
    return rc;
}

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

    if (options.gui) {
        return runClapGuiHost(app, audio, options);
    }

    std::vector<float> outL, outR;
    if (runClapEngine(audio, options, outL, outR) != 0) {
        return 3;
    }

    MiniDaw::writeOutput(options, audio.sampleRate, outL, outR);
    MiniDaw::runOfflineAnalysis(audio);

    std::printf(" [done] CLAP mini-DAW finished: streamed test_1.wav through %s.\n",
                MiniDaw::productName());
    return 0;
}
