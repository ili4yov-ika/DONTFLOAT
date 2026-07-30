#include "dontfloat_qt_hosting.h"

#include <QApplication>
#include <QCoreApplication>
#include <cstring>

namespace Dontfloat::Plugins::Ui {

void ensureQtApplication(const char* applicationName)
{
    if (QCoreApplication::instance()) {
        if (applicationName && applicationName[0] != '\0') {
            QCoreApplication::setApplicationName(QString::fromUtf8(applicationName));
        }
        return;
    }

    static int argc = 1;
    static char appNameStorage[96] = "DONTFLOAT";
    if (applicationName && applicationName[0] != '\0') {
        std::strncpy(appNameStorage, applicationName, sizeof(appNameStorage) - 1);
        appNameStorage[sizeof(appNameStorage) - 1] = '\0';
    }
    static char* argv[] = {appNameStorage, nullptr};
    auto* app = new QApplication(argc, argv);
    app->setApplicationName(QString::fromUtf8(appNameStorage));
    app->setOrganizationName(QStringLiteral("DONTFLOAT"));
}

} // namespace Dontfloat::Plugins::Ui
