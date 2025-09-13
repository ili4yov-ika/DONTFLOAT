#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>
#include <QCoreApplication>
#include <QPalette>
#include <QColor>
#include "../include/mainwindow.h"
#include <QStyleFactory>

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

    // Устанавливаем тему Fusion с тёмной палитрой по умолчанию
    qApp->setStyle(QStyleFactory::create("Fusion"));
    
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Shadow, QColor(53, 53, 53));
    qApp->setPalette(darkPalette);

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
