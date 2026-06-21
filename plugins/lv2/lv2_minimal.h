#ifndef DONTFLOAT_LV2_MINIMAL_H
#define DONTFLOAT_LV2_MINIMAL_H

#include <cstdint>

#if defined(_WIN32)
#define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
#define LV2_SYMBOL_EXPORT __attribute__((visibility("default")))
#endif

using LV2_Handle = void*;

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

#endif // DONTFLOAT_LV2_MINIMAL_H
