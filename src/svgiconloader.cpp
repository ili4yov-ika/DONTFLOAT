#include "../include/svgiconloader.h"

#include <QtGui/QPainter>
#if defined(DONTFLOAT_HAS_QT_SVG)
#include <QtSvg/QSvgRenderer>
#endif

#include <cmath>

namespace SvgIcons {

QPixmap renderPixmap(const QString& resourcePath, QSize size, qreal devicePixelRatio)
{
    if (size.isEmpty()) {
        return {};
    }
    const qreal dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;

#if !defined(DONTFLOAT_HAS_QT_SVG)
    // Сборка без модуля Qt Svg: остаётся обычный QIcon (ему нужен плагин
    // движка иконок рядом с бинарником)
    return QIcon(resourcePath).pixmap(size, dpr);
#else
    QSvgRenderer renderer(resourcePath);
    if (!renderer.isValid()) {
        // Не SVG или ресурс не собрался — пусть попробует обычный QIcon
        return QIcon(resourcePath).pixmap(size, dpr);
    }

    QPixmap pixmap(QSize(int(std::lround(size.width() * dpr)),
                         int(std::lround(size.height() * dpr))));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(dpr);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(QPointF(0.0, 0.0), QSizeF(size)));
    painter.end();
    return pixmap;
#endif
}

QIcon load(const QString& resourcePath, int sizePx, qreal devicePixelRatio)
{
    return QIcon(renderPixmap(resourcePath, QSize(sizePx, sizePx), devicePixelRatio));
}

} // namespace SvgIcons
