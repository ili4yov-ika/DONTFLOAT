#include "../include/beatvisualizer.h"
#include <QDebug>
#include <QtMath>
#include <QPainter>
#include <algorithm>
#include <numeric>
#include <cmath>

// Минимальная реализация для тестирования компиляции
BeatVisualizer::AnalysisResult BeatVisualizer::analyzeBeats(const QVector<QVector<float>>& audioData,
                                                           int sampleRate,
                                                           const VisualizationSettings& settings)
{
    AnalysisResult result;
    
    // Простая заглушка - создаем несколько тестовых маркеров
    if (!audioData.isEmpty() && !audioData[0].isEmpty()) {
        BeatMarker marker;
        marker.position = 0;
        marker.confidence = 0.8f;
        marker.energy = 0.7f;
        marker.frequency = 100.0f;
        marker.type = KickDrum;
        marker.color = QColor(255, 0, 0, 200); // Красный цвет
        marker.size = 1.0f;
        marker.isAccent = true;
        
        result.beats.append(marker);
    }
    
    return result;
}

void BeatVisualizer::drawBeatMarkers(QPainter& painter,
                                    const QVector<BeatMarker>& beats,
                                    const QRectF& rect,
                                    float samplesPerPixel,
                                    int startSample,
                                    const VisualizationSettings& settings)
{
    if (beats.isEmpty()) return;

    painter.setRenderHint(QPainter::Antialiasing);

    for (const auto& beat : beats) {
        float x = (beat.position - startSample) / samplesPerPixel;

        if (x >= 0 && x < rect.width()) {
            QColor color = beat.color;
            float size = beat.size * 10.0f;

            painter.setPen(Qt::NoPen);
            painter.setBrush(color);

            // Рисуем круглый маркер
            painter.drawEllipse(QPointF(rect.x() + x, rect.center().y()), size, size);
        }
    }
}

void BeatVisualizer::drawSpectrogram(QPainter& painter,
                                    const QVector<QVector<float>>& spectrogramData,
                                    const QRectF& rect,
                                    const VisualizationSettings& settings)
{
    // Простая заглушка для спектрограммы
    if (spectrogramData.isEmpty()) return;
    
    painter.setPen(QPen(QColor(100, 100, 100), 1));
    painter.drawRect(rect);
}

void BeatVisualizer::drawBeatEnergy(QPainter& painter,
                                   const QVector<BeatMarker>& beats,
                                   const QRectF& rect,
                                   float samplesPerPixel,
                                   int startSample,
                                   const VisualizationSettings& settings)
{
    if (beats.isEmpty()) return;

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(255, 165, 0, 180), 2.0f));

    QPointF prevPoint;
    bool firstPoint = true;

    for (const auto& beat : beats) {
        float x = (beat.position - startSample) / samplesPerPixel;
        if (x < 0 || x > rect.width()) continue;

        float y = rect.height() - (beat.energy * rect.height());
        QPointF currentPoint(rect.x() + x, rect.y() + y);

        if (!firstPoint) {
            painter.drawLine(prevPoint, currentPoint);
        }
        prevPoint = currentPoint;
        firstPoint = false;
    }
}
