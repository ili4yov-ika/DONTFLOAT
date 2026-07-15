#include "dontfloat_qt_hosting.h"

#include <QApplication>
#include <QCoreApplication>

namespace Dontfloat::Plugins::Ui {

void ensureQtApplication()
{
    if (QCoreApplication::instance()) {
        return;
    }

    static int argc = 1;
    static char appName[] = "DONTFLOATTrackToolPlugin";
    static char* argv[] = {appName, nullptr};
    new QApplication(argc, argv);
}

} // namespace Dontfloat::Plugins::Ui
