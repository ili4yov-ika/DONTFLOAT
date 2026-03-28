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
    // Быстрый выход если нечего рисовать
    if (beats.isEmpty() || !settings.showBeatDeviations || bpm <= 0.0f) {
        return;
    }

    // Вычисляем ожидаемый интервал между битами на основе BPM
    const float expectedBeatInterval = (60.0f * sampleRate) / bpm;
    const float deviationThreshold = settings.deviationThreshold;
    const qreal rectX = rect.x();
    const qreal rectWidth = rect.width();
    const qreal centerY = rect.center().y();

    // Треугольники на всю высоту виджета с небольшими отступами
    const qreal verticalMargin = rect.height() * 0.02;  // 2% отступ сверху и снизу
    const qreal topY = rect.top() + verticalMargin;
    const qreal botY = rect.bottom() - verticalMargin;

    // Включаем антиалиасинг один раз для всех треугольников
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Рисуем метки отклонений для каждого бита
    for (int i = 1; i < beats.size(); ++i) {
        const auto& beat = beats[i];

        // Проверяем, есть ли значимое отклонение (больше порога)
        if (qAbs(beat.deviation) < deviationThreshold) {
            continue;
        }

        // Используем ожидаемую позицию из анализатора (база для deviation), иначе — от предыдущего бита
        float expectedPosition;
        if (beat.expectedPosition != beat.position) {
            expectedPosition = float(beat.expectedPosition);
        } else {
            expectedPosition = float(beats[i - 1].position) + expectedBeatInterval;
        }
        float actualPosition = float(beat.position);

        // Вычисляем координаты для отрисовки
        const float expectedX = float(expectedPosition - startSample) / samplesPerPixel;
        const float actualX = float(actualPosition - startSample) / samplesPerPixel;

        // Проверяем видимость: рисуем только когда обе точки внутри канала
        if (expectedX < 0.0f || expectedX > rectWidth || actualX < 0.0f || actualX > rectWidth) {
            continue;
        }

        // Определяем, растянута ли доля (положительное отклонение) или сжата (отрицательное)
        const bool isStretched = beat.deviation > 0;

        // Выбираем цвет в зависимости от типа отклонения и состояния выравнивания
        QColor baseColor;
        if (isStretched) {
            baseColor = settings.beatsAligned ?
                colors.stretchedAligned : colors.stretchedUnaligned;
        } else {
            baseColor = settings.beatsAligned ?
                colors.compressedAligned : colors.compressedUnaligned;
        }

        // Расстояние между фактической и ожидаемой позицией (в пикселях)
        const float rawDistance = expectedX - actualX;
        const float distance = qAbs(rawDistance);
        if (distance < 0.5f) {
            continue;
        }

        // Ограничиваем размер: макс = 100%, мин = 85% от макс
        const float maxWidthPx = float(DEVIATION_TRIANGLE_MAX_WIDTH_PX);
        const float minWidthPx = maxWidthPx * float(DEVIATION_TRIANGLE_MIN_WIDTH_RATIO);
        const float clampedDistance = qBound(minWidthPx, distance, maxWidthPx);
        const float direction = (rawDistance > 0.0f) ? 1.0f : -1.0f;
        const float apexOffset = direction * clampedDistance;

        // Абсолютные координаты в виджете (основание = фактическая позиция, вершина = со смещением)
        const qreal baseX_abs = rectX + qreal(actualX);
        const qreal apexX_abs = baseX_abs + qreal(apexOffset);

        // Треугольник: 3 вершины (основание вертикально, вершина по центру)
        QPolygonF triangle;
        triangle << QPointF(baseX_abs, topY)
                 << QPointF(apexX_abs, centerY)
                 << QPointF(baseX_abs, botY);

        // Рисуем треугольник (цвет уже задан в baseColor, подправляем только alpha при необходимости)
        if (baseColor.alpha() < 200) {
            baseColor.setAlpha(200);
        }
        painter.setBrush(baseColor);
        painter.setPen(QPen(baseColor, 1.5));
        painter.drawPolygon(triangle);
    }

    painter.restore();
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

    const int endSample = qMin(samples.size(), startSample + int(rect.width() * samplesPerPixel));
    const int beatWindowSize = qRound(0.03f * sampleRate); // 30ms
    const int width = int(rect.width());

    // ОПТИМИЗАЦИЯ: вместо вычисления энергии для каждого сэмпла,
    // вычисляем только для пикселей в области битов
    QVector<float> beatEnvelope(width, 0.0f);
    float maxEnergy = 0.0f;

    // Обрабатываем только биты в видимой области
    for (const auto& beat : beats) {
        const qint64 beatPos = beat.position;

        // Быстрый пропуск битов вне видимой области
        if (beatPos < startSample - beatWindowSize || beatPos > endSample + beatWindowSize) {
            continue;
        }

        // Вычисляем область влияния бита в пикселях
        const float beatX = float(beatPos - startSample) / samplesPerPixel;
        const float windowPixels = float(beatWindowSize) / samplesPerPixel;

        if (beatX < -windowPixels || beatX > width + windowPixels) {
            continue;
        }

        const int pixelStart = qMax(0, int(beatX - windowPixels));
        const int pixelEnd = qMin(width, int(beatX + windowPixels));

        // ОПТИМИЗАЦИЯ: упрощенное вычисление энергии по сэмплам
        const int sampleStart = qMax(startSample, int(beatPos - beatWindowSize));
        const int sampleEnd = qMin(endSample, int(beatPos + beatWindowSize));

        float energy = 0.0f;
        int sampleCount = 0;
        // Пропускаем каждый 8-й сэмпл для скорости
        for (int s = sampleStart; s < sampleEnd; s += 8) {
            if (s < samples.size()) {
                energy += samples[s] * samples[s];
                sampleCount++;
            }
        }

        if (sampleCount > 0) {
            energy = std::sqrt(energy / sampleCount) * beat.energy * beat.confidence;
            maxEnergy = qMax(maxEnergy, energy);

            // Применяем энергию к пикселям в области влияния
            const float beatEnergy = energy;
            for (int x = pixelStart; x < pixelEnd; ++x) {
                const float distFromBeat = qAbs(x - beatX) / windowPixels;
                const float falloff = qMax(0.0f, 1.0f - distFromBeat);
                beatEnvelope[x] = qMax(beatEnvelope[x], beatEnergy * falloff);
            }
        }
    }

    if (maxEnergy <= 0.0f) return;

    // Нормализуем энергию
    const float normalizationFactor = 1.0f / maxEnergy;
    for (int x = 0; x < width; ++x) {
        beatEnvelope[x] *= normalizationFactor;
    }

    // ОПТИМИЗАЦИЯ: рисуем упрощенный полигон (только видимые точки)
    painter.setRenderHint(QPainter::Antialiasing, false);

    QPolygonF beatPolygon;
    beatPolygon.reserve(width * 2 + 2);

    const float centerY = rect.center().y();
    const float halfHeight = rect.height() * 0.5f;
    const float rectX = rect.x();
    const float threshold = 0.001f;

    // Верхняя часть силуэта (только точки с энергией)
    for (int x = 0; x < width; ++x) {
        if (beatEnvelope[x] > threshold) {
            const float y = centerY - (beatEnvelope[x] * halfHeight);
            beatPolygon.append(QPointF(rectX + x, y));
        }
    }

    if (beatPolygon.isEmpty()) return;

    // Нижняя часть силуэта (в обратном порядке)
    for (int x = width - 1; x >= 0; --x) {
        if (beatEnvelope[x] > threshold) {
            const float y = centerY + (beatEnvelope[x] * halfHeight);
            beatPolygon.append(QPointF(rectX + x, y));
        }
    }

    // Рисуем полигон
    const int alpha = int(settings.beatWaveformOpacity * 200.0f);
    painter.setBrush(QBrush(QColor(255, 165, 0, qBound(0, alpha, 255))));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(beatPolygon);
}
