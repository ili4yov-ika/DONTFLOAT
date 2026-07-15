#include "../clap_minimal.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
extern const clap_plugin_entry_t clap_entry;
}

namespace {

bool isFinite(const std::vector<float>& values)
{
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

bool nearlyEqual(float a, float b)
{
    return std::abs(a - b) <= 1.0e-6f;
}

} // namespace

int main()
{
    if (!clap_entry.init("dontfloat_track_tool.clap")) {
        std::cerr << "clap_entry.init failed\n";
        return 1;
    }

    const auto* factory = static_cast<const clap_plugin_factory_t*>(
        clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) != 1) {
        std::cerr << "factory unavailable\n";
        return 1;
    }

    const clap_plugin_descriptor_t* descriptor = factory->get_plugin_descriptor(factory, 0);
    if (!descriptor || std::strcmp(descriptor->id, "com.dontfloat.track-tool") != 0
        || std::strcmp(descriptor->name, "DONTFLOAT Track Tool") != 0) {
        std::cerr << "descriptor mismatch\n";
        return 1;
    }

    clap_host_t host {};
    host.clap_version = CLAP_VERSION_INIT;
    host.name = "DONTFLOAT smoke host";

    const clap_plugin_t* plugin = factory->create_plugin(factory, &host, descriptor->id);
    if (!plugin || !plugin->init(plugin) || !plugin->activate(plugin, 44100.0, 64, 512)
        || !plugin->start_processing(plugin)) {
        std::cerr << "plugin lifecycle failed\n";
        return 1;
    }

    const auto* audioPorts = static_cast<const clap_plugin_audio_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
    if (!audioPorts || audioPorts->count(plugin, true) != 1 || audioPorts->count(plugin, false) != 1) {
        std::cerr << "audio ports extension unavailable\n";
        return 1;
    }

    const auto* gui = static_cast<const clap_plugin_gui_t*>(
        plugin->get_extension(plugin, CLAP_EXT_GUI));
    const char* preferredApi = nullptr;
    bool isFloating = true;
    if (!gui || !gui->get_preferred_api(plugin, &preferredApi, &isFloating)
        || std::strcmp(preferredApi, CLAP_WINDOW_API_WIN32) != 0 || isFloating) {
        std::cerr << "gui extension unavailable\n";
        return 1;
    }

    constexpr uint32_t frames = 128;
    std::vector<float> inL(frames);
    std::vector<float> inR(frames);
    std::vector<float> outL(frames, 0.0f);
    std::vector<float> outR(frames, 0.0f);
    for (uint32_t i = 0; i < frames; ++i) {
        inL[i] = std::sin(float(i) * 0.03f);
        inR[i] = std::cos(float(i) * 0.04f);
    }

    float* inputPtrs[] = {inL.data(), inR.data()};
    float* outputPtrs[] = {outL.data(), outR.data()};
    clap_audio_buffer_t input {};
    input.data32 = inputPtrs;
    input.channel_count = 2;
    clap_audio_buffer_t output {};
    output.data32 = outputPtrs;
    output.channel_count = 2;

    clap_process_t process {};
    process.frames_count = frames;
    process.audio_inputs = &input;
    process.audio_outputs = &output;

    const clap_process_status status = plugin->process(plugin, &process);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    clap_entry.deinit();

    if (status != CLAP_PROCESS_CONTINUE || !isFinite(outL) || !isFinite(outR)) {
        std::cerr << "process failed\n";
        return 1;
    }
    for (uint32_t i = 0; i < frames; ++i) {
        if (!nearlyEqual(outL[i], inL[i]) || !nearlyEqual(outR[i], inR[i])) {
            std::cerr << "passthrough mismatch\n";
            return 1;
        }
    }
    return 0;
}
