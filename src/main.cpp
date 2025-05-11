#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include "UI/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QTranslator translator;
    const QString locale = QLocale::system().name(); // например, "ru_RU"

    const QString translationPath = QCoreApplication::applicationDirPath() + "/translations";
    if (translator.load("dontfloat_" + locale, translationPath)) {
        app.installTranslator(&translator);
    }

    MainWindow w;
    w.show();

    return app.exec();
}
