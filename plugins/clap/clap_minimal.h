#ifndef DONTFLOAT_CLAP_MINIMAL_H
#define DONTFLOAT_CLAP_MINIMAL_H

#include <cstdint>

#if defined(_WIN32)
#define CLAP_EXPORT __declspec(dllexport)
#else
#define CLAP_EXPORT __attribute__((visibility("default")))
#endif

#define CLAP_PLUGIN_FACTORY_ID "clap.plugin-factory"
#define CLAP_EXT_PARAMS "clap.params"
#define CLAP_EXT_AUDIO_PORTS "clap.audio-ports"
#define CLAP_EXT_GUI "clap.gui"
#define CLAP_EXT_TIMER_SUPPORT "clap.timer-support"
/**
 * Своё расширение хоста: перестановка каретки DAW из плагина. В CLAP нет
 * стандартного способа подвинуть транспорт хоста, поэтому мини-DAW отдаёт
 * это расширение, а чужие хосты просто вернут nullptr (плагин это переживёт).
 */
#define CLAP_EXT_DONTFLOAT_TRANSPORT "dontfloat.transport/1"
#define CLAP_PORT_STEREO "stereo"
#define CLAP_WINDOW_API_WIN32 "win32"
#define CLAP_WINDOW_API_COCOA "cocoa"
#define CLAP_WINDOW_API_X11 "x11"
#define CLAP_WINDOW_API_WAYLAND "wayland"

#define CLAP_CORE_EVENT_SPACE_ID 0
#define CLAP_EVENT_PARAM_VALUE 5

#define CLAP_PARAM_IS_AUTOMATABLE (1u << 0)
#define CLAP_AUDIO_PORT_IS_MAIN (1u << 0)

using clap_id = uint32_t;

struct clap_version_t {
    uint32_t major;
    uint32_t minor;
    uint32_t revision;
};

#define CLAP_VERSION_INIT {1, 2, 0}

struct clap_host_t;
struct clap_plugin_t;
struct clap_plugin_descriptor_t;
struct clap_plugin_factory_t;
struct clap_plugin_entry_t;
struct clap_process_t;
struct clap_input_events_t;
struct clap_output_events_t;

enum clap_process_status {
    CLAP_PROCESS_ERROR = 0,
    CLAP_PROCESS_CONTINUE = 1,
    CLAP_PROCESS_CONTINUE_IF_NOT_QUIET = 2,
    CLAP_PROCESS_TAIL = 3,
    CLAP_PROCESS_SLEEP = 4,
};

struct clap_event_header_t {
    uint32_t size;
    uint32_t time;
    uint16_t space_id;
    uint16_t type;
    uint32_t flags;
};

struct clap_event_param_value_t {
    clap_event_header_t header;
    clap_id param_id;
    void* cookie;
    int16_t note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    double value;
};

struct clap_audio_buffer_t {
    float** data32;
    double** data64;
    uint32_t channel_count;
    uint32_t latency;
    uint64_t constant_mask;
};

// Транспорт хоста (подмножество clap_event_transport_t из спецификации):
// нужен, чтобы каретка воспроизведения плагина шла синхронно с DAW.
using clap_beattime = int64_t;
using clap_sectime = int64_t;

enum : int64_t {
    CLAP_BEATTIME_FACTOR = 1LL << 31,
    CLAP_SECTIME_FACTOR = 1LL << 31,
};

enum : uint32_t {
    CLAP_TRANSPORT_HAS_TEMPO = 1u << 0,
    CLAP_TRANSPORT_HAS_BEATS_TIMELINE = 1u << 1,
    CLAP_TRANSPORT_HAS_SECONDS_TIMELINE = 1u << 2,
    CLAP_TRANSPORT_HAS_TIME_SIGNATURE = 1u << 3,
    CLAP_TRANSPORT_IS_PLAYING = 1u << 4,
    CLAP_TRANSPORT_IS_RECORDING = 1u << 5,
    CLAP_TRANSPORT_IS_LOOP_ACTIVE = 1u << 6,
    CLAP_TRANSPORT_IS_WITHIN_PRE_ROLL = 1u << 7,
};

struct clap_event_transport_t {
    clap_event_header_t header;

    uint32_t flags;

    clap_beattime song_pos_beats;   ///< позиция в тактовой сетке
    clap_sectime song_pos_seconds;  ///< позиция во времени

    double tempo;
    double tempo_inc;

    clap_beattime loop_start_beats;
    clap_beattime loop_end_beats;
    clap_sectime loop_start_seconds;
    clap_sectime loop_end_seconds;

    clap_beattime bar_start;
    int32_t bar_number;

    uint16_t tsig_num;
    uint16_t tsig_denom;
};

struct clap_process_t {
    int64_t steady_time;
    uint32_t frames_count;
    const clap_event_transport_t* transport;
    const clap_audio_buffer_t* audio_inputs;
    clap_audio_buffer_t* audio_outputs;
    const clap_input_events_t* in_events;
    const clap_output_events_t* out_events;
};

struct clap_input_events_t {
    void* ctx;
    uint32_t (*size)(const clap_input_events_t* list);
    const clap_event_header_t* (*get)(const clap_input_events_t* list, uint32_t index);
};

struct clap_output_events_t {
    void* ctx;
    bool (*try_push)(const clap_output_events_t* list, const clap_event_header_t* event);
};

struct clap_host_t {
    clap_version_t clap_version;
    void* host_data;
    const char* name;
    const char* vendor;
    const char* url;
    const char* version;
    const void* (*get_extension)(const clap_host_t* host, const char* extension_id);
    void (*request_restart)(const clap_host_t* host);
    void (*request_process)(const clap_host_t* host);
    void (*request_callback)(const clap_host_t* host);
};

struct clap_plugin_descriptor_t {
    clap_version_t clap_version;
    const char* id;
    const char* name;
    const char* vendor;
    const char* url;
    const char* manual_url;
    const char* support_url;
    const char* version;
    const char* description;
    const char* const* features;
};

struct clap_plugin_t {
    const clap_plugin_descriptor_t* desc;
    void* plugin_data;
    bool (*init)(const clap_plugin_t* plugin);
    void (*destroy)(const clap_plugin_t* plugin);
    bool (*activate)(const clap_plugin_t* plugin, double sample_rate,
                     uint32_t min_frames_count, uint32_t max_frames_count);
    void (*deactivate)(const clap_plugin_t* plugin);
    bool (*start_processing)(const clap_plugin_t* plugin);
    void (*stop_processing)(const clap_plugin_t* plugin);
    void (*reset)(const clap_plugin_t* plugin);
    clap_process_status (*process)(const clap_plugin_t* plugin, const clap_process_t* process);
    const void* (*get_extension)(const clap_plugin_t* plugin, const char* id);
    void (*on_main_thread)(const clap_plugin_t* plugin);
};

struct clap_plugin_factory_t {
    uint32_t (*get_plugin_count)(const clap_plugin_factory_t* factory);
    const clap_plugin_descriptor_t* (*get_plugin_descriptor)(const clap_plugin_factory_t* factory, uint32_t index);
    const clap_plugin_t* (*create_plugin)(const clap_plugin_factory_t* factory,
                                          const clap_host_t* host,
                                          const char* plugin_id);
};

struct clap_plugin_entry_t {
    clap_version_t clap_version;
    bool (*init)(const char* plugin_path);
    void (*deinit)();
    const void* (*get_factory)(const char* factory_id);
};

struct clap_param_info_t {
    clap_id id;
    uint32_t flags;
    void* cookie;
    char name[256];
    char module[1024];
    double min_value;
    double max_value;
    double default_value;
};

struct clap_plugin_params_t {
    uint32_t (*count)(const clap_plugin_t* plugin);
    bool (*get_info)(const clap_plugin_t* plugin, uint32_t param_index, clap_param_info_t* param_info);
    bool (*get_value)(const clap_plugin_t* plugin, clap_id param_id, double* out_value);
    bool (*value_to_text)(const clap_plugin_t* plugin, clap_id param_id, double value,
                          char* out_buffer, uint32_t out_buffer_capacity);
    bool (*text_to_value)(const clap_plugin_t* plugin, clap_id param_id, const char* param_value_text,
                          double* out_value);
    void (*flush)(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out);
};

struct clap_audio_port_info_t {
    clap_id id;
    char name[256];
    uint32_t flags;
    uint32_t channel_count;
    const char* port_type;
    clap_id in_place_pair;
};

struct clap_plugin_audio_ports_t {
    uint32_t (*count)(const clap_plugin_t* plugin, bool is_input);
    bool (*get)(const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_audio_port_info_t* info);
};

struct clap_window_t {
    const char* api;
    union {
        void* win32;
        void* cocoa;
        uintptr_t x11;
        void* ptr;
    };
};

// clap.timer-support — lets a plugin ask the host to call it back periodically.
// Used to pump the Qt event loop when the host is not Qt-based.
/** Расширение хоста DONTFLOAT: плагин просит переставить каретку DAW. */
struct clap_host_dontfloat_transport_t {
    /** @param seconds позиция от начала проекта. */
    void (*request_seek)(const clap_host_t* host, double seconds);
    /**
     * Плагин пересчитал звук (коррекция высот, растяжение): хосту стоит
     * прогнать дорожку заново, иначе правки не будут слышны.
     */
    void (*notify_output_changed)(const clap_host_t* host);
};

struct clap_host_timer_support_t {
    bool (*register_timer)(const clap_host_t* host, uint32_t period_ms, clap_id* timer_id);
    bool (*unregister_timer)(const clap_host_t* host, clap_id timer_id);
};

struct clap_plugin_timer_support_t {
    void (*on_timer)(const clap_plugin_t* plugin, clap_id timer_id);
};

/** Подсказки хосту о том, как можно менять размер окна редактора. */
struct clap_gui_resize_hints_t {
    bool can_resize_horizontally;
    bool can_resize_vertically;
    bool preserve_aspect_ratio;
    uint32_t aspect_ratio_width;
    uint32_t aspect_ratio_height;
};

struct clap_plugin_gui_t {
    bool (*is_api_supported)(const clap_plugin_t* plugin, const char* api, bool is_floating);
    bool (*get_preferred_api)(const clap_plugin_t* plugin, const char** api, bool* is_floating);
    bool (*create)(const clap_plugin_t* plugin, const char* api, bool is_floating);
    void (*destroy)(const clap_plugin_t* plugin);
    bool (*set_scale)(const clap_plugin_t* plugin, double scale);
    bool (*get_size)(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height);
    bool (*can_resize)(const clap_plugin_t* plugin);
    bool (*get_resize_hints)(const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints);
    bool (*adjust_size)(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height);
    bool (*set_size)(const clap_plugin_t* plugin, uint32_t width, uint32_t height);
    bool (*set_parent)(const clap_plugin_t* plugin, const clap_window_t* window);
    bool (*set_transient)(const clap_plugin_t* plugin, const clap_window_t* window);
    void (*suggest_title)(const clap_plugin_t* plugin, const char* title);
    bool (*show)(const clap_plugin_t* plugin);
    bool (*hide)(const clap_plugin_t* plugin);
};

#endif // DONTFLOAT_CLAP_MINIMAL_H
