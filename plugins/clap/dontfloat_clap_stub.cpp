// Thin CLAP entry stub: host loads this .clap (no Qt imports). The real plugin
// lives in a sibling *.impl.dll loaded with LOAD_WITH_ALTERED_SEARCH_PATH.

#include "../core/windows_plugin_stub.h"
#include "clap_minimal.h"

#include <cstring>

#ifndef DONTFLOAT_CLAP_IMPL_DLL
#error "DONTFLOAT_CLAP_IMPL_DLL must be defined (e.g. L\"dontfloat.impl.dll\")"
#endif

namespace {

HMODULE g_impl = nullptr;
const clap_plugin_entry_t* g_entry = nullptr;
int g_initCount = 0;

bool ensureLoaded()
{
    if (g_entry) {
        return true;
    }
    g_impl = dontfloatLoadSiblingImpl(DONTFLOAT_CLAP_IMPL_DLL);
    if (!g_impl) {
        return false;
    }
    g_entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(g_impl, "clap_entry"));
    if (!g_entry || !g_entry->init || !g_entry->get_factory) {
        dontfloatUnloadSiblingImpl(g_impl);
        g_impl = nullptr;
        g_entry = nullptr;
        return false;
    }
    return true;
}

bool stubInit(const char* pluginPath)
{
    if (!ensureLoaded()) {
        return false;
    }
    if (g_initCount == 0) {
        if (!g_entry->init(pluginPath)) {
            return false;
        }
    }
    ++g_initCount;
    return true;
}

void stubDeinit()
{
    if (!g_entry || g_initCount <= 0) {
        return;
    }
    --g_initCount;
    if (g_initCount == 0) {
        g_entry->deinit();
        dontfloatUnloadSiblingImpl(g_impl);
        g_impl = nullptr;
        g_entry = nullptr;
    }
}

const void* stubGetFactory(const char* factoryId)
{
    if (!ensureLoaded() || !g_entry) {
        return nullptr;
    }
    return g_entry->get_factory(factoryId);
}

} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    stubInit,
    stubDeinit,
    stubGetFactory,
};
