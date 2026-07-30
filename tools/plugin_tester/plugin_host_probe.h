#ifndef DONTFLOAT_PLUGIN_HOST_PROBE_H
#define DONTFLOAT_PLUGIN_HOST_PROBE_H

#include <QString>
#include <QStringList>

namespace Dontfloat::PluginTester {

enum class PluginFormat {
    Clap = 0,
    Vst3 = 1,
    Lv2 = 2,
};

enum class PluginProduct {
    Full = 0,
    Scratch = 1,
    Pitcher = 2,
};

struct ProbeResult {
    bool ok = false;
    QString path;
    QString summary;
    QStringList details;
};

QString formatLabel(PluginFormat format);
QString productLabel(PluginProduct product);
QString defaultPluginPath(PluginFormat format, PluginProduct product);
QString resolvePluginPath(PluginFormat format, PluginProduct product, const QString& overridePath = QString());
ProbeResult probePlugin(PluginFormat format, PluginProduct product, const QString& overridePath = QString());

} // namespace Dontfloat::PluginTester

#endif
