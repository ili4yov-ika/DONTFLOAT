// Thin VST3 module stub: no Qt imports. Forwards InitDll/ExitDll/GetPluginFactory
// to sibling DONTFLOAT*.impl.dll loaded with LOAD_WITH_ALTERED_SEARCH_PATH.

#include "../core/windows_plugin_stub.h"

#include <cstring>

#ifndef DONTFLOAT_VST3_IMPL_DLL
#error "DONTFLOAT_VST3_IMPL_DLL must be defined"
#endif

namespace {

HMODULE g_impl = nullptr;
using InitFn = bool (*)();
using ExitFn = bool (*)();
using FactoryFn = void* (*)();

InitFn g_init = nullptr;
ExitFn g_exit = nullptr;
FactoryFn g_factory = nullptr;
int g_initCount = 0;

bool ensureLoaded()
{
    if (g_factory) {
        return true;
    }
    g_impl = dontfloatLoadSiblingImpl(DONTFLOAT_VST3_IMPL_DLL);
    if (!g_impl) {
        return false;
    }
    g_init = reinterpret_cast<InitFn>(GetProcAddress(g_impl, "InitDll"));
    g_exit = reinterpret_cast<ExitFn>(GetProcAddress(g_impl, "ExitDll"));
    g_factory = reinterpret_cast<FactoryFn>(GetProcAddress(g_impl, "GetPluginFactory"));
    if (!g_factory) {
        dontfloatUnloadSiblingImpl(g_impl);
        g_impl = nullptr;
        g_init = nullptr;
        g_exit = nullptr;
        return false;
    }
    return true;
}

} // namespace

extern "C" {

__declspec(dllexport) bool InitDll()
{
    if (!ensureLoaded()) {
        return false;
    }
    if (g_initCount == 0 && g_init) {
        if (!g_init()) {
            return false;
        }
    }
    ++g_initCount;
    return true;
}

__declspec(dllexport) bool ExitDll()
{
    if (g_initCount <= 0) {
        return false;
    }
    --g_initCount;
    if (g_initCount == 0) {
        bool ok = true;
        if (g_exit) {
            ok = g_exit();
        }
        dontfloatUnloadSiblingImpl(g_impl);
        g_impl = nullptr;
        g_init = nullptr;
        g_exit = nullptr;
        g_factory = nullptr;
        return ok;
    }
    return true;
}

__declspec(dllexport) void* GetPluginFactory()
{
    if (!ensureLoaded() || !g_factory) {
        return nullptr;
    }
    return g_factory();
}

} // extern "C"
