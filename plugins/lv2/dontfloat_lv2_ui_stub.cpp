// Thin LV2 UI stub: no Qt imports. Forwards lv2ui_descriptor to sibling *.impl.dll.

#include "../core/windows_plugin_stub.h"
#include "lv2_minimal.h"

#ifndef DONTFLOAT_LV2_UI_IMPL_DLL
#error "DONTFLOAT_LV2_UI_IMPL_DLL must be defined"
#endif

namespace {

HMODULE g_impl = nullptr;
using DescriptorFn = const LV2UI_Descriptor* (*)(uint32_t);
DescriptorFn g_descriptor = nullptr;

bool ensureLoaded()
{
    if (g_descriptor) {
        return true;
    }
    g_impl = dontfloatLoadSiblingImpl(DONTFLOAT_LV2_UI_IMPL_DLL);
    if (!g_impl) {
        return false;
    }
    g_descriptor = reinterpret_cast<DescriptorFn>(GetProcAddress(g_impl, "lv2ui_descriptor"));
    if (!g_descriptor) {
        dontfloatUnloadSiblingImpl(g_impl);
        g_impl = nullptr;
        return false;
    }
    return true;
}

} // namespace

extern "C" LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
    if (!ensureLoaded() || !g_descriptor) {
        return nullptr;
    }
    return g_descriptor(index);
}
