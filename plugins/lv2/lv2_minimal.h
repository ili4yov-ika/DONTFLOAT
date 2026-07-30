#ifndef DONTFLOAT_LV2_MINIMAL_H
#define DONTFLOAT_LV2_MINIMAL_H

#include <cstdint>

#if defined(_WIN32)
#define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
#define LV2_SYMBOL_EXPORT __attribute__((visibility("default")))
#endif

using LV2_Handle = void*;
using LV2UI_Handle = void*;
using LV2UI_Controller = void*;
using LV2UI_Widget = void*;

using LV2UI_Write_Function = void (*)(LV2UI_Controller controller,
                                      uint32_t port_index,
                                      uint32_t buffer_size,
                                      uint32_t port_protocol,
                                      const void* buffer);

struct LV2_Feature {
    const char* URI;
    void* data;
};

struct LV2_Descriptor {
    const char* URI;

    LV2_Handle (*instantiate)(const LV2_Descriptor* descriptor,
                              double sample_rate,
                              const char* bundle_path,
                              const LV2_Feature* const* features);

    void (*connect_port)(LV2_Handle instance, uint32_t port, void* data_location);
    void (*activate)(LV2_Handle instance);
    void (*run)(LV2_Handle instance, uint32_t sample_count);
    void (*deactivate)(LV2_Handle instance);
    void (*cleanup)(LV2_Handle instance);
    const void* (*extension_data)(const char* uri);
};

struct LV2UI_Descriptor {
    const char* URI;

    LV2UI_Handle (*instantiate)(const LV2UI_Descriptor* descriptor,
                                const char* plugin_uri,
                                const char* bundle_path,
                                LV2UI_Write_Function write_function,
                                LV2UI_Controller controller,
                                LV2UI_Widget* widget,
                                const LV2_Feature* const* features);

    void (*cleanup)(LV2UI_Handle ui);
    void (*port_event)(LV2UI_Handle ui,
                       uint32_t port_index,
                       uint32_t buffer_size,
                       uint32_t format,
                       const void* buffer);
    const void* (*extension_data)(const char* uri);
};

#define LV2_UI__parent "http://lv2plug.in/ns/extensions/ui#parent"
#define LV2_UI__idleInterface "http://lv2plug.in/ns/extensions/ui#idleInterface"
#define LV2_UI__resize "http://lv2plug.in/ns/extensions/ui#resize"

struct LV2UI_Idle_Interface {
    int (*idle)(LV2UI_Handle ui);
};

struct LV2UI_Resize {
    LV2UI_Controller handle;
    int (*ui_resize)(LV2UI_Controller handle, int width, int height);
};

#endif // DONTFLOAT_LV2_MINIMAL_H
