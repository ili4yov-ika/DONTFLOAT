// Иконки кнопок: SVG рисуется через QSvgRenderer, без плагина движка иконок
// Qt. Внутри DAW путь поиска плагинов Qt чужой, и раньше кнопки транспорта и
// панели разреза оставались пустыми.

#include <QtTest/QTest>
#include <QtGui/QImage>

#include "../include/svgiconloader.h"

namespace {

/** Доля непрозрачных пикселей — пустая иконка даёт 0. */
double opaqueRatio(const QPixmap& pixmap)
{
    const QImage image = pixmap.toImage();
    if (image.isNull()) {
        return 0.0;
    }
    int opaque = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                ++opaque;
            }
        }
    }
    return double(opaque) / double(image.width() * image.height());
}

} // namespace

class SvgIconTest : public QObject
{
    Q_OBJECT

private slots:
    void testToolbarAndTransportIconsAreDrawn_data();
    void testToolbarAndTransportIconsAreDrawn();
    void testDevicePixelRatioGivesSharperPixmap();
    void testMissingResourceDoesNotCrash();
};

void SvgIconTest::testToolbarAndTransportIconsAreDrawn_data()
{
    QTest::addColumn<QString>("resourcePath");
    const char* icons[] = {
        "trimmer", "along_the_grid", "free_cut",  // панель разреза
        "play", "stop", "metronome", "loop",      // транспорт шапки плагина
    };
    for (const char* name : icons) {
        QTest::newRow(name) << QStringLiteral(":/icons/resources/icons/%1.svg")
                                   .arg(QLatin1String(name));
    }
}

void SvgIconTest::testToolbarAndTransportIconsAreDrawn()
{
    QFETCH(QString, resourcePath);

    const QPixmap pixmap = SvgIcons::renderPixmap(resourcePath, QSize(24, 24));
    QVERIFY2(!pixmap.isNull(), qPrintable(resourcePath));
    QCOMPARE(pixmap.size(), QSize(24, 24));
    QVERIFY2(opaqueRatio(pixmap) > 0.01, qPrintable(resourcePath + QStringLiteral(": пусто")));

    // Через QIcon получается то же изображение нужного размера
    const QIcon icon = SvgIcons::load(resourcePath, 24);
    QVERIFY(!icon.isNull());
    QCOMPARE(icon.pixmap(QSize(24, 24)).size(), QSize(24, 24));
}

// Плотность экрана: пиксмап крупнее, а логический размер прежний
void SvgIconTest::testDevicePixelRatioGivesSharperPixmap()
{
    const QPixmap pixmap =
        SvgIcons::renderPixmap(QStringLiteral(":/icons/resources/icons/play.svg"), QSize(24, 24), 2.0);
    QVERIFY(!pixmap.isNull());
    QCOMPARE(pixmap.devicePixelRatio(), 2.0);
    QCOMPARE(pixmap.size(), QSize(48, 48));                     // фактические пиксели
    QCOMPARE(pixmap.deviceIndependentSize(), QSizeF(24, 24));   // на экране те же 24 px
}

// Несуществующий ресурс — пустая иконка, а не падение
void SvgIconTest::testMissingResourceDoesNotCrash()
{
    const QPixmap pixmap =
        SvgIcons::renderPixmap(QStringLiteral(":/icons/resources/icons/no_such_icon.svg"), QSize(24, 24));
    QVERIFY(pixmap.isNull() || opaqueRatio(pixmap) < 0.001);
}

QTEST_MAIN(SvgIconTest)
#include "svg_icon_test.moc"
