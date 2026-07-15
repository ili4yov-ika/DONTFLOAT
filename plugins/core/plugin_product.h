#ifndef DONTFLOAT_PLUGIN_PRODUCT_H
#define DONTFLOAT_PLUGIN_PRODUCT_H

#include "dontfloat_plugin_core.h"

namespace Dontfloat::PluginCore {

enum class PluginProduct : int {
    Full = 0,
    Scratch = 1,
    Pitcher = 2,
};

struct PluginProductDesc {
    PluginProduct product = PluginProduct::Full;
    const char* clapId = "";
    const char* clapName = "";
    const char* clapDescription = "";
    const char* clapFileBase = "";
    const char* lv2Uri = "";
    const char* lv2UiUri = "";
    const char* lv2BundleDir = "";
    const char* lv2PluginTtl = "";
    const char* lv2BinaryBase = "";
    const char* lv2UiBinaryBase = "";
    const char* vst3BundleName = "";
    const char* vst3DisplayName = "";
};

constexpr int kPluginProductCount = 3;

const PluginProductDesc& productDesc(PluginProduct product);
const PluginProductDesc& productDescByIndex(int index);

TrackToolSession& sharedSession(PluginProduct product);
/** Legacy alias — сессия продукта DONTFLOAT (Full). */
TrackToolSession& sharedTrackToolSession();

} // namespace Dontfloat::PluginCore

#endif // DONTFLOAT_PLUGIN_PRODUCT_H
