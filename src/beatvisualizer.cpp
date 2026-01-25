#include "../include/beatvisualizer.h"
#include "../include/bpmanalyzer.h"
#include <QDebug>
#include <QtMath>
#include <QPainter>
#include <QPixmap>
#include <QBrush>
#include <QPainterPath>
#include <QPolygonF>

// Минимальная реализация для тестирования компиляции
BeatVisualizer::AnalysisResult BeatVisualizer::analyzeBeats(const QVector<QVector<float>>& audioData,
                                                           int sampleRate,
                                                           const VisualizationSettings& settings)
{
    Q_UNUSED(sampleRate)
    Q_UNUSED(settings)

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
    Q_UNUSED(settings)

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
    Q_UNUSED(settings)

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
    Q_UNUSED(settings)

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

// ============================================================================
// Методы для отклонений долей (Beat Deviations)
// ============================================================================

QPixmap BeatVisualizer::createDiagonalPattern(float stripeWidth,
                                              float stripeSpacing,
                                              const QColor& color,
                                              bool isStretched)
{
    int patternSize = int(stripeWidth + stripeSpacing);
    if (patternSize < 1) patternSize = 7;

    QPixmap pattern(patternSize, patternSize);
    pattern.fill(Qt::transparent);

    QPainter patternPainter(&pattern);
    patternPainter.setRenderHint(QPainter::Antialiasing, true);
    patternPainter.setPen(Qt::NoPen);
    patternPainter.setBrush(color);

    if (isStretched) {
        // Диагональ вправо-вверх (обратный слэш \) для растянутых долей
        QPolygonF stripe;
        stripe << QPointF(0, patternSize)
               << QPointF(stripeWidth, patternSize)
               << QPointF(patternSize, stripeWidth)
               << QPointF(patternSize, 0)
               << QPointF(0, 0);
        patternPainter.drawPolygon(stripe);
    } else {
        // Диагональ влево-вверх (прямой слэш /) для сжатых долей
        QPolygonF stripe;
        stripe << QPointF(0, 0)
               << QPointF(stripeWidth, 0)
               << QPointF(patternSize, patternSize - stripeWidth)
               << QPointF(patternSize, patternSize)
               << QPointF(0, patternSize);
        patternPainter.drawPolygon(stripe);
    }

    patternPainter.end();
    return pattern;
}

void BeatVisualizer::drawBeatDeviations(QPainter& painter,
                                       const QVector<BPMAnalyzer::BeatInfo>& beats,
                                       const QRectF& rect,
                                       float bpm,
                                       int sampleRate,
                                       float samplesPerPixel,
                                       int startSample,
                                       const VisualizationSettings& settings,
                                       const BeatDeviationColors& colors)
{
    if (beats.isEmpty() || !settings.showBeatDeviations || bpm <= 0.0f) {
        return;
    }

    // Вычисляем ожидаемый интервал между битами на основе BPM
    float expectedBeatInterval = (60.0f * sampleRate) / bpm;

    // Рисуем метки отклонений для каждого бита
    for (int i = 1; i < beats.size(); ++i) {
        const auto& beat = beats[i];

        // Проверяем, есть ли значимое отклонение (больше порога от интервала)
        if (qAbs(beat.deviation) < settings.deviationThreshold) {
            continue;
        }

        // Определяем ожидаемую позицию бита на основе предыдущего
        float expectedPosition = float(beats[i-1].position) + expectedBeatInterval;
        float actualPosition = float(beat.position);

        // Вычисляем координаты для отрисовки
        float expectedX = float(expectedPosition - startSample) / samplesPerPixel;
        float actualX = float(actualPosition - startSample) / samplesPerPixel;

        // Проверяем видимость
        if (actualX < 0 || expectedX > rect.width()) {
            continue;
        }

        // Определяем, растянута ли доля (положительное отклонение) или сжата (отрицательное)
        bool isStretched = beat.deviation > 0;

        // Выбираем цвет в зависимости от типа отклонения и состояния выравнивания
        QColor baseColor;
        if (isStretched) {
            baseColor = settings.beatsAligned ?
                colors.stretchedAligned : colors.stretchedUnaligned;
        } else {
            baseColor = settings.beatsAligned ?
                colors.compressedAligned : colors.compressedUnaligned;
        }

        // Треугольник должен указывать на фактическую позицию (actualX)
        float baseX = expectedX;
        float apexX = actualX;
        float minX = qMin(baseX, apexX);
        float maxX = qMax(baseX, apexX);

        // Пропускаем, если полностью вне видимой области
        if (maxX < 0.0f || minX > rect.width()) {
            continue;
        }

        // Ограничиваем координаты видимой областью
        baseX = qBound(0.0f, baseX, float(rect.width()));
        apexX = qBound(0.0f, apexX, float(rect.width()));
        minX = qMin(baseX, apexX);
        maxX = qMax(baseX, apexX);
        float width = maxX - minX;

        if (width > 0.5f) {
                // Рисуем вертикальную линию на фактической позиции бита
                if (actualX >= 0 && actualX <= rect.width()) {
                    painter.setPen(QPen(colors.beatLineColor, 1.5f));
                    painter.drawLine(QPointF(rect.x() + actualX, rect.top()),
                                   QPointF(rect.x() + actualX, rect.bottom()));
                }

                // Рисуем диагональные линии с треугольным clipping
                float centerY = rect.height() * 0.5f;

                QPainterPath triangleClip;
                triangleClip.moveTo(rect.x() + baseX, rect.top());              // Верхняя точка основания
                triangleClip.lineTo(rect.x() + apexX, rect.top() + centerY);    // Вершина (факт. позиция)
                triangleClip.lineTo(rect.x() + baseX, rect.bottom());           // Нижняя точка основания
                triangleClip.closeSubpath();

                // Базовая заливка треугольника для подчёркивания формы
                painter.save();
                QColor fillColor = baseColor;
                fillColor.setAlpha(settings.deviationAlpha);
                painter.setPen(Qt::NoPen);
                painter.setBrush(fillColor);
                painter.drawPath(triangleClip);
                painter.restore();

                // Без дополнительных линий - только заливка и контур
                // Это позволяет явно увидеть форму треугольника

                // Добавляем бледный фильтр поверх треугольника для лучшей видимости волны
                painter.save();
                painter.setClipPath(triangleClip);
                QColor filterColor(30, 30, 30, 30);
                painter.fillRect(QRectF(rect.x() + minX, rect.top(), width, rect.height()), filterColor);
                painter.restore();

                // Контур треугольника для явной геометрии
                painter.save();
                QColor outlineColor = baseColor;
                outlineColor.setAlpha(qMin(255, settings.deviationAlpha + 80));
                painter.setPen(QPen(outlineColor, 1.0f));
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(triangleClip);
                painter.restore();
        }
    }
}

void BeatVisualizer::drawBeatWaveform(QPainter& painter,
                                     const QVector<float>& samples,
                                     const QVector<BPMAnalyzer::BeatInfo>& beats,
                                     const QRectF& rect,
                                     int sampleRate,
                                     float samplesPerPixel,
                                     int startSample,
                                     const VisualizationSettings& settings)
{
    if (samples.isEmpty() || beats.isEmpty() || !settings.showBeatWaveform) {
        return;
    }

    int endSample = qMin(samples.size(), startSample + int(rect.width() * samplesPerPixel));

    // Вычисляем энергетическую огибающую для видимой области
    const int energyWindowSize = qRound(0.02f * sampleRate); // Окно 20ms
    QVector<float> energyEnvelope;
    energyEnvelope.reserve(endSample - startSample);

    for (int s = startSample; s < endSample; ++s) {
        int localStart = qMax(startSample, s - energyWindowSize / 2);
        int localEnd = qMin(endSample, s + energyWindowSize / 2);

        // Быстрое вычисление энергии без полного RMS
        float energy = 0.0f;
        int count = 0;
        for (int i = localStart; i < localEnd; i += 4) { // Пропускаем каждый 4-й сэмпл для скорости
            if (i < samples.size()) {
                energy += samples[i] * samples[i];
                count++;
            }
        }
        if (count > 0) {
            energy = std::sqrt(energy / count);
        }
        energyEnvelope.append(energy);
    }

    // Создаем вектор для хранения энергии ударных для каждого пикселя
    QVector<float> beatEnvelope(int(rect.width()), 0.0f);
    const int beatWindowSize = qRound(0.03f * sampleRate); // 30ms
    float maxEnergy = 0.0f;

    // Находим максимальную энергию для нормализации
    for (float e : energyEnvelope) {
        maxEnergy = qMax(maxEnergy, e);
    }

    if (maxEnergy <= 0.0f) return; // Нет энергии - нечего рисовать

    // Обрабатываем только биты в видимой области
    for (const auto& beat : beats) {
        qint64 beatPos = beat.position;

        // Пропускаем биты далеко вне видимой области
        if (beatPos < startSample - beatWindowSize || beatPos > endSample + beatWindowSize) {
            continue;
        }

        // Вычисляем область влияния бита в пикселях
        float beatX = (beatPos - startSample) / samplesPerPixel;

        if (beatX < -beatWindowSize / samplesPerPixel || beatX > rect.width() + beatWindowSize / samplesPerPixel) {
            continue;
        }

        // Вычисляем диапазон влияния в пикселях
        int pixelStart = qMax(0, int(beatX - beatWindowSize / samplesPerPixel));
        int pixelEnd = qMin(int(rect.width()), int(beatX + beatWindowSize / samplesPerPixel));

        // Применяем усиление энергии в области бита
        for (int x = pixelStart; x < pixelEnd; ++x) {
            int envelopeIndex = int(x * samplesPerPixel);
            if (envelopeIndex >= 0 && envelopeIndex < energyEnvelope.size()) {
                float energy = energyEnvelope[envelopeIndex];

                // Вычисляем реальный индекс сэмпла для вычисления расстояния
                int actualSampleIndex = startSample + envelopeIndex;

                // Усиливаем энергию в зависимости от близости к биту
                float distance = qAbs((beatPos - actualSampleIndex) / float(sampleRate)); // расстояние в секундах
                float falloff = qMax(0.0f, 1.0f - distance * 20.0f); // затухание
                float enhancedEnergy = energy * (1.0f + beat.energy * beat.confidence * falloff);

                beatEnvelope[x] = qMax(beatEnvelope[x], enhancedEnergy);
            }
        }
    }

    // Нормализуем энергию
    if (maxEnergy > 0.0f) {
        float normalizationFactor = 1.0f / maxEnergy;
        for (int x = 0; x < beatEnvelope.size(); ++x) {
            beatEnvelope[x] *= normalizationFactor;
        }
    }

    // Рисуем полигон силуэта
    painter.setRenderHint(QPainter::Antialiasing, false);

    QPolygonF beatPolygon;
    beatPolygon.reserve(int(rect.width()) * 2 + 2);

    float centerY = rect.center().y();
    float halfHeight = rect.height() * 0.5f;

    // Верхняя часть силуэта
    bool hasPoints = false;
    for (int x = 0; x < rect.width(); ++x) {
        if (beatEnvelope[x] > 0.001f) {
            float normalizedEnergy = qBound(0.0f, beatEnvelope[x], 1.0f);
            float y = centerY - (normalizedEnergy * halfHeight);
            beatPolygon.append(QPointF(rect.x() + x, y));
            hasPoints = true;
        } else if (hasPoints && x < rect.width() - 1 && beatEnvelope[x + 1] <= 0.001f) {
            beatPolygon.append(QPointF(rect.x() + x, centerY));
            hasPoints = false;
        }
    }

    // Переход к нижней части через центр
    if (!beatPolygon.isEmpty()) {
        beatPolygon.append(QPointF(rect.right(), centerY));
    }

    // Нижняя часть силуэта (в обратном порядке)
    hasPoints = false;
    for (int x = int(rect.width()) - 1; x >= 0; --x) {
        if (beatEnvelope[x] > 0.001f) {
            float normalizedEnergy = qBound(0.0f, beatEnvelope[x], 1.0f);
            float y = centerY + (normalizedEnergy * halfHeight);
            beatPolygon.append(QPointF(rect.x() + x, y));
            hasPoints = true;
        } else if (hasPoints && x > 0 && beatEnvelope[x - 1] <= 0.001f) {
            beatPolygon.append(QPointF(rect.x() + x, centerY));
            hasPoints = false;
        }
    }

    // Закрываем полигон
    if (!beatPolygon.isEmpty()) {
        beatPolygon.append(beatPolygon.first());
    }

    // Рисуем полигон с настраиваемой прозрачностью
    if (beatPolygon.size() >= 3) {
        int alpha = int(settings.beatWaveformOpacity * 200.0f);
        painter.setBrush(QBrush(QColor(255, 165, 0, qBound(0, alpha, 255))));
        painter.setPen(QPen(QColor(255, 165, 0, 200), 1));
        painter.drawPolygon(beatPolygon);
    }
}
