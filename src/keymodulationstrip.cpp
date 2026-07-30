#include "../include/keymodulationstrip.h"
#include "../include/pianoroll_engine.h"

#include <cmath>

#include <QtGui/QContextMenuEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QLineEdit>

KeyModulationStrip::KeyModulationStrip(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("keyModulationStrip"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(25);
    setMaximumHeight(25);
    setStyleSheet(QStringLiteral(
        "QWidget#keyModulationStrip {"
        "  background-color: #1a1a1a;"
        "}"));
}

void KeyModulationStrip::setRegions(const QVector<KeyAnalyzer::KeyRegion>& regions)
{
    m_regions = regions;
    rebuildFields();
}

void KeyModulationStrip::clearRegions()
{
    m_regions.clear();
    rebuildFields();
}

void KeyModulationStrip::setTimelineSampleCount(qint64 samples)
{
    if (m_timelineSamples == samples) {
        return;
    }
    m_timelineSamples = samples;
    relayoutFields();
}

void KeyModulationStrip::setTimelineReferenceWidth(int widthPx)
{
    if (m_referenceWidthPx == widthPx) {
        return;
    }
    m_referenceWidthPx = widthPx;
    relayoutFields();
}

void KeyModulationStrip::setZoomLevel(float zoom)
{
    if (qFuzzyCompare(m_zoomLevel, zoom)) {
        return;
    }
    m_zoomLevel = zoom;
    relayoutFields();
}

void KeyModulationStrip::setHorizontalOffset(float offset)
{
    const float clamped = qBound(0.0f, offset, 1.0f);
    if (qFuzzyCompare(m_horizontalOffset, clamped)) {
        return;
    }
    m_horizontalOffset = clamped;
    relayoutFields();
}

void KeyModulationStrip::setRegionKey(int regionIndex, const QString& keyName)
{
    if (regionIndex < 0 || regionIndex >= m_regions.size()) {
        return;
    }
    m_regions[regionIndex].key.keyName = keyName;
    m_regions[regionIndex].key.key = KeyAnalyzer::stringToKey(keyName);
    if (regionIndex < m_fields.size() && m_fields[regionIndex]) {
        m_fields[regionIndex]->setText(keyName);
        m_fields[regionIndex]->setToolTip(keyName.isEmpty()
            ? tr("Undefined")
            : keyName);
    }
}

void KeyModulationStrip::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayoutFields();
}

bool KeyModulationStrip::eventFilter(QObject* watched, QEvent* event)
{
    auto* edit = qobject_cast<QLineEdit*>(watched);
    if (!edit) {
        return QWidget::eventFilter(watched, event);
    }

    const int index = m_fields.indexOf(edit);
    if (index < 0) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::RightButton || mouse->button() == Qt::LeftButton) {
            emit fieldMenuRequested(index, edit, mouse->pos());
            return true;
        }
    }
    if (event->type() == QEvent::ContextMenu) {
        auto* ctx = static_cast<QContextMenuEvent*>(event);
        emit fieldMenuRequested(index, edit, ctx->pos());
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void KeyModulationStrip::rebuildFields()
{
    for (QLineEdit* edit : m_fields) {
        if (edit) {
            edit->removeEventFilter(this);
            edit->deleteLater();
        }
    }
    m_fields.clear();

    const QString style = QStringLiteral(
        "QLineEdit {"
        "  background-color: #2b2b2b;"
        "  border: 1px solid #42a5f5;"
        "  border-radius: 2px;"
        "  padding: 1px 4px;"
        "  color: white;"
        "  font-size: 11px;"
        "}"
        "QLineEdit:hover {"
        "  border: 1px solid #64b5f6;"
        "}");

    m_fields.reserve(m_regions.size());
    for (const KeyAnalyzer::KeyRegion& region : m_regions) {
        auto* edit = new QLineEdit(this);
        edit->setReadOnly(true);
        edit->setFocusPolicy(Qt::ClickFocus);
        edit->setCursor(Qt::PointingHandCursor);
        edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        edit->setStyleSheet(style);
        edit->setText(region.key.keyName);
        edit->setPlaceholderText(tr("Undefined"));
        edit->setToolTip(region.key.keyName.isEmpty()
            ? tr("Key for bars %1–%2")
                  .arg(region.startBar + 1)
                  .arg(region.endBar + 1)
            : tr("%1 (bars %2–%3)")
                  .arg(region.key.keyName)
                  .arg(region.startBar + 1)
                  .arg(region.endBar + 1));
        edit->installEventFilter(this);
        edit->hide();
        m_fields.push_back(edit);
    }

    relayoutFields();
}

float KeyModulationStrip::sampleToWidgetX(qint64 sample) const
{
    const int refW = m_referenceWidthPx > 0 ? m_referenceWidthPx : qMax(1, width());
    const auto vp = PianoRollEngine::Viewport::compute(
        m_timelineSamples, refW, m_zoomLevel, m_horizontalOffset);
    const float timelineX = vp.sampleToPixelX(sample);
    if (refW <= 0 || width() <= 0) {
        return timelineX;
    }
    return timelineX * float(width()) / float(refW);
}

void KeyModulationStrip::relayoutFields()
{
    if (m_fields.isEmpty() || width() <= 0) {
        return;
    }

    for (int i = 0; i < m_fields.size(); ++i) {
        QLineEdit* edit = m_fields[i];
        if (!edit || i >= m_regions.size()) {
            continue;
        }

        const KeyAnalyzer::KeyRegion& region = m_regions[i];
        const float x0 = sampleToWidgetX(region.startSample);
        const float x1 = sampleToWidgetX(qMax(region.startSample + 1, region.endSample));

        // Компактное поле у начала региона (как подпись ключа в Melodyne).
        const int regionWidth = qMax(0, int(std::lround(x1 - x0)) - kFieldGapPx);
        const int fieldWidth = qBound(kFieldMinWidthPx, regionWidth > 0 ? regionWidth : kFieldMinWidthPx,
                                    kFieldMaxWidthPx);
        int left = int(std::lround(x0));
        int right = left + fieldWidth;

        if (right < 0 || left > width()) {
            edit->hide();
            continue;
        }

        left = qMax(0, left);
        right = qMin(width(), right);
        const int w = right - left;
        if (w < 20) {
            edit->hide();
            continue;
        }

        edit->setGeometry(left, 1, w, height() - 2);
        edit->show();
        edit->raise();
    }
}
