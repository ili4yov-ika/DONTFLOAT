#ifndef SVGICONLOADER_H
#define SVGICONLOADER_H

/**
 * Иконки из SVG-ресурсов без плагина движка иконок Qt.
 *
 * `QIcon(":/icons/....svg")` умеет рисовать SVG только через плагин
 * `iconengines/qsvgicon.dll`. Рядом с приложением он лежит, а вот у плагина
 * внутри DAW путь поиска плагинов Qt — чужой (каталог хоста), и кнопки
 * оказываются пустыми: без иконок остаются транспорт в шапке и панель разреза.
 * Поэтому SVG рисуется напрямую через `QSvgRenderer` из библиотеки Qt6Svg —
 * обычная DLL, она всегда лежит рядом с бинарником плагина.
 */

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>

namespace SvgIcons {

/** Пиксмап SVG-ресурса нужного размера с учётом плотности экрана. */
QPixmap renderPixmap(const QString& resourcePath, QSize size, qreal devicePixelRatio = 1.0);

/** Иконка квадратного размера \a sizePx (см. renderPixmap). */
QIcon load(const QString& resourcePath, int sizePx, qreal devicePixelRatio = 1.0);

} // namespace SvgIcons

#endif // SVGICONLOADER_H
