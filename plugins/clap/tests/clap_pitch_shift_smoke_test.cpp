#include "../clap_minimal.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
extern const clap_plugin_entry_t clap_entry;
}

namespace {

struct EventList {
    clap_input_events_t iface {};
    std::vector<clap_event_param_value_t> events;
};

uint32_t eventListSize(const clap_input_events_t* list)
{
    const auto* self = static_cast<const EventList*>(list->ctx);
    return uint32_t(self->events.size());
}

const clap_event_header_t* eventListGet(const clap_input_events_t* list, uint32_t index)
{
    const auto* self = static_cast<const EventList*>(list->ctx);
    if (index >= self->events.size()) {
        return nullptr;
    }
    return &self->events[index].header;
}

clap_event_param_value_t makeParamEvent(clap_id id, double value)
{
    clap_event_param_value_t event {};
    event.header.size = sizeof(clap_event_param_value_t);
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = id;
    event.value = value;
    return event;
}

bool isFinite(const std::vector<float>& values)
{
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    if (!clap_entry.init("dontfloat_pitch_shift.clap")) {
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
    if (!descriptor || std::strcmp(descriptor->id, "com.dontfloat.pitch-shift") != 0) {
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

    const auto* params = static_cast<const clap_plugin_params_t*>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    const auto* audioPorts = static_cast<const clap_plugin_audio_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
    if (!params || !audioPorts || params->count(plugin) != 7
        || audioPorts->count(plugin, true) != 1 || audioPorts->count(plugin, false) != 1) {
        std::cerr << "extensions unavailable\n";
        return 1;
    }

    EventList eventList;
    eventList.iface.ctx = &eventList;
    eventList.iface.size = eventListSize;
    eventList.iface.get = eventListGet;
    eventList.events.push_back(makeParamEvent(1, 1.0));   // Enabled
    eventList.events.push_back(makeParamEvent(2, 7.0));   // Pitch semitones
    eventList.events.push_back(makeParamEvent(6, 0.75));  // Wet

    constexpr uint32_t frames = 256;
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
    process.in_events = &eventList.iface;

    const clap_process_status status = plugin->process(plugin, &process);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    clap_entry.deinit();

    if (status != CLAP_PROCESS_CONTINUE || !isFinite(outL) || !isFinite(outR)) {
        std::cerr << "process failed\n";
        return 1;
    }
    return 0;
}
