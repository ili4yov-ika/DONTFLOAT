#include "../include/waveformcolors.h"

WaveformColors::WaveformColors() {
    setupDefaultColors();
}

void WaveformColors::setupDefaultColors() {
    // Цвета по умолчанию (тёмная тема)
    m_lowColor = QColor(255, 50, 50);      // Красный для низких частот
    m_midColor = QColor(50, 255, 50);      // Зеленый для средних частот
    m_highColor = QColor(50, 50, 255);     // Синий для высоких частот
    
    m_beatColor = QColor(255, 255, 0);     // Желтый для битов
    m_beatHighlightColor = QColor(255, 255, 255); // Белый для выделенных битов
    
    m_gridColor = QColor(128, 128, 128);   // Серый для сетки
    m_playPositionColor = QColor(255, 255, 255); // Белый для позиции воспроизведения
    m_backgroundColor = QColor(30, 30, 30); // Темно-серый фон
    m_markerTextColor = QColor(200, 200, 200); // Светло-серый для текста
    m_markerBackgroundColor = QColor(60, 60, 60); // Темно-серый для фона
    m_barLineColor = QColor(255, 255, 255, 200); // Полупрозрачный белый для линий тактов
}

void WaveformColors::setupDarkColors() {
    m_lowColor = QColor(255, 50, 50);      // Красный
    m_midColor = QColor(50, 255, 50);      // Зеленый
    m_highColor = QColor(50, 50, 255);     // Синий
    
    m_beatColor = QColor(255, 255, 0);     // Желтый
    m_beatHighlightColor = QColor(255, 255, 255); // Белый
    
    m_gridColor = QColor(128, 128, 128);   // Серый
    m_playPositionColor = QColor(255, 255, 255); // Белый
    m_backgroundColor = QColor(30, 30, 30); // Темно-серый
    m_markerTextColor = QColor(200, 200, 200); // Светло-серый
    m_markerBackgroundColor = QColor(60, 60, 60); // Темно-серый
    m_barLineColor = QColor(255, 255, 255, 200); // Полупрозрачный белый
}

void WaveformColors::setupLightColors() {
    m_lowColor = QColor(200, 0, 0);        // Темно-красный
    m_midColor = QColor(0, 200, 0);        // Темно-зеленый
    m_highColor = QColor(0, 0, 200);       // Темно-синий
    
    m_beatColor = QColor(180, 180, 0);     // Темно-желтый
    m_beatHighlightColor = QColor(0, 0, 0); // Черный
    
    m_gridColor = QColor(100, 100, 100);   // Темно-серый
    m_playPositionColor = QColor(0, 0, 0); // Черный
    m_backgroundColor = QColor(240, 240, 240); // Светло-серый
    m_markerTextColor = QColor(50, 50, 50); // Темно-серый
    m_markerBackgroundColor = QColor(220, 220, 220); // Светло-серый
    m_barLineColor = QColor(0, 0, 0, 200); // Полупрозрачный черный
}

void WaveformColors::setColorScheme(const QString& scheme) {
    if (scheme == "dark" || scheme == "default") {
        setupDarkColors();
    } else if (scheme == "light") {
        setupLightColors();
    } else {
        setupDefaultColors();
    }
}

void WaveformColors::setCustomColors(const QColor& low, const QColor& mid, const QColor& high) {
    m_lowColor = low;
    m_midColor = mid;
    m_highColor = high;
} 