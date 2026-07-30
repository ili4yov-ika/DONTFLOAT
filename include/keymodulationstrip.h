#ifndef KEYMODULATIONSTRIP_H
#define KEYMODULATIONSTRIP_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>
#include "keyanalyzer.h"

class QLineEdit;

/**
 * Полоса полей тональности над пианороллом (Melodyne-style).
 * Одно поле на непрерывный регион тактов с одной тональностью;
 * позиция синхронизирована с горизонтальным viewport таймлайна.
 */
class KeyModulationStrip : public QWidget
{
    Q_OBJECT

public:
    explicit KeyModulationStrip(QWidget* parent = nullptr);

    void setRegions(const QVector<KeyAnalyzer::KeyRegion>& regions);
    void clearRegions();
    const QVector<KeyAnalyzer::KeyRegion>& regions() const { return m_regions; }

    void setTimelineSampleCount(qint64 samples);
    void setTimelineReferenceWidth(int widthPx);
    void setZoomLevel(float zoom);
    void setHorizontalOffset(float offset);

    /// Обновить текст региона (после выбора в меню).
    void setRegionKey(int regionIndex, const QString& keyName);

signals:
    /// ПКМ / клик по полю — показать меню выбора тональности.
    void fieldMenuRequested(int regionIndex, QWidget* anchor, const QPoint& localPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuildFields();
    void relayoutFields();
    float sampleToWidgetX(qint64 sample) const;

    QVector<KeyAnalyzer::KeyRegion> m_regions;
    QVector<QLineEdit*> m_fields;

    qint64 m_timelineSamples = 0;
    int m_referenceWidthPx = 0;
    float m_zoomLevel = 1.0f;
    float m_horizontalOffset = 0.0f;

    static constexpr int kFieldMinWidthPx = 56;
    static constexpr int kFieldMaxWidthPx = 132;
    static constexpr int kFieldGapPx = 2;
};

#endif // KEYMODULATIONSTRIP_H
