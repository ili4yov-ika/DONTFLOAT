#include "plugin_tester_window.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("dontfloat_plugin_tester"));
    QApplication::setOrganizationName(QStringLiteral("DONTFLOAT"));

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor(40, 42, 46));
    palette.setColor(QPalette::WindowText, QColor(230, 230, 230));
    palette.setColor(QPalette::Base, QColor(28, 30, 34));
    palette.setColor(QPalette::Text, QColor(230, 230, 230));
    palette.setColor(QPalette::Button, QColor(55, 58, 64));
    palette.setColor(QPalette::ButtonText, QColor(230, 230, 230));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    app.setPalette(palette);

    PluginTesterWindow window;
    window.show();
    return app.exec();
}
