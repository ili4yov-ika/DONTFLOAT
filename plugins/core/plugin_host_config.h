#ifndef DONTFLOAT_PLUGIN_HOST_CONFIG_H
#define DONTFLOAT_PLUGIN_HOST_CONFIG_H

#ifndef DONTFLOAT_PLUGIN_PRODUCT_INDEX
#error "Define DONTFLOAT_PLUGIN_PRODUCT_INDEX (0=Full, 1=Scratch, 2=Pitcher) when building a plugin target"
#endif

#include "../core/plugin_product.h"

namespace Dontfloat::PluginHost {

inline Dontfloat::PluginCore::PluginProduct product()
{
    return static_cast<Dontfloat::PluginCore::PluginProduct>(DONTFLOAT_PLUGIN_PRODUCT_INDEX);
}

inline const Dontfloat::PluginCore::PluginProductDesc& desc()
{
    return Dontfloat::PluginCore::productDescByIndex(DONTFLOAT_PLUGIN_PRODUCT_INDEX);
}

} // namespace Dontfloat::PluginHost

#endif // DONTFLOAT_PLUGIN_HOST_CONFIG_H
