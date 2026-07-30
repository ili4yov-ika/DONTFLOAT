#include "plugin_host_probe.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <cstdio>

using Dontfloat::PluginTester::PluginFormat;
using Dontfloat::PluginTester::PluginProduct;
using Dontfloat::PluginTester::ProbeResult;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    int failures = 0;
    int ran = 0;
    for (int f = 0; f < 3; ++f) {
        for (int p = 0; p < 3; ++p) {
            const auto format = static_cast<PluginFormat>(f);
            const auto product = static_cast<PluginProduct>(p);
            const QString path = Dontfloat::PluginTester::resolvePluginPath(format, product);
            if (!QFileInfo::exists(path)) {
                std::printf("SKIP %s / %s — missing %s\n",
                            qPrintable(Dontfloat::PluginTester::formatLabel(format)),
                            qPrintable(Dontfloat::PluginTester::productLabel(product)),
                            qPrintable(path));
                continue;
            }

            ++ran;
            const ProbeResult result =
                Dontfloat::PluginTester::probePlugin(format, product, path);
            std::printf("%s %s / %s — %s\n",
                        result.ok ? "PASS" : "FAIL",
                        qPrintable(Dontfloat::PluginTester::formatLabel(format)),
                        qPrintable(Dontfloat::PluginTester::productLabel(product)),
                        qPrintable(result.summary));
            if (!result.ok) {
                ++failures;
                for (const QString& line : result.details) {
                    std::printf("  %s\n", qPrintable(line));
                }
                std::printf("  path: %s\n", qPrintable(result.path));
            }
        }
    }

    if (ran == 0) {
        std::printf("No plugin modules found to probe\n");
        return 1;
    }
    return failures == 0 ? 0 : 1;
}
