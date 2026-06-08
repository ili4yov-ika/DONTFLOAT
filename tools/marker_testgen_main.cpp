#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include "../include/markertestgenwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("marker_testgen"));
    QApplication::setOrganizationName(QStringLiteral("DONTFLOAT"));

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(53, 53, 53));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Base, QColor(25, 25, 25));
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::Button, QColor(53, 53, 53));
    dark.setColor(QPalette::ButtonText, Qt::white);
    dark.setColor(QPalette::Highlight, QColor(42, 130, 218));
    app.setPalette(dark);

    MarkerTestGenWindow window;
    window.show();
    return app.exec();
}
