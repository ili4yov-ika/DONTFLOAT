#include "windows_plugin_stub.h"

#if defined(_WIN32)

#include <cstring>

namespace {

HMODULE stubModuleHandle()
{
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&stubModuleHandle),
                       &module);
    return module;
}

} // namespace

HMODULE dontfloatLoadSiblingImpl(const wchar_t* implFileName)
{
    if (!implFileName || !implFileName[0]) {
        return nullptr;
    }

    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(stubModuleHandle(), path, MAX_PATH) == 0) {
        return nullptr;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        return nullptr;
    }
    slash[1] = L'\0';
    if (wcslen(path) + wcslen(implFileName) >= MAX_PATH) {
        return nullptr;
    }
    wcscat_s(path, implFileName);

    return LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

void dontfloatUnloadSiblingImpl(HMODULE module)
{
    if (module) {
        FreeLibrary(module);
    }
}

#endif
