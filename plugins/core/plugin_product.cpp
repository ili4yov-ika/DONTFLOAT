#include "plugin_product.h"

namespace Dontfloat::PluginCore {
namespace {

TrackToolSession g_sessions[kPluginProductCount];

const PluginProductDesc kProductDescs[] = {
    {
        PluginProduct::Full,
        "com.dontfloat.full",
        "DONTFLOAT",
        "Full DONTFLOAT track tool inside your DAW",
        "dontfloat",
        "https://github.com/ili4yov-ika/DONTFLOAT/plugins/full",
        "https://github.com/ili4yov-ika/DONTFLOAT/plugins/full#ui",
        "dontfloat.lv2",
        "dontfloat.ttl",
        "dontfloat",
        "dontfloat_ui",
        "DONTFLOAT.vst3",
        "DONTFLOAT",
    },
    {
        PluginProduct::Scratch,
        "com.dontfloat.scratch",
        "DONTFLOAT Scratch",
        "Beat grid and time stretch without pitch editing",
        "dontfloat_scratch",
        "https://github.com/ili4yov-ika/DONTFLOAT/plugins/scratch",
        "https://github.com/ili4yov-ika/DONTFLOAT/plugins/scratch#ui",
        "dontfloat_scratch.lv2",
        "dontfloat_scratch.ttl",
        "dontfloat_scratch",
        "dontfloat_scratch_ui",
        "DONTFLOAT Scratch.vst3",
        "DONTFLOAT Scratch",
    },
    {
        PluginProduct::Pitcher,
        "com.dontfloat.pitcher",
        "DONTFLOAT Pitcher",
        "Melodyne-like monophonic pitch editor",
        "dontfloat_pitcher",
        "https://github.com/ili4yov-ika/DONTFLOAT/plugins/pitcher",
        "https://github.com/ili4yov-ika/DONTFLOAT/plugins/pitcher#ui",
        "dontfloat_pitcher.lv2",
        "dontfloat_pitcher.ttl",
        "dontfloat_pitcher",
        "dontfloat_pitcher_ui",
        "DONTFLOAT Pitcher.vst3",
        "DONTFLOAT Pitcher",
    },
};

} // namespace

const PluginProductDesc& productDesc(PluginProduct product)
{
    const int index = static_cast<int>(product);
    if (index < 0 || index >= kPluginProductCount) {
        return kProductDescs[0];
    }
    return kProductDescs[index];
}

const PluginProductDesc& productDescByIndex(int index)
{
    if (index < 0 || index >= kPluginProductCount) {
        return kProductDescs[0];
    }
    return kProductDescs[index];
}

TrackToolSession& sharedSession(PluginProduct product)
{
    return g_sessions[static_cast<int>(product)];
}

TrackToolSession& sharedTrackToolSession()
{
    return sharedSession(PluginProduct::Full);
}

} // namespace Dontfloat::PluginCore
