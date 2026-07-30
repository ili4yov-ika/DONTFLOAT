#ifndef DONTFLOAT_WINDOWS_PLUGIN_STUB_H
#define DONTFLOAT_WINDOWS_PLUGIN_STUB_H

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Loads sibling implementation DLL via LoadLibraryEx(ALTERED_SEARCH_PATH)
// so Qt/dependencies next to the impl are found even when the host used plain
// LoadLibrary on this stub module.
HMODULE dontfloatLoadSiblingImpl(const wchar_t* implFileName);
void dontfloatUnloadSiblingImpl(HMODULE module);

#endif

#endif
