#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>
#include <QCoreApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Установка информации о приложении для QSettings
    QCoreApplication::setOrganizationName("DONTFLOAT");
    QCoreApplication::setApplicationName("DONTFLOAT");
    QCoreApplication::setApplicationVersion("1.0.0");

    // Установка иконки приложения
    QIcon::setThemeName("DONTFLOAT");
    QIcon appIcon(":/icons/resources/icons/logo.svg");
    a.setWindowIcon(appIcon);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "dontfloat_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
