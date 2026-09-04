#ifndef DONTFLOAT_TIMELINE_SCROLL_H
#define DONTFLOAT_TIMELINE_SCROLL_H

/**
 * Горизонтальная прокрутка таймлайна в окне плагина.
 *
 * И волна, и пианоролл живут в одних координатах: масштаб `zoom` и смещение
 * `offset` в долях от 0 до 1. Полоса прокрутки — целочисленная, поэтому обе
 * половины окна переводят одно в другое одинаково; счёт вынесен сюда, чтобы
 * они не разъехались.
 *
 * Формулы те же, что в главном окне (`MainWindow::updateHorizontalScrollBar`):
 * при масштабе 1 прокручивать нечего, дальше длина «ленты» растёт вместе с
 * масштабом.
 */

#include <QScrollBar>

#include <algorithm>
#include <cmath>

namespace Dontfloat::Plugins::Ui {

/** Условная длина таймлайна в единицах полосы прокрутки. */
inline constexpr int kTimelineScrollSpan = 1000;

/**
 * Подгоняет диапазон полосы под масштаб.
 * При zoom <= 1 полоса обнуляется: прокручивать нечего.
 */
inline void applyTimelineZoomToScrollBar(QScrollBar* bar, float zoom)
{
    if (!bar) {
        return;
    }
    if (zoom <= 1.0f) {
        const QSignalBlocker blocker(bar);
        bar->setMaximum(0);
        bar->setPageStep(kTimelineScrollSpan);
        bar->setValue(0);
        return;
    }

    const int maxValue = int(std::lround(double(zoom - 1.0f) * kTimelineScrollSpan));
    const int pageStep = int(std::lround(double(kTimelineScrollSpan) / double(zoom)));
    const QSignalBlocker blocker(bar);
    bar->setMaximum(maxValue);
    bar->setPageStep(std::max(1, pageStep));
    bar->setSingleStep(std::max(1, pageStep / 10));
    bar->setValue(std::clamp(bar->value(), 0, maxValue));
}

/** Ставит бегунок туда, куда уехал таймлайн, не рассылая сигнал обратно. */
inline void applyTimelineOffsetToScrollBar(QScrollBar* bar, float offset)
{
    if (!bar || bar->maximum() <= 0) {
        return;
    }
    const int value = int(std::lround(double(std::clamp(offset, 0.0f, 1.0f)) * bar->maximum()));
    const QSignalBlocker blocker(bar);
    bar->setValue(std::clamp(value, 0, bar->maximum()));
}

/** Смещение таймлайна (0..1) по положению бегунка. */
inline float timelineOffsetFromScrollBar(const QScrollBar* bar)
{
    if (!bar || bar->maximum() <= 0) {
        return 0.0f;
    }
    return std::clamp(float(bar->value()) / float(bar->maximum()), 0.0f, 1.0f);
}

} // namespace Dontfloat::Plugins::Ui

#endif // DONTFLOAT_TIMELINE_SCROLL_H
