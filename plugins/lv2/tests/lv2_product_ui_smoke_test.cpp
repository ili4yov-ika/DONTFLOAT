#include "../lv2_minimal.h"
#include "../../core/plugin_host_config.h"

#include <cstring>
#include <iostream>

extern "C" {
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index);
}

int main()
{
    const char* expectedUri = Dontfloat::PluginHost::desc().lv2UiUri;
    const LV2UI_Descriptor* descriptor = lv2ui_descriptor(0);
    if (!descriptor || std::strcmp(descriptor->URI, expectedUri) != 0) {
        std::cerr << "ui descriptor mismatch\n";
        return 1;
    }
    if (lv2ui_descriptor(1) != nullptr) {
        std::cerr << "unexpected second ui descriptor\n";
        return 1;
    }
    if (!descriptor->instantiate || !descriptor->cleanup || !descriptor->port_event) {
        std::cerr << "ui descriptor callbacks incomplete\n";
        return 1;
    }
    return 0;
}
