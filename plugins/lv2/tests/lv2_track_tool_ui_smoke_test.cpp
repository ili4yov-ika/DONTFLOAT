#include "../lv2_minimal.h"

#include <cstring>
#include <iostream>

extern "C" {
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index);
}

int main()
{
    const LV2UI_Descriptor* descriptor = lv2ui_descriptor(0);
    if (!descriptor || std::strcmp(descriptor->URI, "https://github.com/ili4yov-ika/DONTFLOAT/plugins/track-tool#ui") != 0) {
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
