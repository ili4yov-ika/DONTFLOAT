#include "dontfloat_plugin_theme.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QWidget>

namespace Dontfloat::Plugins::Ui {
namespace {

/** Один экземпляр Fusion на процесс: стиль переживает все редакторы. */
QStyle* sharedFusionStyle()
{
    static QStyle* style = QStyleFactory::create(QStringLiteral("Fusion"));
    return style;
}

void applyStyleRecursively(QWidget* widget, QStyle* style)
{
    if (!widget || !style) {
        return;
    }
    widget->setStyle(style);
    const QList<QWidget*> children = widget->findChildren<QWidget*>();
    for (QWidget* child : children) {
        child->setStyle(style);
    }
}

} // namespace

QPalette dontfloatDarkPalette()
{
    // Значения повторяют палитру главного окна (src/main.cpp)
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(35, 35, 35));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    palette.setColor(QPalette::Shadow, QColor(53, 53, 53));
    return palette;
}

QString dontfloatPluginStyleSheet()
{
    // Кнопки транспорта намеренно без своих красок: с Fusion и тёмной
    // палитрой они выглядят ровно как в главном окне.
    return QStringLiteral(
        "#dontfloatPluginEditorShell { background: #353535; }"
        "#dontfloatPluginHeader {"
        "  background: #353535;"
        "  border-bottom: 1px solid #2a2a2a;"
        "}"
        "#dontfloatPluginHeader QLabel#productTitle {"
        "  color: #ffffff; font-size: 13px; font-weight: 700;"
        "}"
        "#dontfloatPluginStatus {"
        "  background: #353535;"
        "  border-top: 1px solid #2a2a2a;"
        "  color: #d6d6d6;"
        "  padding: 2px 6px;"
        "}"
        "QPushButton[dontfloatSlim=\"true\"] {"
        "  padding: 2px 10px; font-size: 11px;"
        "}");
}

void applyDontfloatAppTheme(QApplication* app)
{
    if (!app) {
        return;
    }
    if (QStyle* style = sharedFusionStyle()) {
        QApplication::setStyle(style);
    }
    QApplication::setPalette(dontfloatDarkPalette());
}

void applyDontfloatWidgetTheme(QWidget* root)
{
    if (!root) {
        return;
    }
    applyStyleRecursively(root, sharedFusionStyle());
    // Палитра наследуется потомками, отдельного обхода не нужно
    root->setPalette(dontfloatDarkPalette());
    root->setStyleSheet(dontfloatPluginStyleSheet());
    root->setAutoFillBackground(true);
}

} // namespace Dontfloat::Plugins::Ui
