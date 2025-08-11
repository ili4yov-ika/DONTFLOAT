#ifndef WAVEFORMCOLORS_H
#define WAVEFORMCOLORS_H

#include <QColor>

class WaveformColors {
public:
    WaveformColors();

    // Основные цвета сигнала
    const QColor& getLowColor() const { return m_lowColor; }
    const QColor& getMidColor() const { return m_midColor; }
    const QColor& getHighColor() const { return m_highColor; }

    // Цвета для отображения битов
    const QColor& getBeatColor() const { return m_beatColor; }
    const QColor& getBeatHighlightColor() const { return m_beatHighlightColor; }

    // Цвета интерфейса
    const QColor& getGridColor() const { return m_gridColor; }
    const QColor& getPlayPositionColor() const { return m_playPositionColor; }
    const QColor& getBackgroundColor() const { return m_backgroundColor; }
    const QColor& getMarkerTextColor() const { return m_markerTextColor; }
    const QColor& getMarkerBackgroundColor() const { return m_markerBackgroundColor; }
    const QColor& getBarLineColor() const { return m_barLineColor; }

    // Настройка цветовой схемы
    void setColorScheme(const QString& scheme);
    void setCustomColors(const QColor& low, const QColor& mid, const QColor& high);

private:
    void setupDefaultColors();
    void setupDarkColors();
    void setupLightColors();

    QColor m_lowColor;         // Цвет низких частот
    QColor m_midColor;         // Цвет средних частот
    QColor m_highColor;        // Цвет высоких частот
    QColor m_beatColor;        // Цвет битов
    QColor m_beatHighlightColor; // Цвет выделенных битов
    QColor m_gridColor;        // Цвет сетки
    QColor m_playPositionColor; // Цвет позиции воспроизведения
    QColor m_backgroundColor;  // Цвет фона
    QColor m_markerTextColor;  // Цвет текста маркеров
    QColor m_markerBackgroundColor; // Цвет фона маркеров
    QColor m_barLineColor;     // Цвет линий тактов
};

#endif // WAVEFORMCOLORS_H 