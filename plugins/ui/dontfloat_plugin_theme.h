#ifndef DONTFLOAT_PLUGIN_THEME_H
#define DONTFLOAT_PLUGIN_THEME_H

/**
 * Оформление интерфейса плагинов «как в основной программе».
 *
 * Главное окно DONTFLOAT рисуется стилем Fusion с тёмной палитрой
 * (`src/main.cpp`). В плагине трогать `qApp` можно не всегда: если
 * `QApplication` создал хост (мини-DAW, любая Qt-DAW), глобальные стиль и
 * палитра перекрасили бы его собственный интерфейс. Поэтому:
 *
 * - `applyDontfloatAppTheme()` зовём только для «своего» `QApplication`
 *   (плагин поднял Qt сам — см. `ensureQtApplication`);
 * - `applyDontfloatWidgetTheme()` красит только поддерево редактора: палитра
 *   наследуется потомками, стиль Fusion проставляется рекурсивно.
 */

#include <QtCore/QString>
#include <QtGui/QPalette>

QT_BEGIN_NAMESPACE
class QApplication;
class QWidget;
QT_END_NAMESPACE

namespace Dontfloat::Plugins::Ui {

/** Тёмная палитра главного окна DONTFLOAT. */
QPalette dontfloatDarkPalette();

/** Лист стилей чрома плагина (шапка, кнопки транспорта, статусбар). */
QString dontfloatPluginStyleSheet();

/** Fusion + тёмная палитра для приложения, которое создали мы сами. */
void applyDontfloatAppTheme(QApplication* app);

/** Fusion + палитра + стили для поддерева виджета (хост не затрагивается). */
void applyDontfloatWidgetTheme(QWidget* root);

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_PLUGIN_THEME_H
